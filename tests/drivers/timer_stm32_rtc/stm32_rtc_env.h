/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "stm32_rtc_model.h"

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/timer.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/work.h>
#include <pbl/soc/stm32/clock.h>

PBL_DEVICE_CLASS_DEFINE(pbl_clocksource_class, "clocksource");
PBL_DEVICE_CLASS_DEFINE(pbl_clockevent_class, "clockevent");

static int s_locks;
static int s_handler_calls;
static int s_work_calls;
static uint32_t s_cs_hz;
static uint32_t s_ce_hz;
static uint32_t s_apb1_clocks;
static uint32_t s_busy_us;
static bool s_irq_enabled[64];

void pbl_irq_lock(void) {
  s_locks++;
}

void pbl_irq_unlock(void) {
  cl_assert(s_locks > 0);
  s_locks--;
}

void pbl_irq_enable(pbl_irq_t irq) {
  s_irq_enabled[irq] = true;
}

void pbl_irq_set_pending(pbl_irq_t irq) {
  s_irq_pending[irq] = true;
}

void pbl_irq_clear_pending(pbl_irq_t irq) {
  s_irq_pending[irq] = false;
}

void pbl_busy_wait_us(uint32_t us) {
  s_busy_us += us;
  m_advance(us / 61U);
}

bool pbl_sys_work_call_from_isr(pbl_work_call_fn_t fn, void *data) {
  s_work_calls++;
  return true;
}

void soc_stm32_clock_set(enum soc_stm32_bus bus, uint32_t mask, bool enable) {
  cl_assert_equal_i(bus, SOC_STM32_APB1);
  cl_assert(enable);
  s_apb1_clocks |= mask;
  s_m.apb1enr |= mask;
}

void pbl_clocksource_set_hz(const struct pbl_clocksource *cs, uint32_t hz) {
  s_cs_hz = hz;
}

void pbl_clockevent_set_hz(const struct pbl_clockevent *ce, uint32_t hz) {
  s_ce_hz = hz;
}

static void prv_on_event(void);

void pbl_clockevent_handler(const struct pbl_clockevent *ce) {
  cl_assert(ce == pbl_system_clockevent);
  s_handler_calls++;
  prv_on_event();
}

void pbl_isr_3(void);
void pbl_isr_41(void);
