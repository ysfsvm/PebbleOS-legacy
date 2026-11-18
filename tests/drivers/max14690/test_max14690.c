/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/pmic/max14690.c"

#include <errno.h>
#include <string.h>

#include <clar.h>

// The MAX14690's init: whatever happens to its interrupt, the PMIC keeps powering the display.

static const struct pbl_i2c_dev s_i2c;
static const struct pbl_max14690 s_pmic = {.i2c = &s_i2c};
const struct pbl_max14690 *const MAX14690 = &s_pmic;

static uint8_t s_regs[0x20];
static int s_irq_ret;
static bool s_line_enabled;

// The bus: a write is [register, value], a read writes the register then reads it
////////////////////////////////////////////////////////////////////////////////

int pbl_i2c_transfer(const struct pbl_i2c_dev *dev, struct pbl_i2c_msg *msgs, uint8_t num_msgs) {
  const uint8_t reg = msgs[0].buf[0];

  cl_assert(reg < sizeof(s_regs));
  if (num_msgs == 1U) {
    s_regs[reg] = msgs[0].buf[1];
  } else {
    msgs[1].buf[0] = s_regs[reg];
  }
  return 0;
}

// The rest of the system
////////////////////////////////////////////////////////////////////////////////

int pbl_gpio_configure(const struct pbl_gpio *gpio, uint32_t flags) {
  return 0;
}

int pbl_gpio_to_irq(const struct pbl_gpio *gpio, uint32_t type, struct pbl_irq_spec *spec) {
  return s_irq_ret;
}

int pbl_device_init(const struct pbl_device *dev) {
  return 0;
}

int pbl_intc_request(const struct pbl_irq_spec *spec, pbl_intc_handler_t handler, void *arg) {
  return 0;
}

int pbl_intc_enable(const struct pbl_irq_spec *spec) {
  s_line_enabled = true;
  return 0;
}

void pbl_adc_stm32_read(uint8_t channel, const struct pbl_gpio *pin, uint32_t *vref_total,
                        uint32_t *vmon_total) {
}

uint32_t pbl_adc_stm32_to_mv(uint32_t vref_total, uint32_t vmon_total, uint32_t numerator,
                             uint32_t denominator) {
  return 0U;
}

void pbl_busy_wait_us(uint32_t us) {
}

void pbl_sleep_ms(uint32_t ms) {
}

bool pbl_delayed_work_reschedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                                 pbl_timeout_t delay) {
  return false;
}

bool pbl_sys_work_call_from_isr(pbl_work_call_fn_t fn, void *data) {
  return false;
}

struct pbl_workq pbl_sys_workq;

int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  return 0;
}

void pbl_mutex_unlock(struct pbl_mutex *m) {
}

bool pbl_in_isr(void) {
  return false;
}

bool pbl_irq_is_locked(void) {
  return false;
}

bool pbl_kernel_is_running(void) {
  return true;
}

void event_put(PebbleEvent *event) {
}

// Tests
////////////////////////////////////////////////////////////////////////////////

#define LDO_ENABLED 0x02U

void test_max14690__initialize(void) {
  memset(s_regs, 0, sizeof(s_regs));
  s_regs[PmicRegisters_CHIP_ID] = MAX14690_CHIP_ID;
  s_irq_ret = 0;
  s_line_enabled = false;
  s_present = false;
}

void test_max14690__init(void) {
  cl_assert_equal_i(pbl_max14690_init(&s_pmic.dev), 0);
  cl_assert(s_line_enabled);
  cl_assert_equal_i(s_regs[PmicRegisters_INT_MASK_A], 0x08);
  cl_assert(s_regs[PmicRegisters_LDO2_CONFIG] & LDO_ENABLED);
}

//! LDO2 powers the display FPGA: without it the screen stays dark.
void test_max14690__the_display_rail_comes_up_without_the_interrupt(void) {
  s_irq_ret = -ENOTSUP;

  cl_assert_equal_i(pbl_max14690_init(&s_pmic.dev), 0);
  cl_assert(!s_line_enabled);
  cl_assert(s_regs[PmicRegisters_LDO2_CONFIG] & LDO_ENABLED);
  cl_assert(s_present);
}
