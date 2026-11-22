/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! STM32F4 I2C (v1 controller), interrupt driven, as the stock firmware.

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/i2c/stm32.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/stm32/sleep.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/stm32f4_i2c.h>

#define MHZ_PER_HZ 1000ULL

#define I2C_NORMAL_MODE_CLOCK_SPEED_MAX 100000U

#define I2C_READ_WRITE_BIT 0x01

//! How long to wait for a transfer's stop condition: a byte and a stop at 100 kHz, with margin.
#define BUS_IDLE_TIMEOUT_US 500U
//! Half a clock period while clearing the bus by hand (50 kHz).
#define BUS_CLEAR_HALF_PERIOD_US 10U

#define CCR_STANDARD_MIN 4U

// SR2 << 16 | SR1
#define EVENT(sr2, sr1) (((uint32_t)(sr2) << 16) | (sr1))
#define EV_MASTER       (STM32F4_I2C_SR2_BUSY | STM32F4_I2C_SR2_MSL)
#define EV_MASTER_TX    (EV_MASTER | STM32F4_I2C_SR2_TRA)

// CR1 bits kept by prv_setup()
#define CR1_SETUP_KEEP \
  (0xffffU & ~(STM32F4_I2C_CR1_ACK | STM32F4_I2C_CR1_SMBTYPE | STM32F4_I2C_CR1_SMBUS))

#define SR1_ERRORS \
  (STM32F4_I2C_SR1_OVR | STM32F4_I2C_SR1_BERR | STM32F4_I2C_SR1_AF | STM32F4_I2C_SR1_ARLO)

//! Event expected on entry to each state.
static const uint32_t s_guard_events[] = {
  [PBL_I2C_STM32_START] = EVENT(EV_MASTER, STM32F4_I2C_SR1_SB),
  [PBL_I2C_STM32_ADDRESS_TX] = EVENT(EV_MASTER_TX, STM32F4_I2C_SR1_ADDR | STM32F4_I2C_SR1_TXE),
  [PBL_I2C_STM32_ADDRESS_RX] = EVENT(EV_MASTER, STM32F4_I2C_SR1_ADDR),
  [PBL_I2C_STM32_WRITE_DATA] = EVENT(EV_MASTER_TX, STM32F4_I2C_SR1_TXE),
  [PBL_I2C_STM32_END_WRITE] = EVENT(EV_MASTER_TX, STM32F4_I2C_SR1_TXE | STM32F4_I2C_SR1_BTF),
  [PBL_I2C_STM32_READ_DATA] = EVENT(EV_MASTER, STM32F4_I2C_SR1_RXNE),
};

static const struct pbl_i2c_stm32 *prv_i2c(const struct pbl_i2c_bus *bus) {
  return container_of(bus, const struct pbl_i2c_stm32, bus);
}

static inline uint32_t prv_getreg(const struct pbl_i2c_stm32 *i2c, uint32_t offset) {
  return pbl_readl(i2c->base + offset);
}

static inline void prv_putreg(const struct pbl_i2c_stm32 *i2c, uint32_t offset, uint32_t value) {
  pbl_writel(value, i2c->base + offset);
}

static inline void prv_setbits(const struct pbl_i2c_stm32 *i2c, uint32_t offset, uint32_t bits) {
  prv_putreg(i2c, offset, prv_getreg(i2c, offset) | bits);
}

static inline void prv_clrbits(const struct pbl_i2c_stm32 *i2c, uint32_t offset, uint32_t bits) {
  prv_putreg(i2c, offset, prv_getreg(i2c, offset) & ~bits);
}

//! Fast mode uses duty cycle 2 (divider 3); find a clock not above the target.
static uint32_t prv_clock_speed(uint32_t pclk, uint32_t desired_hz) {
  const uint32_t divider = (desired_hz > I2C_NORMAL_MODE_CLOCK_SPEED_MAX) ? 3U : 2U;
  uint32_t prescaler = pclk / (desired_hz * divider);

  if (pclk % (desired_hz * divider)) {
    prescaler++;
  }

  return pclk / (prescaler * divider);
}

