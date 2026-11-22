/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "s29vs_model.h"
#include "traces.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/cmsis.h>

#include <clar.h>

#define HCLK_HZ 64000000U

static int s_irq_locks;
static uint32_t s_dr11;
static bool s_fmc_clock;

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

void pbl_busy_wait_us(uint32_t us) {
  m_trace("wait %uus", (unsigned int)us);
}

void pbl_sleep_ms(uint32_t ms) {
  m_trace("sleep %ums", (unsigned int)ms);
}

void soc_stm32_clock_set(enum soc_stm32_bus bus, uint32_t mask, bool enable) {
  m_trace("clk %d %08x %s", (int)bus, mask, enable ? "on" : "off");
  if (bus == SOC_STM32_AHB3 && (mask & 1U) != 0) {
    s_fmc_clock = enable;
  }
}

void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value) {
  cl_assert_equal_i(reg_id, 11);
  m_trace("dr11 = %08x", value);
  s_dr11 = value;
}

uint32_t RTC_ReadBackupRegister(uint32_t reg_id) {
  cl_assert_equal_i(reg_id, 11);
  m_trace("dr11 -> %08x", s_dr11);
  return s_dr11;
}

PBL_DEVICE_CLASS_DEFINE(pbl_flash_class, "flash");

int pbl_flash_dev_init(const struct pbl_device *dev) {
  return 0;
}

// Standard Peripheral Library, on registers in memory
////////////////////////////////////////////////////////////////////////////////

GPIO_TypeDef fake_gpio[11];

void RCC_GetClocksFreq(RCC_ClocksTypeDef *clocks) {
  clocks->SYSCLK_Frequency = HCLK_HZ;
  clocks->HCLK_Frequency = HCLK_HZ;
  clocks->PCLK1_Frequency = HCLK_HZ / 2U;
  clocks->PCLK2_Frequency = HCLK_HZ;
}

void RCC_AHB1PeriphResetCmd(uint32_t periph, FunctionalState state) {
}

#undef GPIO_WriteBit

void GPIO_WriteBit(GPIO_TypeDef *gpio, uint16_t pin, BitAction val) {
  const bool was = (gpio->ODR & pin) != 0;
  st_GPIO_WriteBit(gpio, pin, val);
  if (val != Bit_RESET) {
    gpio->ODR |= pin;
  } else {
    gpio->ODR &= ~(uint32_t)pin;
  }
  cl_assert(gpio == GPIOD && pin == GPIO_Pin_2);
  m_trace("rst %d", val != Bit_RESET);
  if (!was && val != Bit_RESET) {
    m_hw_reset();
  }
}

#include "../../../drivers/flash/s29vs.c"

#define REG_PATTERN 0xA5A5A5A5U

static uint32_t s_fmc_bcr1;
static uint32_t s_fmc_btr1;
static uint32_t s_fmc_bwtr1;

static void prv_regs_reset(void) {
  for (int i = 0; i < 11; i++) {
    fake_gpio[i] = (GPIO_TypeDef){
      .MODER = REG_PATTERN,
      .OTYPER = REG_PATTERN & 0xFFFFU,
      .OSPEEDR = REG_PATTERN,
      .PUPDR = REG_PATTERN,
      .ODR = REG_PATTERN & 0xFFFFU,
      .AFR = {REG_PATTERN, REG_PATTERN},
    };
  }
  s_fmc_bcr1 = REG_PATTERN;
  s_fmc_btr1 = REG_PATTERN;
  s_fmc_bwtr1 = REG_PATTERN;
  s_fmc_clock = false;
}

static bool prv_fmc_pin(const GPIO_TypeDef *gpio, unsigned int pin) {
  return ((gpio->MODER >> (2U * pin)) & 3U) == 2U &&
         ((gpio->AFR[pin / 8U] >> (4U * (pin % 8U))) & 0xFU) == 12U;
}

