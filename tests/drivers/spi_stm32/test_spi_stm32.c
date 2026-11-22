/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/spi/stm32.c"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <clar.h>
#include <devicetree/devices.h>

#define RCC_BASE  0x40023800U
#define SPI1_BASE 0x40013000U
#define SPI2_BASE 0x40003800U
#define NUM_REGS  32

#define RCC_APB1RSTR 0x20U
#define RCC_APB2RSTR 0x24U
#define RCC_APB1ENR  0x40U
#define RCC_APB2ENR  0x44U

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;
static char s_log[2048];
static int s_lock_depth;
static int s_pinctrl_calls;
static int s_busy_reads;
static bool s_rx_pending;
static int s_rx_polls;
static bool s_after_rx;
static uint8_t s_rx;

static void prv_log(const char *fmt, ...) {
  char line[64];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  cl_assert(strlen(s_log) + strlen(line) + 2 < sizeof(s_log));
  strcat(s_log, line);
  strcat(s_log, "\n");
}

static uint32_t *prv_model_reg(uintptr_t addr) {
  for (int i = 0; i < s_num_regs; i++) {
    if (s_regs[i].addr == addr) {
      return &s_regs[i].val;
    }
  }
  cl_assert(s_num_regs < NUM_REGS);
  s_regs[s_num_regs].addr = addr;
  switch (addr) {
    case RCC_BASE + 0x04U:
      s_regs[s_num_regs].val = (128U << 6) | 16U;
      break;
    case RCC_BASE + 0x08U:
      s_regs[s_num_regs].val = (2U << 2) | (4U << 10);
      break;
    default:
      s_regs[s_num_regs].val = 0U;
      break;
  }
  return &s_regs[s_num_regs++].val;
}

static bool prv_clock(uintptr_t base) {
  return base == SPI1_BASE ? (*prv_model_reg(RCC_BASE + RCC_APB2ENR) & PBL_BIT(12)) != 0U
                           : (*prv_model_reg(RCC_BASE + RCC_APB1ENR) & PBL_BIT(14)) != 0U;
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  const uintptr_t base = addr & ~0x3ffU;

  if (base == SPI1_BASE || base == SPI2_BASE) {
    switch (addr - base) {
      case STM32F4_SPI_SR_OFFSET:
        if (s_after_rx && s_busy_reads > 0) {
          s_busy_reads--;
          return STM32F4_SPI_SR_TXE | STM32F4_SPI_SR_BSY;
        }
        if (s_rx_pending && s_rx_polls > 0) {
          s_rx_polls--;
          return STM32F4_SPI_SR_TXE | STM32F4_SPI_SR_BSY;
        }
        return STM32F4_SPI_SR_TXE | (s_rx_pending ? STM32F4_SPI_SR_RXNE : 0U);
      case STM32F4_SPI_DR_OFFSET:
        cl_assert(s_rx_pending && s_rx_polls == 0);
        s_rx_pending = false;
        s_after_rx = true;
        return s_rx;
      default:
        break;
    }
  }
  return *prv_model_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  const uintptr_t base = addr & ~0x3ffU;
  const char *name = base == SPI1_BASE ? "spi1" : "spi2";

  if (base == SPI1_BASE || base == SPI2_BASE) {
    cl_assert(prv_clock(base));
    if (addr - base == STM32F4_SPI_DR_OFFSET) {
      const uint32_t cr1 = *prv_model_reg(base + STM32F4_SPI_CR1_OFFSET);
      cl_assert((cr1 & STM32F4_SPI_CR1_SPE) != 0U);
      cl_assert(!s_rx_pending);
      prv_log("%s %02x", name, val);
      s_after_rx = false;
      s_rx = (uint8_t)(val ^ 0xffU);
      s_rx_pending = true;
      s_rx_polls = 2;
      return;
    }
    prv_log("%s %02x = %04x", name, addr - base, val);
  } else if (addr == RCC_BASE + RCC_APB2ENR || addr == RCC_BASE + RCC_APB1ENR) {
    const uint32_t bit = addr == RCC_BASE + RCC_APB2ENR ? PBL_BIT(12) : PBL_BIT(14);
    if (((*prv_model_reg(addr) ^ val) & bit) != 0U) {
      prv_log("clk %s %s", addr == RCC_BASE + RCC_APB2ENR ? "spi1" : "spi2",
              (val & bit) != 0U ? "on" : "off");
    }
  } else if (addr == RCC_BASE + RCC_APB2RSTR) {
    cl_assert(prv_clock(SPI1_BASE));
    prv_log("rst spi1 %s", (val & PBL_BIT(12)) != 0U ? "on" : "off");
  } else if (addr == RCC_BASE + RCC_APB1RSTR) {
    cl_fail("SPI2 has no reset line here");
  }
  *prv_model_reg(addr) = val;
}

