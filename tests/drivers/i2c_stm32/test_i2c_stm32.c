/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#define _GNU_SOURCE

#include "i2c_model.h"
#include "traces.h"

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/adc/stm32.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/i2c/stm32.h>
#include <pbl/drivers/imu/mag3110/mag3110.h>
#include <pbl/drivers/mag.h>
#include <pbl/drivers/pmic/max14690.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/work.h>
#include <pbl/soc/stm32/clock.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <clar.h>
#include <sys/time.h>

#define PCLK1_HZ          32000000U
#define PCLK1_HZ_FIRMWARE 24000000U

static uint32_t s_pclk1_hz;
static int s_irq_locks;
static bool s_done;

void pbl_irq_lock(void) {
  s_irq_locks++;
}

void pbl_irq_unlock(void) {
  cl_assert(s_irq_locks > 0);
  s_irq_locks--;
}

bool pbl_irq_is_locked(void) {
  return s_irq_locks > 0;
}

bool pbl_in_isr(void) {
  return false;
}

bool pbl_kernel_is_running(void) {
  return true;
}

void pbl_irq_enable(pbl_irq_t irq) {
  m_trace("irq %d on", (int)irq);
}

void pbl_irq_disable(pbl_irq_t irq) {
  m_trace("irq %d off", (int)irq);
}

void pbl_busy_wait_us(uint32_t us) {
  m_trace("wait %uus", (unsigned int)us);
}

void pbl_sleep_ms(uint32_t ms) {
  m_trace("sleep %ums", (unsigned int)ms);
}

void soc_stm32_stop_block(void) {
  m_trace("stop block");
}

void soc_stm32_stop_release(void) {
  m_trace("stop release");
}

static int prv_pump(void);

int pbl_sem_take(struct pbl_sem *s, pbl_timeout_t timeout) {
  return prv_pump();
}

void pbl_sem_give(struct pbl_sem *s) {
  s_done = true;
}

struct pbl_workq pbl_sys_workq;

bool pbl_work_submit(struct pbl_workq *q, struct pbl_work *work) {
  m_trace("work");
  return true;
}

bool pbl_sys_work_call_from_isr(pbl_work_call_fn_t fn, void *data) {
  m_trace("work call");
  return true;
}

int pbl_mag_dev_init(const struct pbl_device *dev) {
  const struct pbl_mag *mag = container_of(dev, const struct pbl_mag, dev);

  return mag->ops->init(mag);
}

void pbl_mag_lock(const struct pbl_mag *mag) {
}

void pbl_mag_unlock(const struct pbl_mag *mag) {
}

void pbl_mag_report_sample(const struct pbl_mag *mag, const MagData *sample) {
  m_trace("mag sample %d %d %d", sample->x, sample->y, sample->z);
}

void pbl_adc_stm32_read(uint8_t channel, const struct pbl_gpio *pin, uint32_t *vref_total,
                        uint32_t *vmon_total) {
  m_trace("adc %u", (unsigned int)channel);
  *vref_total = 1500U;
  *vmon_total = 1000U;
}

uint32_t pbl_adc_stm32_to_mv(uint32_t vref_total, uint32_t vmon_total, uint32_t multiplier,
                             uint32_t divisor) {
  return 1000U * multiplier / divisor;
}

#define RCC_BASE   0x40023800U
#define GPIOB_BASE 0x40020400U
#define NUM_REGS   64

#define RCC_APB1RSTR 0x20U
#define RCC_APB1ENR  0x40U
#define I2C1_BIT     21U

#define GPIO_MODER 0x00U
#define GPIO_IDR   0x10U
#define GPIO_ODR   0x14U
#define GPIO_BSRR  0x18U

#define SCL_PIN 6U
#define SDA_PIN 9U

#define DR_IDLE 0x5a00U

I2C_TypeDef fake_i2c[3];

static I2C_TypeDef *const s_regs_i2c1 = &fake_i2c[0];

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;

static enum {
  PH_IDLE,
  PH_SB,
  PH_ADDR,
  PH_TX,
  PH_RX,
} s_phase;
static bool s_in_xfer;
static bool s_rx_more;
static bool s_scl_high;

