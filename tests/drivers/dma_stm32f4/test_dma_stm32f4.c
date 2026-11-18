/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/drivers/clk.h>

#include <clar.h>

// The clock controller the instance takes its clock from, counting the users
PBL_DEVICE_CLASS_DEFINE(pbl_clk_controller_class, "clk");
PBL_CLK_CONTROLLER_STATE_DEFINE(s_rcc, 1);
static const struct pbl_clk_controller s_rcc =
    PBL_CLK_CONTROLLER_INIT(s_rcc, "rcc", nullptr, nullptr, nullptr);

static int s_clk_users;
static int s_clk_ret;
// Depth of the interrupt lock, and whether the controller's init, which needs none, is running
static int s_locks;
static bool s_in_init;

int pbl_clk_enable(const struct pbl_clk *clk) {
  if (s_clk_ret != 0) {
    return s_clk_ret;
  }
  s_clk_users++;
  return 0;
}

int pbl_clk_disable(const struct pbl_clk *clk) {
  // A stream's clock goes with its running state, under the lock
  cl_assert(s_locks > 0 || s_in_init);
  cl_assert(s_clk_users > 0);
  s_clk_users--;
  return 0;
}

#include "../../../drivers/dma/stm32f4.c"

// Registers
////////////////////////////////////////////////////////////////////////////////
// Offsets and bits from the reference manual, not from the driver's header. Without the clock,
// the registers read as zero and ignore writes. A flag is cleared by writing 1 to its bit in the
// flag clear register. A stream whose enable bit is cleared stops after s_stop_reads reads of its
// control register, or never with s_stuck. A stream is enabled and the flags are read under the
// interrupt lock.

#define DMA2_BASE 0x40026400U
#define NUM_REGS  64

#define LISR      0x00U
#define HISR      0x04U
#define LIFCR     0x08U
#define HIFCR     0x0cU
#define SCR(n)    (0x10U + 0x18U * (n))
#define SNDTR(n)  (0x14U + 0x18U * (n))
#define SPAR(n)   (0x18U + 0x18U * (n))
#define SM0AR(n)  (0x1cU + 0x18U * (n))
#define SFCR(n)   (0x24U + 0x18U * (n))
#define CR_EN     (1U << 0)
#define CR_IRQS   (0xfU << 1)
#define ALL_FLAGS 0x0f7d0f7dU

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;
static uint32_t s_stop_reads;
static bool s_stuck;
static uint32_t s_stopping[8];

static uint32_t *prv_reg(uintptr_t addr) {
  for (int i = 0; i < s_num_regs; i++) {
    if (s_regs[i].addr == addr) {
      return &s_regs[i].val;
    }
  }
  cl_assert(s_num_regs < NUM_REGS);
  s_regs[s_num_regs].addr = addr;
  s_regs[s_num_regs].val = 0U;
  return &s_regs[s_num_regs++].val;
}

static uint32_t prv_dma(uint32_t offset) {
  return *prv_reg(DMA2_BASE + offset);
}

static int prv_cr_stream(uintptr_t addr) {
  for (int n = 0; n < 8; n++) {
    if (addr == DMA2_BASE + SCR(n)) {
      return n;
    }
  }
  return -1;
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  int n = prv_cr_stream(addr);

  if (s_clk_users == 0) {
    return 0U;
  }
  if (addr == DMA2_BASE + LISR || addr == DMA2_BASE + HISR) {
    cl_assert(s_locks > 0);
  }
  if (n >= 0 && s_stopping[n] > 0U && !s_stuck && --s_stopping[n] == 0U) {
    *prv_reg(addr) &= ~CR_EN;
  }
  return *prv_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  int n = prv_cr_stream(addr);

  if (s_clk_users == 0) {
    return;
  }
  if (addr == DMA2_BASE + LIFCR) {
    *prv_reg(DMA2_BASE + LISR) &= ~val;
  } else if (addr == DMA2_BASE + HIFCR) {
    *prv_reg(DMA2_BASE + HISR) &= ~val;
  } else if (n >= 0 && (*prv_reg(addr) & CR_EN) != 0U && (val & CR_EN) == 0U &&
             (s_stop_reads > 0U || s_stuck)) {
    *prv_reg(addr) = val | CR_EN;
    s_stopping[n] = s_stop_reads;
  } else {
    if (n >= 0 && (val & CR_EN) != 0U) {
      cl_assert(s_locks > 0);
    }
    *prv_reg(addr) = val;
  }
}

