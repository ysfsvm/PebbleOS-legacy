/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/timer/stm32_rtc.c"
#include "stm32_rtc_env.h"

static uint32_t s_rearm_delta;

static void prv_on_event(void) {
  if (s_rearm_delta != 0U) {
    s_rtc_ce.ops->set_next(&s_rtc_ce, s_rearm_delta);
  }
}

static void prv_reset_driver(void) {
  s_started = false;
  s_offset = 0;
  s_last_now = 0U;
  s_restored_stock = false;
  s_armed = false;
  s_stale_irqs = 0U;
  s_rearmed = 0U;
  s_repended = 0U;
  s_locks = 0;
  s_handler_calls = 0;
  s_work_calls = 0;
  s_rearm_delta = 0U;
  s_busy_us = 0U;
  memset(s_irq_enabled, 0, sizeof(s_irq_enabled));
}

static void prv_init_both(void) {
  cl_assert_equal_i(prv_init(&s_rtc_cs.dev), 0);
  cl_assert_equal_i(prv_init(&s_rtc_ce.dev), 0);
}

static void prv_arm(uint32_t delta) {
  pbl_irq_lock();
  s_rtc_ce.ops->set_next(&s_rtc_ce, delta);
  pbl_irq_unlock();
}

static void prv_run_irq(void) {
  if (s_irq_pending[M_IRQ_WKUP]) {
    s_irq_pending[M_IRQ_WKUP] = false;
    pbl_isr_3();
  }
}

static void prv_event_check(void) {
  pbl_irq_lock();
  stm32_rtc_event_check();
  pbl_irq_unlock();
}

static uint32_t prv_counts_to_event(uint32_t limit) {
  for (uint32_t n = 1U; n <= limit; n++) {
    m_advance(1U);
    if (s_irq_pending[M_IRQ_WKUP]) {
      return n;
    }
  }
  return 0U;
}

void test_timer_stm32_rtc_wut__initialize(void) {
  m_reset_stock();
  prv_reset_driver();
}

void test_timer_stm32_rtc_wut__cleanup(void) {
  cl_assert_equal_i(s_locks, 0);
  cl_assert(s_busy_us < RTC_SYNC_TIMEOUT_US);
}

void test_timer_stm32_rtc_wut__init(void) {
  s_m.cr = M_CR_ALRAE | M_CR_ALRAIE | M_CR_WUTE | M_CR_WUTIE;
  s_m.isr |= M_ISR_ALRAF | M_ISR_WUTF;
  s_m.exti[M_EXTI_PR / 4U] = M_LINE_ALARM | M_LINE_WAKEUP;
  s_m.exti[M_EXTI_IMR / 4U] = 1U << 3;
  s_m.exti[M_EXTI_RTSR / 4U] = 1U << 3;
  s_m.exti[M_EXTI_FTSR / 4U] = (1U << 3) | M_LINE_ALARM | M_LINE_WAKEUP;

  prv_init_both();

  cl_assert_equal_i(s_m.cr & (M_CR_ALRAE | M_CR_ALRAIE | M_CR_WUTE | M_CR_WUTIE), 0);
  cl_assert_equal_i(s_m.isr & (M_ISR_ALRAF | M_ISR_WUTF), 0);
  cl_assert_equal_i(s_m.exti[M_EXTI_PR / 4U], 0);
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_PRER), 0);
  cl_assert_equal_i(m_exti(M_EXTI_IMR), (1U << 3) | M_LINE_WAKEUP);
  cl_assert_equal_i(m_exti(M_EXTI_RTSR), (1U << 3) | M_LINE_WAKEUP);
  cl_assert_equal_i(m_exti(M_EXTI_FTSR), (1U << 3) | M_LINE_ALARM);
  cl_assert(s_irq_enabled[M_IRQ_WKUP]);
  cl_assert(!s_irq_enabled[M_IRQ_ALARM]);
  cl_assert_equal_i(s_cs_hz, 256);
  cl_assert_equal_i(s_ce_hz, 16384);
  cl_assert_equal_i(s_rtc_ce.min_delta, 16);
  cl_assert_equal_i(s_rtc_ce.max_delta, 3600 * 16384);
}

static void prv_check_range(uint32_t delta, uint32_t wucksel, uint32_t wutr) {
  prv_arm(delta);
  cl_assert_equal_i(s_m.cr & M_CR_WUCKSEL, wucksel);
  cl_assert_equal_i(s_m.wutr, wutr);
  cl_assert_equal_i(s_m.cr & (M_CR_WUTE | M_CR_WUTIE), M_CR_WUTE | M_CR_WUTIE);
  cl_assert_equal_i(s_m.wp, 0);
}

void test_timer_stm32_rtc_wut__clock_ranges(void) {
  prv_init_both();

  prv_check_range(16U, 3U, 15U);
  prv_check_range(65536U, 3U, 65535U);
  prv_check_range(65537U, 0U, 8191U);
  prv_check_range(65536U * 8U, 0U, 65535U);
  prv_check_range((65536U * 8U) + 1U, 4U, 31U);
  prv_check_range(3600U * 16384U, 4U, 3599U);

  prv_check_range(65536U, 3U, 65535U);
  prv_check_range(65536U * 8U, 0U, 65535U);
}