static void m_bus_check(uint32_t off) {
  bool pins = prv_fmc_pin(GPIOB, 7);
  for (unsigned int pin = 0; pin < 16U; pin++) {
    pins = pins && (pin == 2U || prv_fmc_pin(GPIOD, pin)) && (pin < 2U || prv_fmc_pin(GPIOE, pin));
  }
  const bool reset_high = ((GPIOD->MODER >> 4) & 3U) == 1U && (GPIOD->ODR & GPIO_Pin_2) != 0;
  const bool bank = (s_fmc_bcr1 & STM32F4_FMC_BCR_MBKEN) != 0;
  if (!s_fmc_clock || !bank || !pins || !reset_high) {
    m_violation("bus access at %06x: clock %d bank %d pins %d RESET# %d", off, s_fmc_clock, bank,
                pins, reset_high);
  }
}

static void prv_trace_gpio(const char *name, const GPIO_TypeDef *gpio) {
  m_trace("%s MODER %08x OTYPER %08x OSPEEDR %08x PUPDR %08x", name, gpio->MODER, gpio->OTYPER,
          gpio->OSPEEDR, gpio->PUPDR);
  m_trace("%s ODR %08x AFRL %08x AFRH %08x", name, gpio->ODR, gpio->AFR[0], gpio->AFR[1]);
}

static void prv_trace_regs(void) {
  m_trace("FMC BCR1 %08x BTR1 %08x BWTR1 %08x", s_fmc_bcr1, s_fmc_btr1, s_fmc_bwtr1);
  prv_trace_gpio("GPIOB", GPIOB);
  prv_trace_gpio("GPIOD", GPIOD);
  prv_trace_gpio("GPIOE", GPIOE);
}

static uint32_t *prv_reg(uintptr_t addr) {
  switch (addr) {
    case 0xA0000000U:
      return &s_fmc_bcr1;
    case 0xA0000004U:
      return &s_fmc_btr1;
    case 0xA0000104U:
      return &s_fmc_bwtr1;
    default:
      cl_fail("unexpected register access");
      return NULL;
  }
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  return *prv_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  uint32_t *reg = prv_reg(addr);
  if (reg == &s_fmc_bcr1 && ((*reg ^ val) & 1U) != 0) {
    m_trace("fmc bank %s", (val & 1U) ? "on" : "off");
  }
  *reg = val;
}

static const struct pbl_flash *const s_flash = &pbl_flash_s29vs;