static void prv_irq(uint8_t stream) {
  pbl_dma_stm32f4_irq_handler(&(const struct pbl_dma_stm32f4_stream_irq){&dt_dma2_inst, stream});
}

//! The stream sets its flags and, at the end of a normal transfer or on an error, clears its
//! enable bit, then its interrupt is taken. Flags in the driver's order: FE, -, DME, TE, HT, TC.
static void prv_event(uint8_t stream, uint32_t flags) {
  static const uint8_t shift[4] = {0U, 6U, 16U, 22U};
  uint32_t *cr = prv_reg(DMA2_BASE + SCR(stream));

  if ((flags & STM32F4_DMA_TEIF) != 0U ||
      ((flags & STM32F4_DMA_TCIF) != 0U && (*cr & (1U << 8)) == 0U)) {
    *cr &= ~CR_EN;
  }
  *prv_reg(DMA2_BASE + (stream < 4U ? LISR : HISR)) |= flags << shift[stream % 4U];
  prv_irq(stream);
}

// Kernel
////////////////////////////////////////////////////////////////////////////////

void pbl_irq_lock(void) {
  s_locks++;
}

void pbl_irq_unlock(void) {
  cl_assert(s_locks > 0);
  s_locks--;
}

void pbl_irq_enable(pbl_irq_t irq) {
}

// Tests
////////////////////////////////////////////////////////////////////////////////

// SPI6_TX: stream 5, channel 1; I2S2_RX would be on DMA1, so stream 3 here stands for a ring
static const struct pbl_dma_chan s_tx =
    PBL_DMA_CHAN(&dt_dma2, 5, 1, PBL_DMA_STM32F4_PRIORITY(PBL_DMA_STM32F4_PRIORITY_VERY_HIGH));
static const struct pbl_dma_chan s_rx =
    PBL_DMA_CHAN(&dt_dma2, 3, 0, PBL_DMA_STM32F4_PRIORITY(PBL_DMA_STM32F4_PRIORITY_HIGH));

#define SPI_DR 0x4001543cU

static uint8_t s_buf[64];
static int s_calls;
static int s_status;

static enum {
  ON_CALL_NOTHING,
  ON_CALL_RESTART_SINGLE,
  ON_CALL_TERMINATE_RING,
  ON_CALL_RESTART_RING,
} s_on_call;