static void prv_setup(const struct pbl_i2c_stm32 *i2c) {
  uint64_t rate_mhz = 0U;
  uint32_t ccr;
  uint32_t trise;

  (void)pbl_clk_get_rate(&i2c->clk, &rate_mhz);

  const uint32_t pclk = (uint32_t)(rate_mhz / MHZ_PER_HZ);
  const uint32_t speed = prv_clock_speed(pclk, i2c->clock_hz);
  const uint32_t freq_mhz = pclk / 1000000U;

  prv_putreg(i2c, STM32F4_I2C_CR2_OFFSET,
             (prv_getreg(i2c, STM32F4_I2C_CR2_OFFSET) & ~STM32F4_I2C_CR2_FREQ_MASK) | freq_mhz);

  prv_clrbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_PE);

  if (speed <= I2C_NORMAL_MODE_CLOCK_SPEED_MAX) {
    ccr = MAX(pclk / (speed * 2U), CCR_STANDARD_MIN);
    trise = freq_mhz + 1U;
  } else {
    ccr = MAX(pclk / (speed * 3U), 1U) | STM32F4_I2C_CCR_FS;
    trise = freq_mhz * 300U / 1000U + 1U;
  }
  prv_putreg(i2c, STM32F4_I2C_TRISE_OFFSET, trise);
  prv_putreg(i2c, STM32F4_I2C_CCR_OFFSET, ccr);

  prv_setbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_PE);
  prv_putreg(i2c, STM32F4_I2C_CR1_OFFSET,
             (prv_getreg(i2c, STM32F4_I2C_CR1_OFFSET) & CR1_SETUP_KEEP) | STM32F4_I2C_CR1_ACK);
  prv_putreg(i2c, STM32F4_I2C_OAR1_OFFSET, STM32F4_I2C_OAR1_BIT14);
  prv_setbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_PE);
}

static void prv_stop_block(const struct pbl_i2c_stm32 *i2c) {
  pbl_irq_lock();
  if (!i2c->state->stop_blocked) {
    i2c->state->stop_blocked = true;
    soc_stm32_stop_block();
  }
  pbl_irq_unlock();
}

static void prv_stop_release(const struct pbl_i2c_stm32 *i2c) {
  pbl_irq_lock();
  if (i2c->state->stop_blocked) {
    i2c->state->stop_blocked = false;
    soc_stm32_stop_release();
  }
  pbl_irq_unlock();
}

static void prv_disable_all_interrupts(const struct pbl_i2c_stm32 *i2c) {
  prv_clrbits(i2c, STM32F4_I2C_CR2_OFFSET,
              STM32F4_I2C_CR2_ITEVTEN | STM32F4_I2C_CR2_ITERREN | STM32F4_I2C_CR2_ITBUFEN);
}

// The pins are only handed to the peripheral while the bus is enabled, as the
// stock firmware does: on a switched rail they sit low while it is off, and an
// I2C seeing that can latch BUSY so that no reset clears it.
static void prv_pins_input(const struct pbl_i2c_stm32 *i2c) {
  pbl_gpio_configure(&i2c->scl, PBL_GPIO_INPUT);
  pbl_gpio_configure(&i2c->sda, PBL_GPIO_INPUT);
}

static bool prv_is_busy(const struct pbl_i2c_stm32 *i2c) {
  return (prv_getreg(i2c, STM32F4_I2C_SR2_OFFSET) & STM32F4_I2C_SR2_BUSY) != 0U;
}

static int prv_init(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);
  const struct pbl_device *deps[] = {
    &i2c->clk.ctrl->dev, &i2c->reset.ctrl->dev, &i2c->scl.port->dev, &i2c->sda.port->dev
  };
  int ret;

  for (size_t i = 0U; i < ARRAY_LENGTH(deps); i++) {
    ret = pbl_device_init(deps[i]);
    if (ret != 0) {
      return ret;
    }
  }

  prv_pins_input(i2c);

  ret = pbl_clk_enable(&i2c->clk);
  if (ret != 0) {
    return ret;
  }
  ret = pbl_reset_reset(&i2c->reset);
  (void)pbl_clk_disable(&i2c->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(i2c->ev_irq);
  pbl_irq_enable(i2c->er_irq);

  return 0;
}

static void prv_enable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);

  (void)pbl_pinctrl_select_state(&i2c->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  (void)pbl_clk_enable(&i2c->clk);
  prv_setup(i2c);
}

static void prv_disable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);

  // A transfer completes when its stop condition is requested, not when it is on the bus, and
  // after a late NACK the device sends one more byte first. Resetting the peripheral in the
  // middle of that leaves the device mid-byte, holding SDA low: wait for the bus to go idle.
  for (uint32_t us = 0U; us < BUS_IDLE_TIMEOUT_US && prv_is_busy(i2c); us++) {
    pbl_busy_wait_us(1);
  }

  (void)pbl_reset_reset(&i2c->reset);
  (void)pbl_clk_disable(&i2c->clk);
  prv_pins_input(i2c);
}

