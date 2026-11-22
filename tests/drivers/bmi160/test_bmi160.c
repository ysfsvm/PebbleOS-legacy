/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bmi160_model.h"
#include "traces.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/accel.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/work.h>
#include <pbl/soc/stm32/board.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <clar.h>
#include <dt-bindings/mfd/stm32f4-rcc.h>

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config =
      {
        .axes_offsets[AXIS_X] = 1,
        .axes_offsets[AXIS_Y] = 0,
        .axes_offsets[AXIS_Z] = 2,
        .shake_thresholds[AccelThresholdHigh] = 0x64,
        .shake_thresholds[AccelThresholdLow] = 0xf,
        .double_tap_threshold = 12500,
      },
  .accel_int_gpios = {
    PBL_GPIO(STM32_GPIOG, 5, 0),
    PBL_GPIO(STM32_GPIOG, 6, 0),
  },
};

#define PCLK2_HZ 64000000U

static int s_irq_locks;
static struct pbl_work *s_work;

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

void pbl_irq_enable(pbl_irq_t irq) {
  m_trace("irq %d on", (int)irq);
}

void pbl_irq_disable(pbl_irq_t irq) {
  m_trace("irq %d off", (int)irq);
}

void pbl_irq_clear_pending(pbl_irq_t irq) {
}

void pbl_busy_wait_us(uint32_t us) {
  m_trace("wait %uus", (unsigned int)us);
}

void pbl_sleep_ms(uint32_t ms) {
  m_trace("sleep %ums", (unsigned int)ms);
}

uint64_t pbl_realtime_ms(void) {
  return 1000000ULL;
}

struct pbl_workq pbl_sys_workq_high;

bool pbl_work_submit(struct pbl_workq *q, struct pbl_work *work) {
  cl_assert(q == &pbl_sys_workq_high);
  cl_assert(s_work == NULL);
  m_trace("work");
  s_work = work;
  return true;
}

static void prv_run_work(void) {
  cl_assert(s_work != NULL);
  struct pbl_work *work = s_work;
  s_work = NULL;
  work->fn(work);
}

int pbl_accel_dev_init(const struct pbl_device *dev) {
  const struct pbl_accel *accel = container_of(dev, const struct pbl_accel, dev);

  return accel->ops->init(accel);
}

void pbl_accel_lock(const struct pbl_accel *accel) {
}

void pbl_accel_unlock(const struct pbl_accel *accel) {
}

void pbl_accel_report_sample(const struct pbl_accel *accel, const AccelDriverSample *sample) {
  m_report("sample %d %d %d %llu", sample->x, sample->y, sample->z,
           (unsigned long long)sample->timestamp_us);
}

void pbl_accel_report_event(const struct pbl_accel *accel, const struct pbl_accel_event *event) {
  m_report("event %d %d %d", (int)event->type, (int)event->axis, (int)event->direction);
}

#define RCC_BASE   0x40023800U
#define GPIOA_BASE 0x40020000U
#define GPIOG_BASE 0x40021800U
#define SPI1_BASE  0x40013000U
#define NUM_REGS   64

#define RCC_APB2RSTR 0x24U
#define RCC_APB2ENR  0x44U
#define SPI1_BIT     12U

#define GPIO_MODER 0x00U
#define GPIO_IDR   0x10U
#define GPIO_ODR   0x14U
#define GPIO_BSRR  0x18U

#define SPI_CR1      0x00U
#define SPI_SR       0x08U
#define SPI_DR       0x0cU
#define SPI_SR_RXNE  0x0001U
#define SPI_SR_TXE   0x0002U
#define SPI_CR1_MSTR 0x0004U
#define SPI_CR1_SPE  0x0040U

static SPI_TypeDef s_spi1;

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;
static bool s_cs_low;
static bool s_rx_pending;
static uint8_t s_rx;
static bool s_int_level[2];