static void prv_dump_trace(const char *name, const char *trace) {
  const char *path = getenv("S29VS_TRACE_OUT");
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

static void prv_flash_init(void) {
  cl_assert_equal_i(prv_init(s_flash), 0);
}

static void prv_fill(uint32_t addr, uint32_t len, uint8_t seed) {
  uint8_t *mem = (uint8_t *)m.mem;
  for (uint32_t i = 0; i < len; i++) {
    mem[addr + i] = (uint8_t)(seed + i * 7U);
  }
}

static void prv_wait_erase(void) {
  int rc;
  int polls = 0;
  while ((rc = prv_erase_status(s_flash)) == -EBUSY) {
    cl_assert(++polls < 100);
  }
  cl_assert_equal_i(rc, 0);
}

static bool prv_is_erased(uint32_t addr, uint32_t len) {
  const uint8_t *mem = (const uint8_t *)m.mem;
  for (uint32_t i = 0; i < len; i++) {
    if (mem[addr + i] != 0xFF) {
      return false;
    }
  }
  return true;
}

void test_flash_s29vs__initialize(void) {
  m_power_on();
  prv_regs_reset();
  pbl_flash_s29vs_flash_state.coredump = false;
  s_dr11 = 0;
  s_irq_locks = 0;
  s_trace_on = false;
}

void test_flash_s29vs__cleanup(void) {
  cl_assert_equal_s(m.violation, "");
  cl_assert_equal_i(m.violations, 0);
  cl_assert_equal_i(s_irq_locks, 0);
}

void test_flash_s29vs__init(void) {
  m_trace_start();
  prv_flash_init();
  prv_trace_regs();
  CHECK_TRACE(TRACE_INIT);
  cl_assert(m.range_set);
  cl_assert_equal_i(m.range_lo, 0x200000);
  cl_assert_equal_i(m.range_hi, 0x2E0000);
  cl_assert_equal_i(m.cr, NOR_CR_RESET);
}

void test_flash_s29vs__init_in_coredump_leaves_erase_record(void) {
  pbl_flash_s29vs_flash_state.coredump = true;
  s_dr11 = 0x80400000U;
  m_trace_start();
  prv_flash_init();
  CHECK_TRACE(TRACE_INIT_COREDUMP);
  cl_assert_equal_i(s_dr11, 0x80400000U);
}

void test_flash_s29vs__init_finishes_interrupted_erase(void) {
  prv_fill(0x420000, 0x20000, 1);
  s_dr11 = 0x80420000U;
  m_trace_start();
  prv_flash_init();
  CHECK_TRACE(TRACE_INIT_REPAIR);
  cl_assert(prv_is_erased(0x420000, 0x20000));
  cl_assert_equal_i(s_dr11, 0);
}

void test_flash_s29vs__init_ignores_guarded_and_out_of_range_record(void) {
  prv_fill(0x200000, 0x20000, 1);
  s_dr11 = 0x80200000U;
  m_trace_start();
  prv_flash_init();
  s_dr11 = 0x81000000U;
  prv_flash_init();
  CHECK_TRACE(TRACE_INIT_IGNORED);
  cl_assert(!prv_is_erased(0x200000, 0x20000));
  cl_assert_equal_i(s_dr11, 0);
}

void test_flash_s29vs__init_ignores_record_without_progress_bit(void) {
  s_dr11 = 0x0800ABCDU;
  m_trace_start();
  prv_flash_init();
  CHECK_TRACE(TRACE_INIT_NO_PROGRESS);
  cl_assert_equal_i(s_dr11, 0x0800ABCDU);
}

void test_flash_s29vs__read_unaligned(void) {
  prv_flash_init();
  prv_fill(0x10000, 0x20, 3);
  uint8_t buf[16];
  m_trace_start();
  cl_assert_equal_i(prv_read(s_flash, 0x10001, buf, 7), 0);
  cl_assert_equal_m(buf, (uint8_t *)m.mem + 0x10001, 7);
  cl_assert_equal_i(prv_read(s_flash, 0x10002, buf + 1, 6), 0);
  cl_assert_equal_m(buf + 1, (uint8_t *)m.mem + 0x10002, 6);
  cl_assert_equal_i(prv_read(s_flash, 0x10004, buf, 5), 0);
  cl_assert_equal_m(buf, (uint8_t *)m.mem + 0x10004, 5);
  CHECK_TRACE(TRACE_READ);
}

void test_flash_s29vs__write_aligned_across_sector(void) {
  prv_flash_init();
  uint8_t data[0x80];
  for (unsigned int i = 0; i < sizeof(data); i++) {
    data[i] = (uint8_t)(0x30 + i);
  }
  m_trace_start();
  cl_assert_equal_i(prv_write(s_flash, 0x5FFC0, data, sizeof(data)), 0);
  CHECK_TRACE(TRACE_WRITE_ALIGNED);
  cl_assert_equal_m((uint8_t *)m.mem + 0x5FFC0, data, sizeof(data));
}

void test_flash_s29vs__write_unaligned_across_page_and_sector(void) {
  prv_flash_init();
  uint8_t data[0x42];
  for (unsigned int i = 0; i < sizeof(data); i++) {
    data[i] = (uint8_t)(0x80 + i);
  }
  m_trace_start();
  cl_assert_equal_i(prv_write(s_flash, 0x7FE1, data, 0x41), 0);
  cl_assert_equal_i(prv_write(s_flash, 0x5FFFE, data, 3), 0);
  CHECK_TRACE(TRACE_WRITE_UNALIGNED);
  cl_assert_equal_m((uint8_t *)m.mem + 0x7FE1, data, 0x41);
  cl_assert_equal_i(((uint8_t *)m.mem)[0x7FE0], 0xFF);
  cl_assert_equal_i(((uint8_t *)m.mem)[0x8022], 0xFF);
  cl_assert_equal_m((uint8_t *)m.mem + 0x5FFFE, data, 3);
  cl_assert_equal_i(((uint8_t *)m.mem)[0x60001], 0xFF);
}

void test_flash_s29vs__write_across_pages(void) {
  prv_flash_init();
  uint8_t data[0x90];
  for (unsigned int i = 0; i < sizeof(data); i++) {
    data[i] = (uint8_t)(0x11 + i);
  }
  m_trace_start();
  cl_assert_equal_i(prv_write(s_flash, 0x400030, data, sizeof(data)), 0);
  CHECK_TRACE(TRACE_WRITE_PAGES);
  cl_assert_equal_m((uint8_t *)m.mem + 0x400030, data, sizeof(data));
}

void test_flash_s29vs__erase_sector(void) {
  prv_flash_init();
  prv_fill(0x400000, 0x20000, 5);
  m_trace_start();
  cl_assert_equal_i(prv_erase(s_flash, 0x400000, 0x20000), 0);
  prv_wait_erase();
  CHECK_TRACE(TRACE_ERASE_SECTOR);
  cl_assert(prv_is_erased(0x400000, 0x20000));
  cl_assert_equal_i(s_dr11, 0);
}

void test_flash_s29vs__erase_boot_sectors(void) {
  prv_flash_init();
  prv_fill(0x0, 0x28000, 5);
  m_trace_start();
  cl_assert_equal_i(prv_erase(s_flash, 0x0, 0x20000), 1);
  CHECK_TRACE(TRACE_ERASE_BOOT);
  cl_assert(prv_is_erased(0x0, 0x20000));
  cl_assert(!prv_is_erased(0x20000, 0x8000));
}

void test_flash_s29vs__erase_one_boot_sector(void) {
  prv_flash_init();
  prv_fill(0x0, 0x20000, 5);
  m_trace_start();
  cl_assert_equal_i(prv_erase(s_flash, 0x8000, 0x8000), 1);
  CHECK_TRACE(TRACE_ERASE_BOOT_ONE);
  cl_assert(!prv_is_erased(0x0, 0x8000));
  cl_assert(prv_is_erased(0x8000, 0x8000));
  cl_assert(!prv_is_erased(0x10000, 0x8000));
}

void test_flash_s29vs__erase_small_unit_above_boot_sectors(void) {
  prv_flash_init();
  m_trace_start();
  cl_assert_equal_i(prv_erase(s_flash, 0x20000, 0x8000), -EINVAL);
  CHECK_TRACE(TRACE_EMPTY);
}

void test_flash_s29vs__erase_suspended_by_read(void) {
  prv_flash_init();
  prv_fill(0x400000, 0x20000, 5);
  prv_fill(0x600000, 0x10, 9);
  uint8_t buf[4];
  m.erase_busy_reads = 6;
  m_trace_start();
  cl_assert_equal_i(prv_erase(s_flash, 0x400000, 0x20000), 0);
  cl_assert_equal_i(prv_erase_status(s_flash), -EBUSY);
  cl_assert_equal_i(prv_erase_suspend(s_flash), 0);
  cl_assert_equal_i(prv_erase_status(s_flash), -EAGAIN);
  cl_assert_equal_i(prv_read(s_flash, 0x600000, buf, sizeof(buf)), 0);
  cl_assert_equal_i(prv_erase_resume(s_flash), 0);
  prv_wait_erase();
  CHECK_TRACE(TRACE_ERASE_SUSPEND);
  cl_assert_equal_m(buf, (uint8_t *)m.mem + 0x600000, sizeof(buf));
  cl_assert(prv_is_erased(0x400000, 0x20000));
}

void test_flash_s29vs__erase_suspend_after_erase_ended(void) {
  prv_flash_init();
  m.erase_busy_reads = 1;
  m_trace_start();
  cl_assert_equal_i(prv_erase(s_flash, 0x400000, 0x20000), 0);
  cl_assert_equal_i(prv_erase_suspend(s_flash), 1);
  cl_assert_equal_i(prv_erase_resume(s_flash), -EACCES);
  CHECK_TRACE(TRACE_ERASE_SUSPEND_LATE);
}

void test_flash_s29vs__guarded_ranges_refused_without_bus_access(void) {
  prv_flash_init();
  const uint8_t data[2] = {0, 0};
  m_trace_start();
  cl_assert_equal_i(prv_write(s_flash, 0x200000, data, 2), -EACCES);
  cl_assert_equal_i(prv_write(s_flash, 0x2FFFFE, data, 2), -EACCES);
  cl_assert_equal_i(prv_write(s_flash, 0xC0000, data, 2), -EACCES);
  cl_assert_equal_i(prv_write(s_flash, 0xE0000, data, 2), -EACCES);
  cl_assert_equal_i(prv_write(s_flash, 0xBFFFF, data, 2), -EACCES);
  cl_assert_equal_i(prv_write(s_flash, 0xFFFFF, data, 2), -EACCES);
  cl_assert_equal_i(prv_erase(s_flash, 0x200000, 0x20000), -EACCES);
  cl_assert_equal_i(prv_erase(s_flash, 0xC0000, 0x20000), -EACCES);
  cl_assert_equal_i(prv_erase(s_flash, 0xE0000, 0x20000), -EACCES);
  cl_assert_equal_i(prv_erase(s_flash, 0x2E0000, 0x20000), -EACCES);
  CHECK_TRACE(TRACE_EMPTY);
}

void test_flash_s29vs__locked_range_refuses_erase_in_chip(void) {
  prv_flash_init();
  prv_fill(0x2E0000, 0x20000, 5);
  m_trace_start();
  cl_assert_equal_i(flash_impl_erase_sector_begin(0x2E0000), E_INVALID_OPERATION);
  CHECK_TRACE(TRACE_LOCKED);
  cl_assert(!prv_is_erased(0x2E0000, 0x20000));
}

void test_flash_s29vs__program_error(void) {
  prv_flash_init();
  const uint8_t data[4] = {1, 2, 3, 4};
  m.fail_next_program = true;
  m_trace_start();
  cl_assert_equal_i(prv_write(s_flash, 0x400000, data, sizeof(data)), -EIO);
  CHECK_TRACE(TRACE_PROGRAM_ERROR);
}

void test_flash_s29vs__erase_error(void) {
  prv_flash_init();
  m.fail_next_erase = true;
  m_trace_start();
  cl_assert_equal_i(prv_erase(s_flash, 0x400000, 0x20000), 0);
  int rc;
  while ((rc = prv_erase_status(s_flash)) == -EBUSY) {
  }
  cl_assert_equal_i(rc, -EIO);
  CHECK_TRACE(TRACE_ERASE_ERROR);
  cl_assert_equal_i(s_dr11, 0);
}

void test_flash_s29vs__boot_sector_erase_retried(void) {
  prv_flash_init();
  prv_fill(0x0, 0x8000, 5);
  m.fail_next_erase = true;
  m_trace_start();
  cl_assert_equal_i(prv_erase(s_flash, 0x0, 0x8000), 1);
  CHECK_TRACE(TRACE_ERASE_RETRY);
  cl_assert(prv_is_erased(0x0, 0x8000));
}

static void prv_in_stop(void *arg) {
  prv_trace_regs();
}

void test_flash_s29vs__power_down(void) {
  prv_flash_init();
  m_trace_start();
  prv_power_down(s_flash, prv_in_stop, NULL);
  prv_trace_regs();
  s29vs_use();
  prv_power_down(s_flash, prv_in_stop, NULL);
  prv_trace_regs();
  s29vs_release_many(1);
  CHECK_TRACE(TRACE_POWER_DOWN);
}