void pbl_irq_lock(void) {
  s_lock_depth++;
}

void pbl_irq_unlock(void) {
  s_lock_depth--;
}

int pbl_pinctrl_select_state(const struct pbl_pinctrl *pinctrl, enum pbl_pinctrl_state_id id) {
  cl_assert_equal_i(id, PBL_PINCTRL_STATE_DEFAULT);
  s_pinctrl_calls++;
  return 0;
}

int pbl_gpio_configure(const struct pbl_gpio *gpio, uint32_t flags) {
  cl_fail("chip select configured");
  return -EIO;
}

int pbl_gpio_set_raw(const struct pbl_gpio *gpio, bool level) {
  cl_fail("chip select driven");
  return -EIO;
}

static const struct pbl_spi_dev s_dev_5mhz = PBL_SPI_DEV(&dt_spi1, 0, 5000000, PBL_SPI_MODE_0);
static const struct pbl_spi_dev s_dev_mode3 =
    PBL_SPI_DEV(&dt_spi1, 0, 32000000, PBL_SPI_MODE_3 | PBL_SPI_LSB_FIRST);
static const struct pbl_spi_dev s_dev_slow = PBL_SPI_DEV(&dt_spi1, 0, 100000, PBL_SPI_MODE_1);
static const struct pbl_spi_dev s_dev_spi2 = PBL_SPI_DEV(&dt_spi2, 0, 1000000, PBL_SPI_MODE_2);

void test_spi_stm32__initialize(void) {
  memset(s_regs, 0, sizeof(s_regs));
  s_num_regs = 0;
  s_log[0] = '\0';
  s_lock_depth = 0;
  s_pinctrl_calls = 0;
  s_busy_reads = 0;
  s_rx_pending = false;
  s_rx_polls = 0;
  s_after_rx = false;
  const struct pbl_device *devs[] = {&dt_rcc.dev, &dt_rcc_reset.dev, &dt_spi1.dev, &dt_spi2.dev};
  for (size_t i = 0; i < ARRAY_LENGTH(devs); i++) {
    *devs[i]->state = (struct pbl_device_state){0};
  }
  memset(dt_rcc.use_counts, 0, dt_rcc.num_clks);
}

void test_spi_stm32__cleanup(void) {
  cl_assert_equal_i(s_lock_depth, 0);
  cl_assert(!s_rx_pending);
}

void test_spi_stm32__init_resets_the_block_once_with_its_clock(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi1.dev), 0);
  cl_assert_equal_i(s_pinctrl_calls, 1);
  cl_assert_equal_s(s_log,
                    "clk spi1 on\n"
                    "rst spi1 on\n"
                    "rst spi1 off\n"
                    "clk spi1 off\n");
}

void test_spi_stm32__init_without_a_reset_line(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi2.dev), 0);
  cl_assert_equal_i(s_pinctrl_calls, 1);
  cl_assert_equal_s(s_log, "");
}

void test_spi_stm32__setup_leaves_the_registers_alone(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi1.dev), 0);
  s_log[0] = '\0';
  cl_assert_equal_i(pbl_spi_setup(&s_dev_5mhz), 0);
  cl_assert_equal_s(s_log, "");
}