static uint32_t prv_reset_value(uintptr_t addr) {
  switch (addr) {
    case GPIOB_BASE + 0x00U:
      return 0x00000280U;
    case GPIOB_BASE + 0x08U:
      return 0x000000c0U;
    case GPIOB_BASE + 0x0cU:
      return 0x00000100U;
    case RCC_BASE + 0x04U:
      return (128U << 6) | 16U;
    case RCC_BASE + 0x08U:
      return (2U << 2) | (4U << 10);
    default:
      return 0U;
  }
}

static uint32_t *prv_reg(uintptr_t addr) {
  for (int i = 0; i < s_num_regs; i++) {
    if (s_regs[i].addr == addr) {
      return &s_regs[i].val;
    }
  }
  cl_assert(s_num_regs < NUM_REGS);
  s_regs[s_num_regs].addr = addr;
  s_regs[s_num_regs].val = prv_reset_value(addr);
  return &s_regs[s_num_regs++].val;
}

static bool prv_i2c1_clock(void) {
  return (*prv_reg(RCC_BASE + RCC_APB1ENR) & PBL_BIT(I2C1_BIT)) != 0U;
}

static void prv_update_busy(void) {
  if (s_in_xfer || m.sda_held > 0 || m.busy_glitch) {
    s_regs_i2c1->SR2 |= I2C_SR2_BUSY;
  } else {
    s_regs_i2c1->SR2 &= ~I2C_SR2_BUSY;
  }
}

static void prv_i2c1_reset(void) {
  *s_regs_i2c1 = (I2C_TypeDef){.DR = DR_IDLE};
  s_phase = PH_IDLE;
  s_in_xfer = false;
  m.busy_glitch = false;
  prv_update_busy();
}

static void prv_regs_reset(void) {
  memset(s_regs, 0, sizeof(s_regs));
  s_num_regs = 0;
  memset(fake_i2c, 0, sizeof(fake_i2c));
  s_regs_i2c1->DR = DR_IDLE;
  s_phase = PH_IDLE;
  s_in_xfer = false;
  s_rx_more = false;
  s_scl_high = true;
}

static void prv_rcc_write(uint32_t off, uint32_t val) {
  uint32_t *reg = prv_reg(RCC_BASE + off);
  const uint32_t changed = (*reg ^ val) & PBL_BIT(I2C1_BIT);
  *reg = val;
  if ((off != RCC_APB1ENR && off != RCC_APB1RSTR) || changed == 0U) {
    return;
  }
  const bool on = (val & PBL_BIT(I2C1_BIT)) != 0U;
  m_trace("%s i2c1 %s", off == RCC_APB1ENR ? "clk" : "rst", on ? "on" : "off");
  if (off == RCC_APB1RSTR && on) {
    prv_i2c1_reset();
  }
}

static const char *prv_mode_name(uint32_t mode) {
  static const char *const names[] = {"in", "out", "af", "an"};
  return names[mode & 3U];
}

static bool prv_pin_out_level(unsigned int pin, bool *level) {
  const uint32_t moder = *prv_reg(GPIOB_BASE + GPIO_MODER);
  if (((moder >> (2U * pin)) & 3U) != 1U) {
    return false;
  }
  *level = (*prv_reg(GPIOB_BASE + GPIO_ODR) & PBL_BIT(pin)) != 0U;
  return true;
}

static void prv_bus_lines_update(void) {
  bool level;
  const bool scl_high = !prv_pin_out_level(SCL_PIN, &level) || level;
  if (scl_high && !s_scl_high && m.sda_held > 0) {
    if (--m.sda_held == 0) {
      m_trace("sda released");
    }
  }
  s_scl_high = scl_high;
  prv_update_busy();
}

