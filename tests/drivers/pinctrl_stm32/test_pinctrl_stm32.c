/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/pinctrl/stm32.c"

#include <stdint.h>
#include <string.h>

#include <pbl/kernel/irq.h>

#include <clar.h>

#define GPIOA_BASE 0x40020000U
#define GPIOG_BASE 0x40021800U
#define NUM_REGS   32

#define MODER   0x00U
#define OTYPER  0x04U
#define OSPEEDR 0x08U
#define PUPDR   0x0cU
#define BSRR    0x18U
#define AFRL    0x20U
#define AFRH    0x24U

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;
static uint32_t s_fill;
static uintptr_t s_writes[NUM_REGS];
static int s_num_writes;

static uint32_t *prv_reg(uintptr_t addr) {
  for (int i = 0; i < s_num_regs; i++) {
    if (s_regs[i].addr == addr) {
      return &s_regs[i].val;
    }
  }
  cl_assert(s_num_regs < NUM_REGS);
  s_regs[s_num_regs].addr = addr;
  s_regs[s_num_regs].val = s_fill;
  return &s_regs[s_num_regs++].val;
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  return *prv_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  *prv_reg(addr) = val;
  if (s_num_writes < NUM_REGS) {
    s_writes[s_num_writes++] = addr;
  }
}

static int prv_write_index(uintptr_t addr) {
  for (int i = 0; i < s_num_writes; i++) {
    if (s_writes[i] == addr) {
      return i;
    }
  }
  return -1;
}

static void prv_reset(uint32_t fill) {
  memset(s_regs, 0, sizeof(s_regs));
  s_num_regs = 0;
  s_fill = fill;
  s_num_writes = 0;
}

static uint32_t prv_field2(uintptr_t base, uint32_t offset, uint8_t pin) {
  return (*prv_reg(base + offset) >> (2U * pin)) & 3U;
}

static uint32_t prv_af(uintptr_t base, uint8_t pin) {
  return (*prv_reg(base + (pin < 8U ? AFRL : AFRH)) >> (4U * (pin % 8U))) & 0xfU;
}

static int s_clk_ret;

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

void pbl_irq_enable(pbl_irq_t irq) {
}

int pbl_clk_enable(const struct pbl_clk *clk) {
  return s_clk_ret;
}

static const uint32_t s_fills[] = {0U, 0xffffffffU};

static int prv_select(const struct pbl_pinctrl_state *state, uint32_t fill) {
  const struct pbl_pinctrl pinctrl = PBL_PINCTRL(state, nullptr, 0);

  prv_reset(fill);
  return pbl_pinctrl_select_state(&pinctrl, PBL_PINCTRL_STATE_DEFAULT);
}

static void prv_assert_pin(uintptr_t base, uint8_t pin, uint32_t mode, uint32_t pull,
                           uint32_t speed, bool open_drain) {
  cl_assert_equal_i(prv_field2(base, MODER, pin), mode);
  cl_assert_equal_i(prv_field2(base, PUPDR, pin), pull);
  cl_assert_equal_i(prv_field2(base, OSPEEDR, pin), speed);
  cl_assert_equal_i((*prv_reg(base + OTYPER) >> pin) & 1U, open_drain ? 1U : 0U);
}

static void prv_assert_others(uintptr_t base, uint32_t pins2, uint32_t pins1) {
  cl_assert_equal_i(*prv_reg(base + MODER) & ~pins2, s_fill & ~pins2);
  cl_assert_equal_i(*prv_reg(base + PUPDR) & ~pins2, s_fill & ~pins2);
  cl_assert_equal_i(*prv_reg(base + OSPEEDR) & ~pins2, s_fill & ~pins2);
  cl_assert_equal_i(*prv_reg(base + OTYPER) & ~pins1, s_fill & ~pins1);
}

void test_pinctrl_stm32__initialize(void) {
  *dt_gpioa.dev.state = (struct pbl_device_state){0};
  *dt_gpiog.dev.state = (struct pbl_device_state){0};
  s_clk_ret = 0;
  prv_reset(0U);
}

void test_pinctrl_stm32__alternate_functions(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fills); i++) {
    cl_assert_equal_i(prv_select(&dt_usart1_pins, s_fills[i]), 0);

    prv_assert_pin(GPIOA_BASE, 9U, 2U, 0U, 2U, false);
    prv_assert_pin(GPIOA_BASE, 10U, 2U, 1U, 2U, false);
    cl_assert_equal_i(prv_af(GPIOA_BASE, 9U), 7U);
    cl_assert_equal_i(prv_af(GPIOA_BASE, 10U), 7U);
    prv_assert_others(GPIOA_BASE, 0xfU << 18, 3U << 9);
    cl_assert_equal_i(*prv_reg(GPIOA_BASE + AFRH) & ~0xff0U, s_fill & ~0xff0U);
  }
}

