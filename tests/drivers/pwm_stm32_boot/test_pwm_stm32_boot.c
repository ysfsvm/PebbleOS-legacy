/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/device.h>
#include <pbl/drivers/backlight.h>
#include <pbl/drivers/vibe.h>
#include <pbl/kernel/irq.h>
#include <pbl/services/battery/battery_monitor.h>
#include <pbl/services/battery/battery_state.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <devicetree/devices.h>

#define RCC_BASE    0x40023800U
#define RCC_PLLCFGR 0x04U
#define RCC_CFGR    0x08U
#define GPIOB_BASE  0x40020400U
#define GPIOF_BASE  0x40021400U
#define GPIO_MODER  0x00U
#define GPIO_ODR    0x14U
#define GPIO_BSRR   0x18U
#define NUM_REGS    64

#define PLLCFGR_FIRMWARE ((4U << 24) | (192U << 6) | 16U)
#define CFGR_FIRMWARE    ((4U << 13) | (5U << 10) | (2U << 2) | 2U)

extern const struct pbl_device *const __pbl_devices_start[] PBL_UNSORTED_SECTION_START(pbl_devices);
extern const struct pbl_device *const __pbl_devices_end[] PBL_UNSORTED_SECTION_END(pbl_devices);

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;
static char s_events[256];

static uint32_t *prv_reg(uintptr_t addr) {
  for (int i = 0; i < s_num_regs; i++) {
    if (s_regs[i].addr == addr) {
      return &s_regs[i].val;
    }
  }
  cl_assert(s_num_regs < NUM_REGS);
  s_regs[s_num_regs].addr = addr;
  s_regs[s_num_regs].val = addr == RCC_BASE + RCC_PLLCFGR ? PLLCFGR_FIRMWARE
                           : addr == RCC_BASE + RCC_CFGR  ? CFGR_FIRMWARE
                                                          : 0U;
  return &s_regs[s_num_regs++].val;
}

static const struct {
  uintptr_t port;
  unsigned int pin;
  const char *name;
} s_pins[] = {
  {GPIOF_BASE, 4U, "pf4"},
  {GPIOB_BASE, 8U, "pb8"},
  {GPIOB_BASE, 14U, "pb14"},
};

uint32_t fake_mmio_readl(uintptr_t addr) {
  return *prv_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  static const char *const modes[] = {"in", "out", "af", "an"};
  uint32_t *reg = prv_reg(addr);
  const uint32_t before = *reg;

  *reg = val;
  for (size_t i = 0; i < ARRAY_LENGTH(s_pins); i++) {
    const unsigned int shift = 2U * s_pins[i].pin;

    if (addr == s_pins[i].port + GPIO_BSRR) {
      uint32_t *odr = prv_reg(s_pins[i].port + GPIO_ODR);

      *odr = (*odr | (val & 0xffffU)) & ~(val >> 16);
    }
    if (addr == s_pins[i].port + GPIO_MODER && ((before ^ val) >> shift) & 3U) {
      const size_t len = strlen(s_events);
      const bool high = *prv_reg(s_pins[i].port + GPIO_ODR) & (1U << s_pins[i].pin);

      snprintf(&s_events[len], sizeof(s_events) - len, "%s %s %s\n", s_pins[i].name,
               modes[(val >> shift) & 3U], high ? "high" : "low");
    }
  }
}

void pbl_irq_enable(pbl_irq_t irq) {
}

void pbl_irq_disable(pbl_irq_t irq) {
}

void soc_stm32_stop_block(void) {
}

void soc_stm32_stop_release(void) {
}

bool battery_monitor_critical_lockout(void) {
  return false;
}

BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){0};
}

uint16_t battery_state_get_voltage(void) {
  return 3800U;
}

uint16_t pmic_get_vsys(void) {
  return 4200U;
}

static int prv_table_index(const struct pbl_device *dev) {
  for (const struct pbl_device *const *d = __pbl_devices_start; d < __pbl_devices_end; d++) {
    if (*d == dev) {
      return (int)(d - __pbl_devices_start);
    }
  }
  return -1;
}

void test_pwm_stm32_boot__initialize(void) {
  memset(s_regs, 0, sizeof(s_regs));
  s_num_regs = 0;
  s_events[0] = '\0';
}

void test_pwm_stm32_boot__the_backlight_comes_before_port_f_in_the_table(void) {
  cl_assert(prv_table_index(&dt_backlight.dev) >= 0);
  cl_assert(prv_table_index(&dt_backlight.dev) < prv_table_index(&dt_gpiof.dev));
}

void test_pwm_stm32_boot__the_vibe_enable_goes_low_before_the_timer_outputs(void) {
  cl_assert_equal_i(pbl_device_init_all(), 0);

  cl_assert_equal_s(s_events,
                    "pf4 out low\n"
                    "pb14 out low\n"
                    "pb8 out low\n");
}