static void prv_callback(void *arg, int status) {
  cl_assert_equal_i(s_locks, 0);
  s_calls++;
  s_status = status;
  switch (s_on_call) {
    case ON_CALL_RESTART_SINGLE:
      cl_assert_equal_i(
          pbl_dma_prep_slave_single(&s_tx, s_buf, 8, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
      break;
    case ON_CALL_TERMINATE_RING:
      cl_assert_equal_i(pbl_dma_terminate(&s_rx), 0);
      break;
    case ON_CALL_RESTART_RING:
      cl_assert_equal_i(pbl_dma_terminate(&s_rx), 0);
      cl_assert_equal_i(
          pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 16, 8, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr),
          0);
      break;
    default:
      break;
  }
  s_on_call = ON_CALL_NOTHING;
}

static int prv_init(void) {
  int ret;

  s_in_init = true;
  ret = pbl_device_init(&dt_dma2.dev);
  s_in_init = false;
  return ret;
}

static void prv_reset_regs(void) {
  memset(s_regs, 0, sizeof(s_regs));
  memset(s_stopping, 0, sizeof(s_stopping));
  s_num_regs = 0;
}

void test_dma_stm32f4__initialize(void) {
  prv_reset_regs();
  memset(dt_dma2_streams, 0, sizeof(dt_dma2_streams));
  memset(dt_dma2_dma_chans, 0, sizeof(dt_dma2_dma_chans));
  *dt_dma2.dev.state = (struct pbl_device_state){0};
  s_stop_reads = 3U;
  s_stuck = false;
  s_clk_users = 0;
  s_clk_ret = 0;
  s_locks = 0;
  s_calls = 0;
  s_status = 1;
  s_on_call = ON_CALL_NOTHING;
  cl_assert_equal_i(prv_init(), 0);

  const struct pbl_dma_slave_config tx = {
    .dst_addr = SPI_DR,
    .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
  };
  cl_assert_equal_i(pbl_dma_slave_config(&s_tx, &tx), 0);

  const struct pbl_dma_slave_config rx = {
    .src_addr = SPI_DR,
    .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_2_BYTES,
  };
  cl_assert_equal_i(pbl_dma_slave_config(&s_rx, &rx), 0);
}

void test_dma_stm32f4__cleanup(void) {
  cl_assert_equal_i(s_locks, 0);
}

void test_dma_stm32f4__init_stops_what_was_left_running(void) {
  *dt_dma2.dev.state = (struct pbl_device_state){0};
  for (uint8_t n = 0U; n < 8U; n++) {
    *prv_reg(DMA2_BASE + SCR(n)) = CR_EN | CR_IRQS | (1U << 10);
    *prv_reg(DMA2_BASE + SFCR(n)) = 0xa1U;
  }
  *prv_reg(DMA2_BASE + LISR) = ALL_FLAGS;
  *prv_reg(DMA2_BASE + HISR) = ALL_FLAGS;

  cl_assert_equal_i(prv_init(), 0);
  for (uint8_t n = 0U; n < 8U; n++) {
    cl_assert_equal_i(*prv_reg(DMA2_BASE + SCR(n)), 1U << 10);
    cl_assert_equal_i(*prv_reg(DMA2_BASE + SFCR(n)), 0U);
  }
  cl_assert_equal_i(*prv_reg(DMA2_BASE + LISR), 0U);
  cl_assert_equal_i(*prv_reg(DMA2_BASE + HISR), 0U);
  cl_assert_equal_i(s_clk_users, 0);
}

void test_dma_stm32f4__a_transfer_runs_with_the_clock_on(void) {
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);

  // EN, TEIE, TCIE, memory to peripheral, MINC, very high priority, channel 1
  cl_assert_equal_i(prv_dma(0x88), 0x02030455U);
  cl_assert_equal_i(prv_dma(0x8c), 40U);
  cl_assert_equal_i(prv_dma(0x90), SPI_DR);
  cl_assert_equal_i(prv_dma(0x94), (uint32_t)(uintptr_t)s_buf);
  cl_assert_equal_i(prv_dma(0x9c), 0U);
  cl_assert_equal_i(s_clk_users, 1);

  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr),
      -EBUSY);
}

void test_dma_stm32f4__the_end_of_a_transfer_lets_the_clock_go(void) {
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);

  // TCIF5 is bit 11 of HISR
  *prv_reg(DMA2_BASE + 0x88) &= ~CR_EN;
  *prv_reg(DMA2_BASE + HISR) |= (1U << 11) | (1U << 10);
  prv_irq(5);
  cl_assert_equal_i(s_calls, 1);
  cl_assert_equal_i(s_status, 0);
  cl_assert_equal_i(s_clk_users, 0);
  cl_assert_equal_i(prv_dma(HISR), 0U);
}

void test_dma_stm32f4__the_callback_starts_the_next_transfer(void) {
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
  s_on_call = ON_CALL_RESTART_SINGLE;

  prv_event(5, STM32F4_DMA_TCIF);
  cl_assert_equal_i(s_calls, 1);
  cl_assert_equal_i(s_clk_users, 1);
  cl_assert_equal_i(prv_dma(0x8c), 8U);
  cl_assert(prv_dma(0x88) & CR_EN);
}

void test_dma_stm32f4__a_ring_reports_each_half(void) {
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 64, 32, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr), 0);
  // EN, TEIE, HTIE, TCIE, peripheral to memory, CIRC, MINC, half words, high priority, channel 0
  cl_assert_equal_i(prv_dma(0x58), 0x00022d1dU);
  cl_assert_equal_i(prv_dma(0x5c), 32U);

  prv_event(3, STM32F4_DMA_HTIF);
  // TCIF3 is bit 27 of LISR
  *prv_reg(DMA2_BASE + LISR) |= 1U << 27;
  prv_irq(3);
  prv_event(3, STM32F4_DMA_HTIF);
  cl_assert_equal_i(s_calls, 3);
  cl_assert_equal_i(s_clk_users, 1);
  cl_assert_equal_i(prv_dma(LISR), 0U);

  cl_assert_equal_i(pbl_dma_terminate(&s_rx), 0);
  cl_assert_equal_i(s_clk_users, 0);
  cl_assert_equal_i(*prv_reg(DMA2_BASE + 0x58) & (CR_EN | CR_IRQS), 0U);
  cl_assert_equal_i(pbl_dma_terminate(&s_rx), 0);
  cl_assert_equal_i(s_clk_users, 0);
}

