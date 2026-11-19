/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <clar.h>

// The vectors of the STM32F4 the driver connects
#define PBL_SOC_IRQN_EXTI0     6
#define PBL_SOC_IRQN_EXTI1     7
#define PBL_SOC_IRQN_EXTI2     8
#define PBL_SOC_IRQN_EXTI3     9
#define PBL_SOC_IRQN_EXTI4     10
#define PBL_SOC_IRQN_EXTI9_5   23
#define PBL_SOC_IRQN_EXTI15_10 40

#include "../../../drivers/gpio/stm32.c"

// Registers
////////////////////////////////////////////////////////////////////////////////
// The EXTI as the reference manual has it: an edge sets a line's pending bit only while the line
// is unmasked, writing 1 clears a pending bit, and a vector is requested while any of its lines
// is pending, masked or not.

#define EXTI_BASE   0x40013c00U
#define SYSCFG_BASE 0x40013800U
#define NUM_REGS    64

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;

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

uint32_t fake_mmio_readl(uintptr_t addr) {
  return *prv_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  if (addr == EXTI_BASE + STM32F4_EXTI_PR_OFFSET) {
    *prv_reg(addr) &= ~val;
  } else {
    *prv_reg(addr) = val;
  }
}

static uint32_t prv_exti(uint32_t offset) {
  return *prv_reg(EXTI_BASE + offset);
}

static void prv_edge(uint32_t line, bool rising) {
  uint32_t trigger = prv_exti(rising ? STM32F4_EXTI_RTSR_OFFSET : STM32F4_EXTI_FTSR_OFFSET);

  if ((prv_exti(STM32F4_EXTI_IMR_OFFSET) & trigger & PBL_BIT(line)) != 0U) {
    *prv_reg(EXTI_BASE + STM32F4_EXTI_PR_OFFSET) |= PBL_BIT(line);
  }
}

void pbl_isr_40(void);

#define MAX_ENTRIES 100

//! Take EXTI15_10 as the NVIC does: again for as long as the request stays. Returns the entries.
static int prv_run_exti15_10(void) {
  int entries = 0;

  while ((prv_exti(STM32F4_EXTI_PR_OFFSET) & 0xfc00U) != 0U && entries < MAX_ENTRIES) {
    pbl_isr_40();
    entries++;
  }
  return entries;
}

// Kernel
////////////////////////////////////////////////////////////////////////////////

static int s_lock_depth;
static bool s_nvic_enabled[64];

void pbl_irq_lock(void) {
  s_lock_depth++;
}

void pbl_irq_unlock(void) {
  s_lock_depth--;
}

void pbl_irq_enable(pbl_irq_t irq) {
  s_nvic_enabled[irq] = true;
}

int pbl_clk_enable(const struct pbl_clk *clk) {
  return 0;
}

// Tests
////////////////////////////////////////////////////////////////////////////////

static struct pbl_irq_spec s_cts;
static int s_handler_calls;
static int s_second_edge;

void test_gpio_stm32__initialize(void) {
  memset(s_regs, 0, sizeof(s_regs));
  memset(s_nvic_enabled, 0, sizeof(s_nvic_enabled));
  s_num_regs = 0;
  s_handler_calls = 0;
  s_second_edge = 0;
  cl_assert_equal_i(pbl_device_init(&dt_gpioa_intc.dev), 0);
}

void test_gpio_stm32__cleanup(void) {
  (void)pbl_intc_free(&s_cts);
}

static void prv_request_cts(pbl_intc_handler_t handler) {
  static const struct pbl_gpio s_pin = PBL_GPIO(&dt_gpioa, 11, 0);

  cl_assert_equal_i(pbl_gpio_to_irq(&s_pin, PBL_IRQ_TYPE_EDGE_BOTH, &s_cts), 0);
  cl_assert_equal_i(pbl_intc_request(&s_cts, handler, nullptr), 0);
  cl_assert_equal_i(pbl_intc_enable(&s_cts), 0);
}

static void prv_count(void *arg) {
  s_handler_calls++;
}

void test_gpio_stm32__routes_and_triggers(void) {
  static const struct pbl_gpio s_pin = PBL_GPIO(&dt_gpiog, 10, 0);
  struct pbl_irq_spec irq;

  cl_assert_equal_i(pbl_device_init(&dt_gpiog_intc.dev), 0);
  cl_assert_equal_i(pbl_gpio_to_irq(&s_pin, PBL_IRQ_TYPE_EDGE_FALLING, &irq), 0);
  cl_assert_equal_i(pbl_intc_request(&irq, prv_count, nullptr), 0);

  // Port G (6) on line 10, in EXTICR3
  cl_assert_equal_i(*prv_reg(SYSCFG_BASE + 0x10U) & 0x0f00U, 0x0600U);
  cl_assert_equal_i(prv_exti(STM32F4_EXTI_FTSR_OFFSET), PBL_BIT(10));
  cl_assert_equal_i(prv_exti(STM32F4_EXTI_RTSR_OFFSET), 0U);
  cl_assert_equal_i(prv_exti(STM32F4_EXTI_IMR_OFFSET), 0U);

  cl_assert_equal_i(pbl_intc_enable(&irq), 0);
  cl_assert_equal_i(prv_exti(STM32F4_EXTI_IMR_OFFSET), PBL_BIT(10));
  cl_assert(s_nvic_enabled[PBL_SOC_IRQN_EXTI15_10]);

  cl_assert_equal_i(pbl_intc_free(&irq), 0);
  cl_assert_equal_i(prv_exti(STM32F4_EXTI_IMR_OFFSET), 0U);
}

void test_gpio_stm32__an_edge_calls_the_handler_once(void) {
  prv_request_cts(prv_count);

  prv_edge(11, false);
  cl_assert_equal_i(prv_run_exti15_10(), 1);
  cl_assert_equal_i(s_handler_calls, 1);
  cl_assert_equal_i(prv_exti(STM32F4_EXTI_PR_OFFSET), 0U);
}

//! The CC2564's wake pulse on CTS: its second edge comes while the handler runs, before it masks
//! the line, as when the handler runs late after STOP.
static void prv_wake(void *arg) {
  s_handler_calls++;
  if (s_second_edge++ == 0) {
    prv_edge(11, true);
  }
  (void)pbl_intc_disable(&s_cts);
}

void test_gpio_stm32__a_line_masked_while_pending_is_cleared(void) {
  prv_request_cts(prv_wake);

  prv_edge(11, false);
  // Without clearing the pending bit of the masked line, the request stays and the vector is
  // taken until the watchdog resets the watch.
  cl_assert(prv_run_exti15_10() < MAX_ENTRIES);
  cl_assert_equal_i(s_handler_calls, 1);
  cl_assert_equal_i(prv_exti(STM32F4_EXTI_PR_OFFSET), 0U);
}

void test_gpio_stm32__a_masked_pending_line_is_cleared_without_its_handler(void) {
  prv_request_cts(prv_count);
  cl_assert_equal_i(pbl_intc_disable(&s_cts), 0);

  // A line left pending while masked, however it came to be
  *prv_reg(EXTI_BASE + STM32F4_EXTI_PR_OFFSET) |= PBL_BIT(11);
  cl_assert_equal_i(prv_run_exti15_10(), 1);
  cl_assert_equal_i(s_handler_calls, 0);
  cl_assert_equal_i(prv_exti(STM32F4_EXTI_PR_OFFSET), 0U);
}