void test_timer_stm32_rtc_wut__event_timing(void) {
  prv_init_both();

  prv_arm(16U);
  cl_assert_equal_i(prv_counts_to_event(100U), 16);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
  cl_assert_equal_i(s_m.cr & (M_CR_WUTE | M_CR_WUTIE), 0);
  cl_assert_equal_i(s_m.isr & M_ISR_WUTF, 0);
  cl_assert_equal_i(prv_counts_to_event(100U), 0);

  prv_arm(80000U);
  const uint32_t n = prv_counts_to_event(90000U);
  cl_assert(n > 80000U - 8U && n <= 80000U);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 2);
}

void test_timer_stm32_rtc_wut__same_period_runs_on_without_writes(void) {
  prv_init_both();
  prv_arm(16U);

  s_log_len = 0;
  prv_arm(16U);
  for (int i = 0; i < s_log_len; i++) {
    cl_assert(!s_log[i].write);
  }
  cl_assert(s_armed);

  s_rearm_delta = 16U;
  cl_assert_equal_i(prv_counts_to_event(100U), 16);
  s_log_len = 0;
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_CR), 0);
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_WUTR), 0);
  cl_assert_equal_i(s_m.isr & M_ISR_WUTF, 0);
  cl_assert_equal_i(m_exti(M_EXTI_PR), 0);
  cl_assert_equal_i(prv_counts_to_event(100U), 16);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 2);

  s_rearm_delta = 32U;
  cl_assert_equal_i(prv_counts_to_event(100U), 16);
  prv_run_irq();
  cl_assert_equal_i(s_m.wutr, 31);
  cl_assert_equal_i(prv_counts_to_event(100U), 32);
}

void test_timer_stm32_rtc_wut__stale_interrupt_leaves_the_running_timer(void) {
  prv_init_both();
  prv_arm(64U);
  m_advance(10U);

  pbl_isr_3();
  cl_assert_equal_i(s_handler_calls, 0);
  cl_assert_equal_i(s_stale_irqs, 1);
  cl_assert_equal_i(s_m.cr & (M_CR_WUTE | M_CR_WUTIE), M_CR_WUTE | M_CR_WUTIE);

  cl_assert_equal_i(prv_counts_to_event(100U), 54);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
}

void test_timer_stm32_rtc_wut__set_next_drops_pending_interrupt(void) {
  prv_init_both();
  s_irq_pending[M_IRQ_WKUP] = true;

  prv_arm(64U);
  cl_assert(!s_irq_pending[M_IRQ_WKUP]);
}

void test_timer_stm32_rtc_wut__set_next_drops_expired_event(void) {
  prv_init_both();
  prv_arm(16U);
  cl_assert_equal_i(prv_counts_to_event(100U), 16);

  prv_arm(64U);
  cl_assert(!s_irq_pending[M_IRQ_WKUP]);
  cl_assert_equal_i(s_m.isr & M_ISR_WUTF, 0);
  cl_assert_equal_i(m_exti(M_EXTI_PR), 0);
  cl_assert_equal_i(prv_counts_to_event(100U), 64);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
}

void test_timer_stm32_rtc_wut__event_check(void) {
  prv_init_both();

  s_log_len = 0;
  prv_event_check();
  cl_assert_equal_i(s_log_len, 0);

  prv_arm(16U);
  prv_event_check();
  cl_assert_equal_i(s_repended, 0);
  cl_assert_equal_i(s_rearmed, 0);
  cl_assert(!s_irq_pending[M_IRQ_WKUP]);

  cl_assert_equal_i(prv_counts_to_event(100U), 16);
  s_irq_pending[M_IRQ_WKUP] = false;
  prv_event_check();
  cl_assert(s_irq_pending[M_IRQ_WKUP]);
  cl_assert_equal_i(s_repended, 1);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);

  s_work_calls = 0;
  prv_arm(1000U);
  s_m.cr &= ~(M_CR_WUTE | M_CR_WUTIE);
  prv_event_check();
  cl_assert_equal_i(s_rearmed, 1);
  cl_assert_equal_i(s_m.cr & (M_CR_WUTE | M_CR_WUTIE), M_CR_WUTE | M_CR_WUTIE);
  cl_assert_equal_i(s_m.wutr, CONFIG_TIMER_STM32_RTC_WUT_MIN_COUNT - 1);
  cl_assert_equal_i(s_work_calls, 1);
}

void test_timer_stm32_rtc_wut__shutdown(void) {
  prv_init_both();
  prv_arm(16U);

  pbl_irq_lock();
  s_rtc_ce.ops->shutdown(&s_rtc_ce);
  pbl_irq_unlock();
  cl_assert(!s_armed);
  cl_assert_equal_i(s_m.cr & (M_CR_WUTE | M_CR_WUTIE), 0);
  cl_assert_equal_i(prv_counts_to_event(100U), 0);
}