void test_dma_stm32f4__a_ring_of_one_period_has_no_half(void) {
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 64, 64, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr), 0);
  cl_assert_equal_i(prv_dma(0x58) & (1U << 3), 0U);

  prv_event(3, STM32F4_DMA_HTIF);
  cl_assert_equal_i(s_calls, 0);
  prv_event(3, STM32F4_DMA_TCIF);
  cl_assert_equal_i(s_calls, 1);
}

void test_dma_stm32f4__both_halves_in_one_interrupt(void) {
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 64, 32, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr), 0);

  prv_event(3, STM32F4_DMA_HTIF | STM32F4_DMA_TCIF);
  cl_assert_equal_i(s_calls, 2);
  cl_assert_equal_i(s_clk_users, 1);
}

void test_dma_stm32f4__a_ring_stopped_by_its_first_callback(void) {
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 64, 32, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr), 0);
  s_on_call = ON_CALL_TERMINATE_RING;

  prv_event(3, STM32F4_DMA_HTIF | STM32F4_DMA_TCIF);
  cl_assert_equal_i(s_calls, 1);
  cl_assert_equal_i(s_clk_users, 0);
}

void test_dma_stm32f4__a_ring_restarted_by_its_first_callback(void) {
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 64, 32, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr), 0);
  s_on_call = ON_CALL_RESTART_RING;

  prv_event(3, STM32F4_DMA_HTIF | STM32F4_DMA_TCIF);
  cl_assert_equal_i(s_calls, 1);
  cl_assert_equal_i(s_clk_users, 1);
  cl_assert_equal_i(prv_dma(0x5c), 8U);
  cl_assert(prv_dma(0x58) & CR_EN);
}

void test_dma_stm32f4__fifo_and_direct_mode_errors_only_clear(void) {
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);

  prv_event(5, STM32F4_DMA_FEIF | STM32F4_DMA_DMEIF);
  cl_assert_equal_i(s_calls, 0);
  cl_assert_equal_i(s_clk_users, 1);
  cl_assert(prv_dma(0x88) & CR_EN);
  cl_assert_equal_i(prv_dma(HISR), 0U);
}

void test_dma_stm32f4__a_transfer_error_stops_the_stream(void) {
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 64, 32, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr), 0);

  prv_event(3, STM32F4_DMA_TEIF);
  cl_assert_equal_i(s_calls, 1);
  cl_assert_equal_i(s_status, -EIO);
  cl_assert_equal_i(s_clk_users, 0);
}

void test_dma_stm32f4__no_callback_after_terminate(void) {
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
  cl_assert_equal_i(pbl_dma_terminate(&s_tx), 0);

  prv_event(5, STM32F4_DMA_TCIF);
  cl_assert_equal_i(s_calls, 0);
  cl_assert_equal_i(s_clk_users, 0);
}

void test_dma_stm32f4__no_callback_after_terminate_with_the_clock_on(void) {
  // Another stream keeps the clock, so the stopped one's flags can be read
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 64, 32, PBL_DMA_DEV_TO_MEM, nullptr, nullptr), 0);
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
  cl_assert_equal_i(pbl_dma_terminate(&s_tx), 0);
  cl_assert_equal_i(s_clk_users, 1);

  prv_event(5, STM32F4_DMA_TCIF);
  prv_event(5, STM32F4_DMA_TEIF);
  cl_assert_equal_i(s_calls, 0);
  cl_assert_equal_i(s_clk_users, 1);
}

void test_dma_stm32f4__terminate_mid_flight_then_restart(void) {
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
  cl_assert_equal_i(pbl_dma_terminate(&s_tx), 0);
  cl_assert_equal_i(s_clk_users, 0);
  cl_assert_equal_i(*prv_reg(DMA2_BASE + 0x88) & CR_EN, 0U);

  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 8, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
  cl_assert_equal_i(prv_dma(0x8c), 8U);
  cl_assert(prv_dma(0x88) & CR_EN);
  prv_event(5, STM32F4_DMA_TCIF);
  cl_assert_equal_i(s_calls, 1);
}

void test_dma_stm32f4__a_stream_that_does_not_stop(void) {
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
  s_stuck = true;

  cl_assert_equal_i(pbl_dma_terminate(&s_tx), -ETIMEDOUT);
  cl_assert_equal_i(s_clk_users, 0);
}