static void prv_gpiob_write(uint32_t off, uint32_t val) {
  const uint32_t old_moder = *prv_reg(GPIOB_BASE + GPIO_MODER);
  const uint32_t old_odr = *prv_reg(GPIOB_BASE + GPIO_ODR);
  if (off == GPIO_BSRR) {
    *prv_reg(GPIOB_BASE + GPIO_ODR) = (old_odr | (val & 0xffffU)) & ~(val >> 16);
  } else {
    *prv_reg(GPIOB_BASE + off) = val;
  }
  const uint32_t moder = *prv_reg(GPIOB_BASE + GPIO_MODER);
  const uint32_t odr = *prv_reg(GPIOB_BASE + GPIO_ODR);
  static const unsigned int pins[] = {SCL_PIN, SDA_PIN};
  for (size_t i = 0; i < ARRAY_LENGTH(pins); i++) {
    const unsigned int pin = pins[i];
    const char *name = pin == SCL_PIN ? "scl" : "sda";
    const uint32_t mode = (moder >> (2U * pin)) & 3U;
    if (mode != ((old_moder >> (2U * pin)) & 3U)) {
      m_trace("%s %s", name, prv_mode_name(mode));
      if (mode == 2U && m.busy_glitch) {
        m.busy_glitch = false;
        m_trace("busy glitch gone");
      }
    }
    if (mode == 1U && ((odr ^ old_odr) & PBL_BIT(pin)) != 0U) {
      m_trace("%s %d", name, (odr & PBL_BIT(pin)) != 0U);
    }
  }
  prv_bus_lines_update();
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  if (addr == GPIOB_BASE + GPIO_IDR) {
    bool level;
    uint32_t idr = PBL_BIT(SCL_PIN);
    const bool sda_driven_low = prv_pin_out_level(SDA_PIN, &level) && !level;
    if (m.sda_held == 0 && !sda_driven_low) {
      idr |= PBL_BIT(SDA_PIN);
    }
    return idr;
  }
  return *prv_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  if (addr >= RCC_BASE && addr < RCC_BASE + 0x400U) {
    prv_rcc_write(addr - RCC_BASE, val);
    return;
  }
  if (addr >= GPIOB_BASE && addr < GPIOB_BASE + 0x400U) {
    prv_gpiob_write(addr - GPIOB_BASE, val);
    return;
  }
  *prv_reg(addr) = val;
}

static void prv_trace_regs(void) {
  const I2C_TypeDef *r = s_regs_i2c1;
  m_trace("I2C1 CR1 %04x CR2 %04x OAR1 %04x CCR %04x TRISE %04x FLTR %04x", r->CR1, r->CR2, r->OAR1,
          r->CCR, r->TRISE, r->FLTR);
  m_trace("GPIOB MODER %08x OTYPER %08x OSPEEDR %08x PUPDR %08x", *prv_reg(GPIOB_BASE + 0x00U),
          *prv_reg(GPIOB_BASE + 0x04U), *prv_reg(GPIOB_BASE + 0x08U), *prv_reg(GPIOB_BASE + 0x0cU));
  m_trace("GPIOB ODR %08x AFRL %08x AFRH %08x", *prv_reg(GPIOB_BASE + GPIO_ODR),
          *prv_reg(GPIOB_BASE + 0x20U), *prv_reg(GPIOB_BASE + 0x24U));
  m_trace("GPIOF MODER %08x PUPDR %08x GPIOG MODER %08x PUPDR %08x", *prv_reg(0x40021400U),
          *prv_reg(0x4002140cU), *prv_reg(0x40021800U), *prv_reg(0x4002180cU));
  m_trace("EXTI IMR %08x RTSR %08x FTSR %08x SYSCFG EXTICR2 %08x EXTICR4 %08x",
          *prv_reg(0x40013c00U), *prv_reg(0x40013c08U), *prv_reg(0x40013c0cU),
          *prv_reg(0x4001380cU), *prv_reg(0x40013814U));
}

static bool s_dump_regs;

static void prv_observe(void) {
  I2C_TypeDef *r = s_regs_i2c1;

  if ((r->CR1 & I2C_CR1_STOP) != 0U) {
    r->CR1 &= ~I2C_CR1_STOP;
    if (s_in_xfer) {
      m_stop();
    }
    s_in_xfer = false;
    s_phase = PH_IDLE;
    r->SR1 = 0U;
    r->SR2 = 0U;
  }
  if ((r->CR1 & I2C_CR1_START) != 0U) {
    if ((r->CR1 & I2C_CR1_PE) == 0U || !prv_i2c1_clock()) {
      m_violation("start with CR1 %04x clock %d", r->CR1, prv_i2c1_clock());
    }
    if (s_dump_regs) {
      s_dump_regs = false;
      prv_trace_regs();
    }
    r->CR1 &= ~I2C_CR1_START;
    m_start(s_in_xfer);
    s_in_xfer = true;
    s_phase = PH_SB;
    r->SR1 = I2C_SR1_SB;
    r->SR2 = I2C_SR2_MSL;
    r->DR = DR_IDLE;
  }
  if (s_phase == PH_IDLE) {
    r->DR = DR_IDLE;
  } else if (r->DR != DR_IDLE && s_phase != PH_RX) {
    const uint8_t byte = (uint8_t)r->DR;
    r->DR = DR_IDLE;
    if (s_phase == PH_SB) {
      if (m.stall) {
        m_token("%02x%c~", byte >> 1, (byte & 1U) != 0U ? 'r' : 'w');
        r->SR1 = 0U;
        s_phase = PH_ADDR;
      } else if (m_address(byte)) {
        s_phase = PH_ADDR;
        r->SR1 = I2C_SR1_ADDR | (m.read ? 0U : I2C_SR1_TXE);
        r->SR2 |= m.read ? 0U : I2C_SR2_TRA;
        s_rx_more = m.read;
      } else {
        r->SR1 = I2C_SR1_AF;
        s_phase = PH_ADDR;
      }
    } else if (s_phase == PH_TX) {
      r->SR1 = m_write(byte) ? I2C_SR1_TXE : I2C_SR1_AF;
    } else {
      m_violation("DR write %02x in phase %d", byte, s_phase);
    }
  }
  prv_update_busy();
}