static void prv_reattach(const struct pbl_i2c_stm32 *i2c) {
  (void)pbl_pinctrl_select_state(&i2c->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  prv_setup(i2c);
}

//! Free a bus held by a device that is still sending: up to nine clocks let it finish its byte
//! and see a NACK, then a stop condition. The peripheral is reset and set up again after.
static void prv_bus_clear(const struct pbl_i2c_stm32 *i2c) {
  (void)pbl_reset_reset(&i2c->reset);
  pbl_gpio_configure(&i2c->sda, PBL_GPIO_INPUT);
  pbl_gpio_configure(&i2c->scl, PBL_GPIO_OUTPUT_HIGH | PBL_GPIO_OPEN_DRAIN);

  for (int i = 0; i < 9 && pbl_gpio_get_raw(&i2c->sda) == 0; i++) {
    pbl_gpio_set_raw(&i2c->scl, false);
    pbl_busy_wait_us(BUS_CLEAR_HALF_PERIOD_US);
    pbl_gpio_set_raw(&i2c->scl, true);
    pbl_busy_wait_us(BUS_CLEAR_HALF_PERIOD_US);
  }

  // Stop: SDA rises while SCL is high.
  pbl_gpio_set_raw(&i2c->scl, false);
  pbl_busy_wait_us(BUS_CLEAR_HALF_PERIOD_US);
  pbl_gpio_configure(&i2c->sda, PBL_GPIO_OUTPUT_LOW | PBL_GPIO_OPEN_DRAIN);
  pbl_busy_wait_us(BUS_CLEAR_HALF_PERIOD_US);
  pbl_gpio_set_raw(&i2c->scl, true);
  pbl_busy_wait_us(BUS_CLEAR_HALF_PERIOD_US);
  pbl_gpio_set_raw(&i2c->sda, true);
  pbl_busy_wait_us(BUS_CLEAR_HALF_PERIOD_US);

  prv_reattach(i2c);
}

static void prv_begin(const struct pbl_i2c_stm32 *i2c) {
  i2c->state->xfer_state = PBL_I2C_STM32_START;
  i2c->state->idx = 0;
  prv_setbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_ACK);
  prv_setbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_START);
}

static int prv_start(const struct pbl_i2c_bus *bus, uint16_t addr, struct pbl_i2c_msg *msgs,
                     uint8_t num_msgs) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);
  struct pbl_i2c_stm32_state *state = i2c->state;

  for (uint8_t i = 0; i < num_msgs; i++) {
    if (msgs[i].len == 0U) {
      return -ENOTSUP;
    }
  }

  // A glitch can latch BUSY with the bus idle; only a reset clears it.
  if (prv_is_busy(i2c)) {
    prv_setbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_SWRST);
    prv_clrbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_SWRST);
    prv_reattach(i2c);
    if (prv_is_busy(i2c)) {
      // Or a device was left mid-byte and holds SDA low: clock it out.
      prv_bus_clear(i2c);
      if (prv_is_busy(i2c)) {
        return -EBUSY;
      }
    }
  }

  state->msgs = msgs;
  state->num_msgs = num_msgs;
  state->msg = 0;
  state->addr = addr;
  prv_stop_block(i2c);

  prv_begin(i2c);
  prv_setbits(i2c, STM32F4_I2C_CR2_OFFSET, STM32F4_I2C_CR2_ITEVTEN | STM32F4_I2C_CR2_ITERREN);

  return 0;
}

static void prv_abort(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);

  prv_disable_all_interrupts(i2c);
  prv_setbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_STOP);
  i2c->state->xfer_state = PBL_I2C_STM32_COMPLETE;
  prv_stop_release(i2c);
}

static void prv_end_transfer_irq(const struct pbl_i2c_stm32 *i2c, int status) {
  prv_disable_all_interrupts(i2c);
  prv_setbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_STOP);
  i2c->state->xfer_state = PBL_I2C_STM32_COMPLETE;
  prv_stop_release(i2c);
  pbl_i2c_bus_complete(&i2c->bus, status);
}

//! The current message is done: continue a write, restart for the next message or stop.
static void prv_next_msg(const struct pbl_i2c_stm32 *i2c) {
  struct pbl_i2c_stm32_state *state = i2c->state;

  state->msg++;
  state->idx = 0;
  if (state->msg == state->num_msgs) {
    prv_end_transfer_irq(i2c, 0);
  } else {
    prv_begin(i2c);
  }
}

static bool prv_check_event(const struct pbl_i2c_stm32 *i2c, uint32_t event) {
  const uint32_t sr1 = prv_getreg(i2c, STM32F4_I2C_SR1_OFFSET);

  if ((sr1 & ~STM32F4_I2C_SR1_STOPF) == 0U) {
    return false;
  }

  return (EVENT(prv_getreg(i2c, STM32F4_I2C_SR2_OFFSET), sr1) & event) == event;
}

