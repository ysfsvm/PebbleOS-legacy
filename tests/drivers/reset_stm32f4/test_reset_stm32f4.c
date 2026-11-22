/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/reset/stm32f4.c"

#include <errno.h>
#include <string.h>

#include <clar.h>
#include <dt-bindings/mfd/stm32f4-rcc.h>

#define RCC_BASE 0x40023800U
#define NUM_REGS 8

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;
static int s_writes;
static int s_lock_depth;

static uint32_t *prv_model_reg(uintptr_t addr) {
  for (int i = 0; i < s_num_regs; i++) {
    if (s_regs[i].addr == addr) {
      return &s_regs[i].val;
    }
  }
  cl_assert(s_num_regs < NUM_REGS);
  s_regs[s_num_regs].addr = addr;
  s_regs[s_num_regs].val = 0x00a0a000U;
  return &s_regs[s_num_regs++].val;
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  return *prv_model_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  cl_assert(s_lock_depth > 0);
  s_writes++;
  *prv_model_reg(addr) = val;
}

void pbl_irq_lock(void) {
  s_lock_depth++;
}

void pbl_irq_unlock(void) {
  s_lock_depth--;
}

static const struct pbl_reset s_spi1 = PBL_RESET(&dt_rcc_reset, STM32F4_APB2_RESET(SPI1));
static const struct pbl_reset s_i2c1 = PBL_RESET(&dt_rcc_reset, STM32F4_APB1_RESET(I2C1));
static const struct pbl_reset s_gpioa = PBL_RESET(&dt_rcc_reset, STM32F4_AHB1_RESET(GPIOA));

void test_reset_stm32f4__initialize(void) {
  memset(s_regs, 0, sizeof(s_regs));
  s_num_regs = 0;
  s_writes = 0;
  s_lock_depth = 0;
  *dt_rcc_reset.dev.state = (struct pbl_device_state){0};
  cl_assert_equal_i(pbl_device_init(&dt_rcc_reset.dev), 0);
}

void test_reset_stm32f4__assert_and_deassert_one_bit(void) {
  cl_assert_equal_i(pbl_reset_assert(&s_spi1), 0);
  cl_assert_equal_i(*prv_model_reg(RCC_BASE + 0x24U), 0x00a0a000U | PBL_BIT(12));
  cl_assert_equal_i(pbl_reset_deassert(&s_spi1), 0);
  cl_assert_equal_i(*prv_model_reg(RCC_BASE + 0x24U), 0x00a0a000U);
  cl_assert_equal_i(s_writes, 2);
  cl_assert_equal_i(s_lock_depth, 0);
}

void test_reset_stm32f4__each_bus_register(void) {
  cl_assert_equal_i(pbl_reset_assert(&s_i2c1), 0);
  cl_assert_equal_i(*prv_model_reg(RCC_BASE + 0x20U), 0x00a0a000U | PBL_BIT(21));
  cl_assert_equal_i(pbl_reset_assert(&s_gpioa), 0);
  cl_assert_equal_i(*prv_model_reg(RCC_BASE + 0x10U), 0x00a0a000U | PBL_BIT(0));
}

void test_reset_stm32f4__pulse(void) {
  cl_assert_equal_i(pbl_reset_reset(&s_spi1), 0);
  cl_assert_equal_i(*prv_model_reg(RCC_BASE + 0x24U), 0x00a0a000U);
  cl_assert_equal_i(s_writes, 2);
}

void test_reset_stm32f4__lines_outside_the_reset_registers(void) {
  const struct pbl_reset below = PBL_RESET(&dt_rcc_reset, 0x7fU);
  const struct pbl_reset gap = PBL_RESET(&dt_rcc_reset, 0x1cU * 8U + 3U);
  const struct pbl_reset above = PBL_RESET(&dt_rcc_reset, 0x28U * 8U);

  cl_assert_equal_i(pbl_reset_assert(&below), -EINVAL);
  cl_assert_equal_i(pbl_reset_assert(&gap), -EINVAL);
  cl_assert_equal_i(pbl_reset_assert(&above), -EINVAL);
  cl_assert_equal_i(s_writes, 0);
}