//! The handler read SR1 and SR2, clearing ADDR, and read DR, clearing RXNE.
static void prv_after_ev(bool had_rxne) {
  I2C_TypeDef *r = s_regs_i2c1;

  if (s_phase == PH_ADDR && (r->SR1 & I2C_SR1_ADDR) != 0U) {
    r->SR1 &= ~I2C_SR1_ADDR;
    s_phase = m.read ? PH_RX : PH_TX;
  }
  if (s_phase == PH_RX && had_rxne) {
    r->SR1 &= ~I2C_SR1_RXNE;
    r->DR = DR_IDLE;
  }
}

static bool prv_ev_pending(void) {
  const I2C_TypeDef *r = s_regs_i2c1;
  if ((r->CR2 & I2C_CR2_ITEVTEN) == 0U) {
    return false;
  }
  return (r->SR1 & (I2C_SR1_SB | I2C_SR1_ADDR | I2C_SR1_BTF | I2C_SR1_STOPF)) != 0U ||
         ((r->CR2 & I2C_CR2_ITBUFEN) != 0U && (r->SR1 & (I2C_SR1_TXE | I2C_SR1_RXNE)) != 0U);
}

static bool prv_er_pending(void) {
  const I2C_TypeDef *r = s_regs_i2c1;
  return (r->CR2 & I2C_CR2_ITERREN) != 0U &&
         (r->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR)) != 0U;
}

static bool prv_advance(void) {
  I2C_TypeDef *r = s_regs_i2c1;

  if (s_phase == PH_TX && (r->SR1 & I2C_SR1_TXE) != 0U && (r->SR1 & I2C_SR1_BTF) == 0U &&
      (r->CR2 & I2C_CR2_ITBUFEN) == 0U) {
    r->SR1 |= I2C_SR1_BTF;
    return true;
  }
  if (s_phase == PH_RX && (r->SR1 & I2C_SR1_RXNE) == 0U && s_rx_more) {
    const bool ack = (r->CR1 & I2C_CR1_ACK) != 0U;
    r->DR = m_read(ack);
    r->SR1 |= I2C_SR1_RXNE;
    s_rx_more = ack;
    return true;
  }
  return false;
}

PBL_I2C_STM32_DEFINE(s_i2c1, "i2c1", I2C1, RCC_APB1Periph_I2C1, 400000, PBL_GPIO(STM32_GPIOB, 6, 0),
                     GPIO_AF_I2C1, PBL_GPIO(STM32_GPIOB, 9, 0), GPIO_AF_I2C1, nullptr);

static const struct pbl_i2c_dev s_pmic_i2c = PBL_I2C_DEV(&s_i2c1.bus, 0x28);
static const struct pbl_i2c_dev s_mag_i2c = PBL_I2C_DEV(&s_i2c1.bus, 0x0e);

PBL_MAX14690_DEFINE(s_max14690, &s_i2c1.bus, &s_pmic_i2c, .int_gpio = PBL_GPIO(STM32_GPIOG, 7, 0),
                    .mon_adc_channel = 1, .mon_adc_pin = PBL_GPIO(STM32_GPIOG, 0, 0));
const struct pbl_max14690 *const MAX14690 = &s_max14690;

