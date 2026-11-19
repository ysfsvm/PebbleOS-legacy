/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/clk/stm32f4.c"

#include <stdint.h>
#include <string.h>

#include <clar.h>
#include <dt-bindings/mfd/stm32f4-rcc.h>

// Registers
////////////////////////////////////////////////////////////////////////////////

#define RCC_BASE 0x40023800U

static uint32_t s_cfgr;
static uint32_t s_pllcfgr;
static uint32_t s_dckcfgr;

uint32_t fake_mmio_readl(uintptr_t addr) {
  switch (addr - RCC_BASE) {
    case STM32F4_RCC_CFGR_OFFSET:
      return s_cfgr;
    case STM32F4_RCC_PLLCFGR_OFFSET:
      return s_pllcfgr;
    case STM32F4_RCC_DCKCFGR_OFFSET:
      return s_dckcfgr;
    default:
      cl_fail("unexpected register read");
      return 0U;
  }
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  cl_fail("unexpected register write");
}

// Kernel
////////////////////////////////////////////////////////////////////////////////

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

// Tests
////////////////////////////////////////////////////////////////////////////////

// CFGR prescaler fields
#define PPRE_DIV2 4U
#define PPRE_DIV4 5U
#define PPRE_DIV8 6U

//! In Hz.
static uint32_t prv_rate(uint32_t index) {
  const struct pbl_clk clk = PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, index));
  uint64_t rate_mhz = 0U;

  cl_assert_equal_i(pbl_clk_get_rate(&clk, &rate_mhz), 0);
  cl_assert_equal_i(rate_mhz % 1000U, 0);
  return (uint32_t)(rate_mhz / 1000U);
}

static void prv_set_apb(uint32_t ppre1, uint32_t ppre2) {
  s_cfgr = PBL_FIELD_PREP(STM32F4_RCC_CFGR_SWS_MASK, STM32F4_RCC_CFGR_SWS_PLL) |
           PBL_FIELD_PREP(STM32F4_RCC_CFGR_PPRE1_MASK, ppre1) |
           PBL_FIELD_PREP(STM32F4_RCC_CFGR_PPRE2_MASK, ppre2);
}

void test_clk_stm32f4__initialize(void) {
  // Snowy: 96 MHz from the HSI (M 16, N 192, P 2), APB1 at /4, APB2 at /2
  s_pllcfgr = PBL_FIELD_PREP(STM32F4_RCC_PLLCFGR_PLLM_MASK, 16U) |
              PBL_FIELD_PREP(STM32F4_RCC_PLLCFGR_PLLN_MASK, 192U);
  s_dckcfgr = 0U;
  prv_set_apb(PPRE_DIV4, PPRE_DIV2);
  cl_assert_equal_i(pbl_device_init(&dt_rcc.dev), 0);
}

void test_clk_stm32f4__a_gate_runs_at_its_bus(void) {
  cl_assert_equal_i(prv_rate(STM32F4_AHB1_CLOCK(GPIOA)), 96000000);
  cl_assert_equal_i(prv_rate(STM32F4_APB1_CLOCK(UART2)), 24000000);
  cl_assert_equal_i(prv_rate(STM32F4_APB2_CLOCK(SPI1)), 48000000);
}

//! A divided APB clocks its timers at twice its rate.
void test_clk_stm32f4__a_timer_runs_at_twice_a_divided_bus(void) {
  cl_assert_equal_i(prv_rate(STM32F4_APB1_CLOCK(TIM4)), 48000000);
  cl_assert_equal_i(prv_rate(STM32F4_APB1_CLOCK(TIM12)), 48000000);
  cl_assert_equal_i(prv_rate(STM32F4_APB2_CLOCK(TIM1)), 96000000);
  cl_assert_equal_i(prv_rate(STM32F4_APB2_CLOCK(TIM9)), 96000000);
}

void test_clk_stm32f4__a_timer_runs_at_an_undivided_bus(void) {
  prv_set_apb(0U, 0U);
  cl_assert_equal_i(prv_rate(STM32F4_APB1_CLOCK(TIM4)), 96000000);
  cl_assert_equal_i(prv_rate(STM32F4_APB2_CLOCK(TIM1)), 96000000);
}

//! With TIMPRE: HCLK up to a divider of 4, four times the bus rate above.
void test_clk_stm32f4__timpre(void) {
  s_dckcfgr = STM32F4_RCC_DCKCFGR_TIMPRE;
  cl_assert_equal_i(prv_rate(STM32F4_APB1_CLOCK(TIM4)), 96000000);

  prv_set_apb(PPRE_DIV8, PPRE_DIV2);
  cl_assert_equal_i(prv_rate(STM32F4_APB1_CLOCK(TIM4)), 48000000);
  cl_assert_equal_i(prv_rate(STM32F4_APB1_CLOCK(UART2)), 12000000);
}