void test_pinctrl_stm32__low_pins_pull_down_and_speeds(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fills); i++) {
    cl_assert_equal_i(prv_select(&dt_low_pins, s_fills[i]), 0);

    prv_assert_pin(GPIOA_BASE, 3U, 2U, 2U, 1U, false);
    prv_assert_pin(GPIOA_BASE, 2U, 2U, 0U, 3U, false);
    cl_assert_equal_i(prv_af(GPIOA_BASE, 3U), 5U);
    cl_assert_equal_i(prv_af(GPIOA_BASE, 2U), 1U);
    prv_assert_others(GPIOA_BASE, 0xfU << 4, 3U << 2);
    cl_assert_equal_i(*prv_reg(GPIOA_BASE + AFRL) & ~0xff00U, s_fill & ~0xff00U);
  }
}

void test_pinctrl_stm32__open_drain_on_another_port(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fills); i++) {
    cl_assert_equal_i(prv_select(&dt_i2c_pins, s_fills[i]), 0);

    prv_assert_pin(GPIOG_BASE, 13U, 2U, 0U, 0U, true);
    cl_assert_equal_i(prv_af(GPIOG_BASE, 13U), 4U);
    prv_assert_others(GPIOG_BASE, 3U << 26, 1U << 13);
  }
}

void test_pinctrl_stm32__analog_clears_the_pull(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fills); i++) {
    cl_assert_equal_i(prv_select(&dt_adc_pins, s_fills[i]), 0);

    prv_assert_pin(GPIOA_BASE, 4U, 3U, 0U, 0U, false);
    prv_assert_others(GPIOA_BASE, 3U << 8, 1U << 4);
  }
}

void test_pinctrl_stm32__a_reserved_pin_is_left_alone(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fills); i++) {
    cl_assert_equal_i(prv_select(&dt_rsvd_pins, s_fills[i]), 0);

    prv_assert_pin(GPIOA_BASE, 6U, 2U, 0U, 0U, false);
    cl_assert_equal_i(prv_af(GPIOA_BASE, 6U), 2U);
    prv_assert_others(GPIOA_BASE, 3U << 12, 1U << 6);
  }
}

void test_pinctrl_stm32__a_port_the_tree_lacks(void) {
  cl_assert_equal_i(prv_select(&dt_port_k_pins, 0U), -ENODEV);
  cl_assert_equal_i(s_num_regs, 0);
}

void test_pinctrl_stm32__a_port_that_fails_to_start(void) {
  s_clk_ret = -EIO;

  cl_assert_equal_i(prv_select(&dt_usart1_pins, 0U), -ENODEV);
  cl_assert_equal_i(s_num_regs, 0);
}

void test_pinctrl_stm32__a_gpio_pin_is_the_gpio_drivers(void) {
  cl_assert_equal_i(prv_select(&dt_gpio_pins, 0U), -EINVAL);
  cl_assert_equal_i(s_num_regs, 0);
}

void test_pinctrl_stm32__bits_outside_the_cell_are_refused(void) {
  cl_assert_equal_i(prv_select(&dt_bit19_pins, 0U), -EINVAL);
  cl_assert_equal_i(prv_select(&dt_bit24_pins, 0U), -EINVAL);
  cl_assert_equal_i(prv_select(&dt_mode19_pins, 0U), -EINVAL);
  cl_assert_equal_i(s_num_regs, 0);
}

void test_pinctrl_stm32__a_gpio_pin_driven_low(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fills); i++) {
    cl_assert_equal_i(prv_select(&dt_out_low_pins, s_fills[i]), 0);

    prv_assert_pin(GPIOA_BASE, 1U, 1U, 2U, 1U, false);
    prv_assert_others(GPIOA_BASE, 3U << 2, 1U << 1);
    cl_assert_equal_i(*prv_reg(GPIOA_BASE + BSRR), 1U << 17);
    cl_assert(prv_write_index(GPIOA_BASE + BSRR) >= 0);
    cl_assert(prv_write_index(GPIOA_BASE + BSRR) < prv_write_index(GPIOA_BASE + MODER));
  }
}

void test_pinctrl_stm32__a_gpio_pin_driven_high_open_drain(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_fills); i++) {
    cl_assert_equal_i(prv_select(&dt_out_high_pins, s_fills[i]), 0);

    prv_assert_pin(GPIOG_BASE, 13U, 1U, 0U, 0U, true);
    prv_assert_others(GPIOG_BASE, 3U << 26, 1U << 13);
    cl_assert_equal_i(*prv_reg(GPIOG_BASE + BSRR), 1U << 13);
    cl_assert(prv_write_index(GPIOG_BASE + BSRR) < prv_write_index(GPIOG_BASE + MODER));
  }
}

void test_pinctrl_stm32__both_levels_or_a_level_on_a_function_are_refused(void) {
  cl_assert_equal_i(prv_select(&dt_out_both_pins, 0U), -EINVAL);
  cl_assert_equal_i(prv_select(&dt_af_level_pins, 0U), -EINVAL);
  cl_assert_equal_i(s_num_regs, 0);
}