PBL_MAG3110_DEFINE(s_mag, &s_i2c1.bus, 0x0e, .int_gpio = PBL_GPIO(STM32_GPIOF, 14, 0),
                   .axis_map = {1, 0, 2}, .axis_dir = {-1, -1, 1});

#include "../../../drivers/pmic/max14690.c"

static int prv_pump(void) {
  for (int i = 0; i < 1000; i++) {
    prv_observe();
    if (s_done) {
      s_done = false;
      return 0;
    }
    if (prv_er_pending()) {
      pbl_i2c_stm32_er_irq_handler(&s_i2c1);
      continue;
    }
    if (prv_ev_pending()) {
      const bool had_rxne = (s_regs_i2c1->SR1 & I2C_SR1_RXNE) != 0U;
      pbl_i2c_stm32_ev_irq_handler(&s_i2c1);
      prv_after_ev(had_rxne);
      continue;
    }
    if (!prv_advance()) {
      return -EAGAIN;
    }
  }
  m_violation("transfer without end");
  return -EAGAIN;
}

// Standard Peripheral Library, on I2C1 registers in memory
////////////////////////////////////////////////////////////////////////////////

void soc_stm32_clock_set(enum soc_stm32_bus bus, uint32_t mask, bool enable) {
  cl_assert_equal_i(bus, SOC_STM32_APB1);
  const uint32_t enr = *prv_reg(RCC_BASE + RCC_APB1ENR);
  prv_rcc_write(RCC_APB1ENR, enable ? (enr | mask) : (enr & ~mask));
}

void RCC_APB1PeriphResetCmd(uint32_t periph, FunctionalState state) {
  const uint32_t rstr = *prv_reg(RCC_BASE + RCC_APB1RSTR);
  prv_rcc_write(RCC_APB1RSTR, state != DISABLE ? (rstr | periph) : (rstr & ~periph));
}

void RCC_GetClocksFreq(RCC_ClocksTypeDef *clocks) {
  const uint32_t sysclk = s_pclk1_hz == PCLK1_HZ_FIRMWARE ? 4U * s_pclk1_hz : 2U * s_pclk1_hz;
  clocks->SYSCLK_Frequency = sysclk;
  clocks->HCLK_Frequency = sysclk;
  clocks->PCLK1_Frequency = s_pclk1_hz;
  clocks->PCLK2_Frequency = s_pclk1_hz == PCLK1_HZ_FIRMWARE ? sysclk / 2U : sysclk;
}

static void prv_dump_trace(const char *name, const char *trace) {
  const char *path = getenv("I2C_TRACE_OUT");
  if (path == NULL) {
    return;
  }
  FILE *f = fopen(path, "a");
  cl_assert(f != NULL);
  fprintf(f, "#define %s \\\n", name);
  for (const char *line = trace; *line != '\0';) {
    const char *end = strchr(line, '\n');
    fprintf(f, "  \"%.*s\\n\" \\\n", (int)(end - line), line);
    line = end + 1;
  }
  fprintf(f, "  \"\"\n\n");
  fclose(f);
}