void pbl_i2c_stm32_ev_irq_handler(const struct pbl_i2c_stm32 *i2c) {
  struct pbl_i2c_stm32_state *state = i2c->state;

  if (state->xfer_state == PBL_I2C_STM32_COMPLETE) {
    // Spurious interrupt
    prv_clrbits(i2c, STM32F4_I2C_CR2_OFFSET, STM32F4_I2C_CR2_ITEVTEN | STM32F4_I2C_CR2_ITBUFEN);
    return;
  }

  // Spurious byte-transmitted and event-less interrupts follow repeated
  // starts; ignore anything that is not the expected event.
  if (!prv_check_event(i2c, s_guard_events[state->xfer_state])) {
    return;
  }

  struct pbl_i2c_msg *msg = &state->msgs[state->msg];
  const uint8_t addr = state->addr << 1;

  switch (state->xfer_state) {
    case PBL_I2C_STM32_START:
      if ((msg->flags & PBL_I2C_MSG_READ) != 0U) {
        prv_putreg(i2c, STM32F4_I2C_DR_OFFSET, addr | I2C_READ_WRITE_BIT);
        if (msg->len == 1) {
          // NACK the only byte to stop the device sending.
          prv_clrbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_ACK);
        }
        state->xfer_state = PBL_I2C_STM32_ADDRESS_RX;
      } else {
        prv_putreg(i2c, STM32F4_I2C_DR_OFFSET, addr & ~I2C_READ_WRITE_BIT);
        state->xfer_state = PBL_I2C_STM32_ADDRESS_TX;
      }
      break;

    case PBL_I2C_STM32_ADDRESS_TX:
      prv_setbits(i2c, STM32F4_I2C_CR2_OFFSET, STM32F4_I2C_CR2_ITBUFEN);
      state->xfer_state = PBL_I2C_STM32_WRITE_DATA;
      prv_putreg(i2c, STM32F4_I2C_DR_OFFSET, msg->buf[state->idx++]);
      break;

    case PBL_I2C_STM32_WRITE_DATA:
      if (state->idx == msg->len) {
        if (state->msg + 1 < state->num_msgs &&
            (state->msgs[state->msg + 1].flags & PBL_I2C_MSG_NOSTART) != 0U) {
          state->msg++;
          state->idx = 0;
          msg++;
        } else {
          prv_clrbits(i2c, STM32F4_I2C_CR2_OFFSET, STM32F4_I2C_CR2_ITBUFEN);
          state->xfer_state = PBL_I2C_STM32_END_WRITE;
          break;
        }
      }
      prv_putreg(i2c, STM32F4_I2C_DR_OFFSET, msg->buf[state->idx++]);
      break;

    case PBL_I2C_STM32_END_WRITE:
      prv_next_msg(i2c);
      break;

    case PBL_I2C_STM32_ADDRESS_RX:
      prv_setbits(i2c, STM32F4_I2C_CR2_OFFSET, STM32F4_I2C_CR2_ITBUFEN);
      state->xfer_state = PBL_I2C_STM32_READ_DATA;
      break;

    case PBL_I2C_STM32_READ_DATA:
      msg->buf[state->idx++] = (uint8_t)prv_getreg(i2c, STM32F4_I2C_DR_OFFSET);
      if (state->idx + 1 == msg->len) {
        // NACK the last byte.
        prv_clrbits(i2c, STM32F4_I2C_CR1_OFFSET, STM32F4_I2C_CR1_ACK);
      } else if (state->idx == msg->len) {
        prv_clrbits(i2c, STM32F4_I2C_CR2_OFFSET, STM32F4_I2C_CR2_ITBUFEN);
        prv_next_msg(i2c);
      }
      break;

    default:
      PBL_ASSERTN(false);
      break;
  }
}

void pbl_i2c_stm32_er_irq_handler(const struct pbl_i2c_stm32 *i2c) {
  const enum pbl_i2c_stm32_xfer_state xfer_state = i2c->state->xfer_state;

  if (xfer_state == PBL_I2C_STM32_COMPLETE) {
    prv_clrbits(i2c, STM32F4_I2C_CR2_OFFSET, STM32F4_I2C_CR2_ITERREN);
    return;
  }

  // A NACK of the address: no device there.
  const bool no_device =
      (prv_getreg(i2c, STM32F4_I2C_SR1_OFFSET) & STM32F4_I2C_SR1_AF) != 0U &&
      (xfer_state == PBL_I2C_STM32_ADDRESS_TX || xfer_state == PBL_I2C_STM32_ADDRESS_RX);

  // Overrun, bus error or NACK: end the transfer.
  prv_clrbits(i2c, STM32F4_I2C_SR1_OFFSET, SR1_ERRORS);
  prv_end_transfer_irq(i2c, no_device ? -ENXIO : -EIO);
}

const struct pbl_i2c_bus_ops pbl_i2c_stm32_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .start = prv_start,
  .abort = prv_abort,
};

#include PBL_DT_INSTANCES(st_stm32f4_i2c)