static uint32_t prv_reset_value(uintptr_t addr) {
  switch (addr) {
    case GPIOA_BASE + 0x00U:
      return 0xa8000000U;
    case GPIOA_BASE + 0x08U:
      return 0x0c000000U;
    case GPIOA_BASE + 0x0cU:
      return 0x64000000U;
    case RCC_BASE + 0x04U:
      return (128U << 6) | 16U;
    case RCC_BASE + 0x08U:
      return 2U << 2;
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

static void prv_spi1_reset(void) {
  s_spi1 = (SPI_TypeDef){.SR = SPI_SR_TXE, .CRCPR = 7, .I2SPR = 2};
  s_rx_pending = false;
}

static void prv_regs_reset(void) {
  memset(s_regs, 0, sizeof(s_regs));
  s_num_regs = 0;
  s_spi1 = (SPI_TypeDef){
    .CR1 = 0x5a5a,
    .CR2 = 0x00e7,
    .SR = SPI_SR_TXE,
    .CRCPR = 0x1234,
    .I2SCFGR = 0x0f3f,
    .I2SPR = 2
  };
  s_cs_low = false;
  s_rx_pending = false;
  s_int_level[0] = false;
  s_int_level[1] = false;
}

static bool prv_spi1_clock(void) {
  return (*prv_reg(RCC_BASE + RCC_APB2ENR) & PBL_BIT(SPI1_BIT)) != 0U;
}

static const char *prv_rcc_name(uint32_t off) {
  switch (off) {
    case 0x10U:
    case 0x30U:
      return "ahb1";
    case 0x20U:
    case 0x40U:
      return "apb1";
    case 0x24U:
    case 0x44U:
      return "apb2";
    default:
      return "?";
  }
}

static void prv_rcc_write(uint32_t off, uint32_t val) {
  uint32_t *reg = prv_reg(RCC_BASE + off);
  const uint32_t changed = *reg ^ val;
  *reg = val;
  if (off < 0x10U || off > 0x44U || (off > 0x24U && off < 0x30U)) {
    return;
  }
  for (unsigned int bit = 0; bit < 32U; bit++) {
    if ((changed & PBL_BIT(bit)) != 0U) {
      m_trace("%s %s %u %s", off >= 0x30U ? "clk" : "rst", prv_rcc_name(off), bit,
              (val & PBL_BIT(bit)) != 0U ? "on" : "off");
    }
  }
  if (off == RCC_APB2RSTR && (val & PBL_BIT(SPI1_BIT)) != 0U) {
    prv_spi1_reset();
  }
}

static void prv_cs_update(void) {
  const uint32_t moder = *prv_reg(GPIOA_BASE + GPIO_MODER);
  const uint32_t odr = *prv_reg(GPIOA_BASE + GPIO_ODR);
  const bool low = ((moder >> 8) & 3U) == 1U && (odr & PBL_BIT(4)) == 0U;
  if (low != s_cs_low) {
    s_cs_low = low;
    m_trace("cs %d", low ? 0 : 1);
    m_chip_select(low);
  }
}

static void prv_gpioa_write(uint32_t off, uint32_t val) {
  if (off == GPIO_BSRR) {
    uint32_t *odr = prv_reg(GPIOA_BASE + GPIO_ODR);
    *odr = (*odr | (val & 0xffffU)) & ~(val >> 16);
  } else {
    *prv_reg(GPIOA_BASE + off) = val;
  }
  prv_cs_update();
}

static void prv_spi_send(uint16_t val) {
  const uint16_t cr1 = s_spi1.CR1;
  if (!prv_spi1_clock() || (cr1 & SPI_CR1_SPE) == 0U || (cr1 & SPI_CR1_MSTR) == 0U) {
    m_violation("SPI1 byte %02x with clock %d CR1 %04x", val, prv_spi1_clock(), cr1);
  }
  if (s_rx_pending) {
    m_violation("SPI1 overrun");
  }
  s_rx = m_chip_exchange((uint8_t)val);
  s_rx_pending = true;
}

static uint16_t prv_spi_receive(void) {
  if (!s_rx_pending) {
    m_violation("SPI1 read without data");
  }
  s_rx_pending = false;
  return s_rx;
}

static uint16_t prv_spi_sr(void) {
  return (uint16_t)(SPI_SR_TXE | (s_rx_pending ? SPI_SR_RXNE : 0U));
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  if (addr >= SPI1_BASE && addr < SPI1_BASE + 0x24U) {
    const uint32_t off = addr - SPI1_BASE;
    if (off == SPI_DR) {
      return prv_spi_receive();
    }
    if (off == SPI_SR) {
      return prv_spi_sr();
    }
    return ((volatile uint16_t *)&s_spi1)[off / 2U];
  }
  if (addr == GPIOG_BASE + GPIO_IDR) {
    return (s_int_level[0] ? PBL_BIT(5) : 0U) | (s_int_level[1] ? PBL_BIT(6) : 0U);
  }
  return *prv_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  if (addr >= SPI1_BASE && addr < SPI1_BASE + 0x24U) {
    const uint32_t off = addr - SPI1_BASE;
    if (off == SPI_DR) {
      prv_spi_send((uint16_t)val);
    } else if (off != SPI_SR) {
      ((volatile uint16_t *)&s_spi1)[off / 2U] = (uint16_t)val;
    }
    return;
  }
  if (addr >= RCC_BASE && addr < RCC_BASE + 0x400U) {
    prv_rcc_write(addr - RCC_BASE, val);
    return;
  }
  if (addr >= GPIOA_BASE && addr < GPIOA_BASE + 0x400U) {
    prv_gpioa_write(addr - GPIOA_BASE, val);
    return;
  }
  *prv_reg(addr) = val;
}

static void prv_trace_regs(void) {
  const SPI_TypeDef *spi = &s_spi1;
  m_trace("SPI1 CR1 %04x CR2 %04x CRCPR %04x I2SCFGR %04x I2SPR %04x", spi->CR1, spi->CR2,
          spi->CRCPR, spi->I2SCFGR, spi->I2SPR);
  m_trace("GPIOA MODER %08x OTYPER %08x OSPEEDR %08x PUPDR %08x", *prv_reg(GPIOA_BASE + 0x00U),
          *prv_reg(GPIOA_BASE + 0x04U), *prv_reg(GPIOA_BASE + 0x08U), *prv_reg(GPIOA_BASE + 0x0cU));
  m_trace("GPIOA ODR %08x AFRL %08x AFRH %08x", *prv_reg(GPIOA_BASE + GPIO_ODR),
          *prv_reg(GPIOA_BASE + 0x20U), *prv_reg(GPIOA_BASE + 0x24U));
  m_trace("GPIOG MODER %08x PUPDR %08x", *prv_reg(GPIOG_BASE + 0x00U),
          *prv_reg(GPIOG_BASE + 0x0cU));
  m_trace("EXTI IMR %08x RTSR %08x FTSR %08x SYSCFG EXTICR2 %08x", *prv_reg(0x40013c00U),
          *prv_reg(0x40013c08U), *prv_reg(0x40013c0cU), *prv_reg(0x4001380cU));
  m_trace("RCC APB2ENR %08x APB2RSTR %08x", *prv_reg(RCC_BASE + RCC_APB2ENR),
          *prv_reg(RCC_BASE + RCC_APB2RSTR));
}

#include "../../../drivers/imu/bmi160/bmi160.c"
#include "../../../drivers/imu/bmi160/bmi160_spi.c"

static const struct pbl_accel *const s_accel = &dt_accel;

static void prv_dump_trace(const char *name, const char *trace) {
  const char *path = getenv("BMI160_TRACE_OUT");
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

static void prv_driver_reset(void) {
  s_dev = NULL;
  s_raw_unit_to_mgs = 8192;
  s_accel_power_mode = BMI160_Accel_Mode_Suspend;
  s_gyro_power_mode = BMI160_Gyro_Mode_Suspend;
  s_accel_outstanding_motion_work = false;
  s_accel_outstanding_data_work = false;
  s_fifo_in_use = false;
  curr_fifo_num_samples_wm = 0;
  s_rotated_180 = false;
  s_shake_threshold = 0;
  s_double_tap_detection_enabled = false;
  s_shake_detection_enabled = false;
  s_operating_states[AccelOperatingModeData].enabled = false;
  s_operating_states[AccelOperatingModeData].sample_interval = BMI160SampleRate_25_HZ;
  s_operating_states[AccelOperatingModeShakeDetection].enabled = false;
  s_operating_states[AccelOperatingModeShakeDetection].sample_interval = BMI160SampleRate_25_HZ;
  s_operating_states[AccelOperatingModeDoubleTapDetection].enabled = false;
  s_operating_states[AccelOperatingModeDoubleTapDetection].sample_interval =
      BMI160SampleRate_200_HZ;
  s_interrupt_users = 0;
  memset(s_irqs, 0, sizeof(s_irqs));
}

static void prv_devices_reset(void) {
  const struct pbl_device *devs[] = {&dt_rcc.dev,        &dt_rcc_reset.dev, &pbl_pinctrl_stm32.dev,
                                     &dt_gpioa.dev,      &dt_gpiog.dev,     &dt_gpioa_intc.dev,
                                     &dt_gpiog_intc.dev, &dt_spi1.dev,      &dt_accel.dev};
  for (size_t i = 0; i < ARRAY_LENGTH(devs); i++) {
    *devs[i]->state = (struct pbl_device_state){0};
  }
  memset(dt_rcc.use_counts, 0, dt_rcc.num_clks);
  cl_assert_equal_i(pbl_device_init(&dt_rcc.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_gpioa.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_gpiog.dev), 0);
}

static void prv_accel_init(void) {
  cl_assert_equal_i(pbl_device_init(&s_accel->dev), 0);
}

static void prv_set_status(uint8_t reg, uint8_t val) {
  m.regs[reg] = val;
}

void test_bmi160__initialize(void) {
  m_power_on(true);
  prv_regs_reset();
  prv_driver_reset();
  s_irq_locks = 0;
  s_work = NULL;
  s_trace_on = false;
  prv_devices_reset();
}

void test_bmi160__cleanup(void) {
  s_trace_on = false;
  if (s_dev != NULL && m.present) {
    (void)prv_set_num_samples(s_accel, 0);
  }
  for (size_t i = 0; i < ARRAY_LENGTH(s_irqs); i++) {
    if (s_irqs[i].intc != NULL) {
      (void)pbl_intc_free(&s_irqs[i]);
    }
  }
  cl_assert_equal_s(m.violation, "");
  cl_assert_equal_i(m.violations, 0);
  cl_assert_equal_i(s_irq_locks, 0);
  cl_assert(s_work == NULL);
}

void test_bmi160__init(void) {
  m_trace_start();
  prv_accel_init();
  prv_trace_regs();
  CHECK_TRACE(TRACE_INIT);
}

void test_bmi160__init_without_chip(void) {
  m_power_on(false);
  m_trace_start();
  cl_assert_equal_i(pbl_device_init(&s_accel->dev), -ENODEV);
  prv_trace_regs();
  CHECK_TRACE(TRACE_INIT_ABSENT);
  s_dev = NULL;
}

void test_bmi160__peek(void) {
  prv_accel_init();
  static const uint8_t data[] = {0x10, 0x02, 0xf0, 0xfd, 0x00, 0x10, 0x34, 0x12, 0x00};
  memcpy(&m.regs[0x12], data, sizeof(data));
  AccelDriverSample sample;
  m_trace_start();
  cl_assert_equal_i(pbl_bmi160_ops.peek(s_accel, &sample), 0);
  CHECK_TRACE(TRACE_PEEK);
}

void test_bmi160__data_ready(void) {
  prv_accel_init();
  uint32_t actual_us;
  m_trace_start();
  cl_assert_equal_i(pbl_bmi160_ops.set_sampling_interval(s_accel, 10000, &actual_us), 0);
  cl_assert_equal_i(pbl_bmi160_ops.set_num_samples(s_accel, 1), 0);
  prv_set_status(0x1b, 0x80);
  static const uint8_t data[] = {0x00, 0x08, 0x00, 0xf8, 0x00, 0x20, 0x00, 0x01, 0x00};
  memcpy(&m.regs[0x12], data, sizeof(data));
  bmi160_IRQ2_handler(NULL);
  prv_run_work();
  prv_set_status(0x1b, 0x00);
  bmi160_IRQ2_handler(NULL);
  prv_run_work();
  CHECK_TRACE(TRACE_DATA_READY);
}

void test_bmi160__fifo(void) {
  prv_accel_init();
  uint32_t actual_us;
  cl_assert_equal_i(pbl_bmi160_ops.set_sampling_interval(s_accel, 40000, &actual_us), 0);
  m_trace_start();
  cl_assert_equal_i(pbl_bmi160_ops.set_num_samples(s_accel, 25), 0);
  uint8_t fifo[10 * 6];
  for (size_t i = 0; i < sizeof(fifo); i++) {
    fifo[i] = (uint8_t)(i * 13U + 1U);
  }
  m_fifo_load(fifo, sizeof(fifo));
  s_int_level[1] = false;
  bmi160_IRQ2_handler(NULL);
  prv_run_work();
  CHECK_TRACE(TRACE_FIFO);
}

void test_bmi160__fifo_still_full(void) {
  prv_accel_init();
  uint32_t actual_us;
  cl_assert_equal_i(pbl_bmi160_ops.set_sampling_interval(s_accel, 40000, &actual_us), 0);
  cl_assert_equal_i(pbl_bmi160_ops.set_num_samples(s_accel, 25), 0);
  uint8_t fifo[4 * 6 + 3];
  for (size_t i = 0; i < sizeof(fifo); i++) {
    fifo[i] = (uint8_t)(0xa0U - i);
  }
  m_fifo_load(fifo, sizeof(fifo));
  s_int_level[1] = true;
  m_trace_start();
  bmi160_IRQ2_handler(NULL);
  prv_run_work();
  cl_assert_equal_i(pbl_bmi160_ops.set_num_samples(s_accel, 10), 0);
  CHECK_TRACE(TRACE_FIFO_STILL_FULL);
}

void test_bmi160__shake(void) {
  prv_accel_init();
  m_trace_start();
  cl_assert_equal_i(pbl_bmi160_ops.set_shake_detection(s_accel, true), 0);
  prv_set_status(0x1c, 0x04);
  prv_set_status(0x1e, 0x02 | 0x08);
  bmi160_IRQ1_handler(NULL);
  prv_run_work();
  cl_assert_equal_i(pbl_bmi160_ops.set_shake_sensitivity_high(s_accel, true), 0);
  cl_assert_equal_i(pbl_bmi160_ops.set_shake_sensitivity_percent(s_accel, 20), 0);
  cl_assert_equal_i(pbl_bmi160_ops.set_shake_sensitivity_high(s_accel, false), 0);
  cl_assert_equal_i(pbl_bmi160_ops.set_shake_detection(s_accel, false), 0);
  CHECK_TRACE(TRACE_SHAKE);
}

void test_bmi160__double_tap(void) {
  prv_accel_init();
  cl_assert_equal_i(pbl_bmi160_ops.set_rotated(s_accel, true), 0);
  m_trace_start();
  cl_assert_equal_i(pbl_bmi160_ops.set_double_tap_detection(s_accel, true), 0);
  prv_set_status(0x1c, 0x10);
  prv_set_status(0x1e, 0x40 | 0x80);
  bmi160_IRQ1_handler(NULL);
  prv_run_work();
  prv_set_status(0x1c, 0x00);
  prv_set_status(0x1e, 0x00);
  bmi160_IRQ1_handler(NULL);
  prv_run_work();
  cl_assert_equal_i(pbl_bmi160_ops.set_double_tap_detection(s_accel, false), 0);
  CHECK_TRACE(TRACE_DOUBLE_TAP);
}

void test_bmi160__power_modes(void) {
  prv_accel_init();
  uint32_t actual_us;
  m_trace_start();
  cl_assert_equal_i(pbl_bmi160_ops.set_sampling_interval(s_accel, 40000, &actual_us), 0);
  cl_assert_equal_i(actual_us, 40000);
  cl_assert_equal_i(pbl_bmi160_ops.set_sampling_interval(s_accel, 1000, &actual_us), 0);
  cl_assert_equal_i(pbl_bmi160_ops.set_sampling_interval(s_accel, 0, &actual_us), 0);
  bmi160_set_accel_power_mode(BMI160_Accel_Mode_Suspend);
  bmi160_set_accel_power_mode(BMI160_Accel_Mode_Normal);
  prv_trace_regs();
  CHECK_TRACE(TRACE_POWER_MODES);
}