#define CHECK_TRACE(name)               \
  do {                                  \
    const char *trace = m_trace_stop(); \
    prv_dump_trace(#name, trace);       \
    cl_assert_equal_s(trace, name);     \
  } while (0)

static sigjmp_buf s_power_off_jmp;

static void prv_alarm(int sig) {
  siglongjmp(s_power_off_jmp, 1);
}

static void prv_bus_init(void) {
  cl_assert_equal_i(pbl_device_init(&s_i2c1.bus.dev), 0);
}

static void prv_pmic_init(void) {
  cl_assert_equal_i(pbl_device_init(&s_max14690.dev), 0);
}

void test_i2c_stm32__initialize(void) {
  s_pclk1_hz = PCLK1_HZ;
  m_devices_reset();
  prv_regs_reset();
  s_irq_locks = 0;
  s_done = false;
  s_dump_regs = false;
  s_trace_on = false;
  s_present = false;
  const struct pbl_device *devs[] = {&dt_rcc.dev,        &dt_gpiob.dev,      &dt_gpiof.dev,
                                     &dt_gpiog.dev,      &dt_gpiob_intc.dev, &dt_gpiof_intc.dev,
                                     &dt_gpiog_intc.dev, &s_i2c1.bus.dev,    &s_max14690.dev,
                                     &s_mag.mag.dev};
  for (size_t i = 0; i < ARRAY_LENGTH(devs); i++) {
    *devs[i]->state = (struct pbl_device_state){0};
  }
  memset(dt_rcc.use_counts, 0, dt_rcc.num_clks);
  cl_assert_equal_i(pbl_device_init(&dt_rcc.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_gpiob.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_gpiof.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_gpiog.dev), 0);
}

void test_i2c_stm32__cleanup(void) {
  s_trace_on = false;
  static const struct pbl_gpio lines[] = {
    PBL_GPIO(STM32_GPIOG, 7, 0), PBL_GPIO(STM32_GPIOF, 14, 0)
  };
  for (size_t i = 0; i < ARRAY_LENGTH(lines); i++) {
    struct pbl_irq_spec spec;
    if (pbl_gpio_to_irq(&lines[i], PBL_IRQ_TYPE_EDGE_FALLING, &spec) == 0) {
      (void)pbl_intc_free(&spec);
    }
  }
  cl_assert_equal_s(m.violation, "");
  cl_assert_equal_i(m.violations, 0);
  cl_assert_equal_i(s_irq_locks, 0);
  cl_assert(!s_i2c1.state->stop_blocked);
}

void test_i2c_stm32__bus_init(void) {
  m_trace_start();
  prv_bus_init();
  prv_trace_regs();
  CHECK_TRACE(TRACE_BUS_INIT);
}

void test_i2c_stm32__pmic_boot(void) {
  s_dump_regs = true;
  m_trace_start();
  prv_pmic_init();
  prv_trace_regs();
  CHECK_TRACE(TRACE_PMIC_BOOT);
}

void test_i2c_stm32__pmic_boot_apb1_24mhz(void) {
  s_pclk1_hz = PCLK1_HZ_FIRMWARE;
  *prv_reg(RCC_BASE + 0x04U) = (4U << 24) | (192U << 6) | 16U;
  *prv_reg(RCC_BASE + 0x08U) = (4U << 13) | (5U << 10) | (2U << 2) | 2U;
  s_dump_regs = true;
  m_trace_start();
  prv_pmic_init();
  prv_trace_regs();
  CHECK_TRACE(TRACE_PMIC_BOOT_APB1_24MHZ);
}

void test_i2c_stm32__pmic_absent(void) {
  m.pmic.present = false;
  m_trace_start();
  cl_assert_equal_i(pbl_device_init(&s_max14690.dev), -ENODEV);
  CHECK_TRACE(TRACE_PMIC_ABSENT);
}

void test_i2c_stm32__pmic_charging(void) {
  prv_pmic_init();
  m.pmic.regs[0x02] = 0x03;
  m.pmic.regs[0x03] = 0x08;
  m.pmic.regs[0x05] = 0x08;
  BatteryChargeStatus status;
  m_trace_start();
  prv_handle_interrupt(NULL);
  prv_debounce_work(NULL);
  cl_assert(pmic_is_charging());
  cl_assert(pmic_is_usb_connected());
  cl_assert_equal_i(battery_charge_status_get(&status), 0);
  cl_assert_equal_i(status, BatteryChargeStatusCC);
  battery_set_charge_enable(false);
  battery_set_charge_enable(true);
  m.pmic.regs[0x03] = 0x00;
  prv_debounce_work(NULL);
  CHECK_TRACE(TRACE_PMIC_CHARGING);
}

void test_i2c_stm32__pmic_rails_and_mon(void) {
  prv_pmic_init();
  uint8_t chip_id;
  uint8_t chip_rev;
  uint8_t buck1;
  m_trace_start();
  set_ldo3_power_state(true);
  set_ldo3_power_state(true);
  set_ldo3_power_state(false);
  set_ldo3_power_state(false);
  pmic_read_chip_info(&chip_id, &chip_rev, &buck1);
  cl_assert_equal_i(battery_get_millivolts(), 3000);
  cl_assert_equal_i(pmic_get_vsys(), 4000);
  CHECK_TRACE(TRACE_PMIC_RAILS_MON);
}

void test_i2c_stm32__pmic_power_off(void) {
  prv_pmic_init();
  struct sigaction sa = {.sa_handler = prv_alarm};
  sigemptyset(&sa.sa_mask);
  cl_assert_equal_i(sigaction(SIGALRM, &sa, NULL), 0);
  m_trace_start();
  if (sigsetjmp(s_power_off_jmp, 1) == 0) {
    const struct itimerval t = {.it_value = {.tv_usec = 50000}};
    cl_assert_equal_i(setitimer(ITIMER_REAL, &t, NULL), 0);
    prv_power_off(PBL_SYS_OFF_POWER_OFF, NULL);
    cl_fail("power off returned");
  }
  m_trace("powered off");
  CHECK_TRACE(TRACE_PMIC_POWER_OFF);
  *s_i2c1.bus.state = (struct pbl_i2c_bus_state){0};
  s_i2c1.state->stop_blocked = false;
  s_irq_locks = 0;
}

void test_i2c_stm32__mag(void) {
  prv_pmic_init();
  static const uint8_t sample[] = {0x01, 0x10, 0xff, 0x20, 0x00, 0x30};
  memcpy(&m.mag.regs[0x01], sample, sizeof(sample));
  uint32_t actual_us;
  MagData data;
  m_trace_start();
  cl_assert_equal_i(pbl_device_init(&s_mag.mag.dev), 0);
  cl_assert_equal_i(pbl_mag3110_ops.set_sampling_interval(&s_mag.mag, 50000, &actual_us), 0);
  m.mag.regs[0x00] = 0x08;
  cl_assert_equal_i(pbl_mag3110_ops.peek(&s_mag.mag, &data), 0);
  cl_assert_equal_i(pbl_mag3110_ops.set_sampling_interval(&s_mag.mag, 200000, &actual_us), 0);
  cl_assert_equal_i(pbl_mag3110_ops.set_sampling_interval(&s_mag.mag, 0, &actual_us), 0);
  CHECK_TRACE(TRACE_MAG);
}

void test_i2c_stm32__data_nack(void) {
  prv_pmic_init();
  m.pmic.nack_byte = 2;
  m_trace_start();
  cl_assert(!pmic_set_charger_state(false));
  CHECK_TRACE(TRACE_DATA_NACK);
  cl_assert_equal_i(pbl_i2c_reg_write_byte(&s_pmic_i2c, 0x0a, 0xcc), -EIO);
}

void test_i2c_stm32__address_nack(void) {
  prv_pmic_init();
  m.mag.present = false;
  m_trace_start();
  cl_assert_equal_i(pbl_device_init(&s_mag.mag.dev), -ENODEV);
  CHECK_TRACE(TRACE_ADDRESS_NACK);
  uint8_t val;
  cl_assert_equal_i(pbl_i2c_reg_read_byte(&s_mag_i2c, 0x07, &val), -ENXIO);
}

void test_i2c_stm32__timeout(void) {
  prv_pmic_init();
  uint8_t val;
  m.stall = true;
  m_trace_start();
  cl_assert_equal_i(pbl_i2c_reg_read_byte(&s_pmic_i2c, 0x02, &val), -ETIMEDOUT);
  m.stall = false;
  cl_assert_equal_i(pbl_i2c_reg_read_byte(&s_pmic_i2c, 0x02, &val), 0);
  CHECK_TRACE(TRACE_TIMEOUT);
}

void test_i2c_stm32__busy_glitch(void) {
  prv_pmic_init();
  uint8_t val;
  m.busy_glitch = true;
  prv_update_busy();
  m_trace_start();
  cl_assert_equal_i(pbl_i2c_reg_read_byte(&s_pmic_i2c, 0x00, &val), 0);
  CHECK_TRACE(TRACE_BUSY_GLITCH);
}

void test_i2c_stm32__bus_clear(void) {
  prv_pmic_init();
  uint8_t val;
  m.sda_held = 4;
  prv_update_busy();
  m_trace_start();
  cl_assert_equal_i(pbl_i2c_reg_read_byte(&s_pmic_i2c, 0x00, &val), 0);
  CHECK_TRACE(TRACE_BUS_CLEAR);
}

void test_i2c_stm32__bus_stuck(void) {
  prv_pmic_init();
  uint8_t val;
  m.sda_held = 100;
  prv_update_busy();
  m_trace_start();
  cl_assert_equal_i(pbl_i2c_reg_read_byte(&s_pmic_i2c, 0x00, &val), -EBUSY);
  CHECK_TRACE(TRACE_BUS_STUCK);
  m.sda_held = 0;
}