void test_spi_stm32__first_transfer_sets_the_block_up(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi1.dev), 0);
  cl_assert_equal_i(pbl_spi_setup(&s_dev_5mhz), 0);
  s_log[0] = '\0';
  const uint8_t tx[] = {0x80};
  uint8_t rx[2];
  cl_assert_equal_i(pbl_spi_write_read(&s_dev_5mhz, tx, sizeof(tx), rx, sizeof(rx)), 0);
  cl_assert_equal_s(s_log,
                    "clk spi1 on\n"
                    "spi1 00 = 031c\n"
                    "spi1 00 = 035c\n"
                    "spi1 80\n"
                    "spi1 00\n"
                    "spi1 00\n"
                    "clk spi1 off\n");
  cl_assert_equal_i(rx[0], 0xff);
  cl_assert_equal_i(rx[1], 0xff);
}

void test_spi_stm32__later_transfers_keep_the_settings(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi1.dev), 0);
  const uint8_t tx[] = {0x12, 0x34};
  uint8_t rx[2];
  cl_assert_equal_i(pbl_spi_write(&s_dev_5mhz, tx, 1), 0);
  s_log[0] = '\0';
  struct pbl_spi_xfer xfer = {.tx_buf = tx, .rx_buf = rx, .len = 2};
  cl_assert_equal_i(pbl_spi_transfer(&s_dev_5mhz, &xfer, 1), 0);
  cl_assert_equal_s(s_log,
                    "clk spi1 on\n"
                    "spi1 12\n"
                    "spi1 34\n"
                    "clk spi1 off\n");
  cl_assert_equal_i(rx[0], 0xed);
  cl_assert_equal_i(rx[1], 0xcb);
}

void test_spi_stm32__another_device_changes_the_settings(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi1.dev), 0);
  const uint8_t b = 0x5a;
  cl_assert_equal_i(pbl_spi_write(&s_dev_5mhz, &b, 1), 0);
  s_log[0] = '\0';
  cl_assert_equal_i(pbl_spi_write(&s_dev_mode3, &b, 1), 0);
  cl_assert_equal_s(s_log,
                    "clk spi1 on\n"
                    "spi1 00 = 0387\n"
                    "spi1 00 = 03c7\n"
                    "spi1 5a\n"
                    "clk spi1 off\n");
  s_log[0] = '\0';
  cl_assert_equal_i(pbl_spi_write(&s_dev_slow, &b, 1), 0);
  cl_assert_equal_s(s_log,
                    "clk spi1 on\n"
                    "spi1 00 = 033d\n"
                    "spi1 00 = 037d\n"
                    "spi1 5a\n"
                    "clk spi1 off\n");
}

void test_spi_stm32__divider_from_the_bus_clock(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi2.dev), 0);
  const uint8_t b = 0x01;
  cl_assert_equal_i(pbl_spi_write(&s_dev_spi2, &b, 1), 0);
  cl_assert_equal_s(s_log,
                    "clk spi2 on\n"
                    "spi2 00 = 0326\n"
                    "spi2 00 = 0366\n"
                    "spi2 01\n"
                    "clk spi2 off\n");
}

void test_spi_stm32__waits_for_the_last_bit(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi1.dev), 0);
  const uint8_t b = 0x01;
  cl_assert_equal_i(pbl_spi_write(&s_dev_5mhz, &b, 1), 0);
  s_busy_reads = 3;
  cl_assert_equal_i(pbl_spi_write(&s_dev_5mhz, &b, 1), 0);
  cl_assert_equal_i(s_busy_reads, 0);
}

void test_spi_stm32__a_stuck_busy_flag_ends(void) {
  cl_assert_equal_i(pbl_device_init(&dt_spi1.dev), 0);
  const uint8_t b = 0x01;
  s_busy_reads = BUSY_POLLS_MAX + 10;
  cl_assert_equal_i(pbl_spi_write(&s_dev_5mhz, &b, 1), 0);
  cl_assert_equal_i(s_busy_reads, 10);
  s_busy_reads = 0;
}

void test_spi_stm32__zero_maximum_frequency(void) {
  static const struct pbl_spi_dev zero = PBL_SPI_DEV(&dt_spi1, 0, 0, PBL_SPI_MODE_0);
  cl_assert_equal_i(pbl_device_init(&dt_spi1.dev), 0);
  cl_assert_equal_i(pbl_spi_setup(&zero), -EINVAL);
}