void test_dma_stm32f4__a_clock_that_fails_leaves_the_stream_idle(void) {
  s_clk_ret = -EIO;
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), -EIO);
  cl_assert_equal_i(s_clk_users, 0);

  s_clk_ret = 0;
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
  cl_assert_equal_i(s_clk_users, 1);
}

void test_dma_stm32f4__slave_config_while_running(void) {
  const struct pbl_dma_slave_config tx = {
    .dst_addr = SPI_DR,
    .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
  };

  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_tx, s_buf, 40, PBL_DMA_MEM_TO_DEV, prv_callback, nullptr), 0);
  cl_assert_equal_i(pbl_dma_slave_config(&s_tx, &tx), -EBUSY);
}

void test_dma_stm32f4__the_residue_is_in_bytes(void) {
  size_t residue = 1U;

  cl_assert_equal_i(pbl_dma_tx_status(&s_rx, &residue), 0);
  cl_assert_equal_i(residue, 0U);

  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 64, 64, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr), 0);
  *prv_reg(DMA2_BASE + 0x5c) = 10U;
  cl_assert_equal_i(pbl_dma_tx_status(&s_rx, &residue), 0);
  cl_assert_equal_i(residue, 20U);
}

void test_dma_stm32f4__what_it_cannot_do(void) {
  const struct pbl_dma_chan pinc = PBL_DMA_CHAN(&dt_dma2, 1, 0, 1U << 9);
  const struct pbl_dma_chan request = PBL_DMA_CHAN(&dt_dma2, 1, 8, 0);
  const struct pbl_dma_slave_config cfg = {
    .src_addr = SPI_DR,
    .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
  };

  cl_assert_equal_i(pbl_dma_slave_config(&pinc, &cfg), -EINVAL);
  cl_assert_equal_i(pbl_dma_slave_config(&request, &cfg), -EINVAL);
  // Half words, an odd length
  cl_assert_equal_i(
      pbl_dma_prep_slave_single(&s_rx, s_buf, 7, PBL_DMA_DEV_TO_MEM, nullptr, nullptr), -EINVAL);
  // Half words, periods of an odd length
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 6, 3, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr),
      -EINVAL);
  // A ring of three periods
  cl_assert_equal_i(
      pbl_dma_prep_dma_cyclic(&s_rx, s_buf, 48, 16, PBL_DMA_DEV_TO_MEM, prv_callback, nullptr),
      -EINVAL);
  // More items than NDTR holds
  cl_assert_equal_i(pbl_dma_prep_slave_single(&s_tx, (void *)0x20000000, 0x10000,
                                              PBL_DMA_MEM_TO_DEV, nullptr, nullptr),
                    -EINVAL);
  cl_assert_equal_i(s_clk_users, 0);
  cl_assert_equal_i(pbl_dma_prep_slave_single(&s_tx, (void *)0x20000000, 0xffff, PBL_DMA_MEM_TO_DEV,
                                              nullptr, nullptr),
                    0);
  cl_assert_equal_i(prv_dma(0x8c), 0xffffU);
  cl_assert_equal_i(pbl_dma_terminate(&s_tx), 0);
  cl_assert_equal_i(s_clk_users, 0);
}

static int prv_start_at(uintptr_t addr, size_t len) {
  int ret =
      pbl_dma_prep_slave_single(&s_tx, (void *)addr, len, PBL_DMA_MEM_TO_DEV, nullptr, nullptr);

  if (ret == 0) {
    cl_assert_equal_i(pbl_dma_terminate(&s_tx), 0);
  }
  return ret;
}

void test_dma_stm32f4__the_core_coupled_memory_is_out_of_reach(void) {
  // Up to the start of the CCM, from its end
  cl_assert_equal_i(prv_start_at(0x10000000 - 8, 8), 0);
  cl_assert_equal_i(prv_start_at(0x10000000 - 8, 9), -EINVAL);
  cl_assert_equal_i(prv_start_at(0x10000100, 8), -EINVAL);
  cl_assert_equal_i(prv_start_at(0x10010000 - 1, 1), -EINVAL);
  cl_assert_equal_i(prv_start_at(0x10010000, 8), 0);
  // Across the end of the address space
  cl_assert_equal_i(prv_start_at(UINTPTR_MAX - 7, 16), -EINVAL);
  cl_assert_equal_i(s_clk_users, 0);
}
