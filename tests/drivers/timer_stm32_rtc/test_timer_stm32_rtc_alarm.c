/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/timer/stm32_rtc.c"
#include "stm32_rtc_env.h"

static void prv_on_event(void) {
}

static void prv_reset_driver(void) {
  s_started = false;
  s_offset = 0;
  s_last_now = 0U;
  s_last_ssr = 0U;
  s_last_tr = 0U;
  s_read_ssr = 0U;
  s_read_tr = 0U;
  s_mixed = 0U;
  s_backward = 0U;
  s_restored_stock = false;
  s_armed = false;
  s_deadline = 0U;
  s_locks = 0;
  s_handler_calls = 0;
  s_work_calls = 0;
  s_cs_hz = 0U;
  s_ce_hz = 0U;
  s_apb1_clocks = 0U;
  s_busy_us = 0U;
  memset(s_irq_enabled, 0, sizeof(s_irq_enabled));
}

static void prv_init_both(void) {
  cl_assert_equal_i(prv_init(&s_rtc_cs.dev), 0);
  cl_assert_equal_i(prv_init(&s_rtc_ce.dev), 0);
}

static uint32_t prv_cs_read(void) {
  pbl_irq_lock();
  const uint32_t raw = s_rtc_cs.ops->read(&s_rtc_cs);
  pbl_irq_unlock();
  return raw;
}

static uint64_t prv_count(void) {
  pbl_irq_lock();
  const uint64_t count = prv_calendar_count();
  pbl_irq_unlock();
  return count;
}

static void prv_arm(uint32_t delta) {
  pbl_irq_lock();
  s_rtc_ce.ops->set_next(&s_rtc_ce, delta);
  pbl_irq_unlock();
}

static void prv_run_irq(void) {
  if (s_irq_pending[M_IRQ_ALARM]) {
    s_irq_pending[M_IRQ_ALARM] = false;
    pbl_isr_41();
  }
}

void test_timer_stm32_rtc_alarm__initialize(void) {
  m_reset_stock();
  prv_reset_driver();
}

void test_timer_stm32_rtc_alarm__cleanup(void) {
  cl_assert_equal_i(s_locks, 0);
  cl_assert(s_busy_us < RTC_SYNC_TIMEOUT_US);
}

void test_timer_stm32_rtc_alarm__init_on_stock_setup(void) {
  const int64_t secs = s_m.secs;

  s_m.cr = M_CR_ALRAE | M_CR_ALRAIE | M_CR_WUTE | M_CR_WUTIE | 3U;
  s_m.isr |= M_ISR_ALRAF | M_ISR_WUTF;
  s_m.exti[M_EXTI_PR / 4U] = M_LINE_ALARM | M_LINE_WAKEUP;
  s_m.exti[M_EXTI_IMR / 4U] = 1U << 3;
  s_m.exti[M_EXTI_RTSR / 4U] = 1U << 3;
  s_m.exti[M_EXTI_FTSR / 4U] = (1U << 3) | M_LINE_ALARM | M_LINE_WAKEUP;

  prv_init_both();

  cl_assert_equal_i(m_writes(M_RTC_BASE + M_PRER), 0);
  for (int i = 0; i < s_log_len; i++) {
    if (s_log[i].write && s_log[i].addr == M_RTC_BASE + M_CR) {
      cl_assert_equal_i(s_log[i].val & (M_CR_FMT | M_CR_BYPSHAD), 0);
    }
  }
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_TR), 0);
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_DR), 0);
  cl_assert_equal_i(m_writes(M_RCC_BASE + M_RCC_BDCR), 0);
  cl_assert_equal_i(s_m.prer, M_PRER_STOCK);
  cl_assert(!stm32_rtc_restored_stock_config());
  m_assert_eq64(s_m.secs, secs);

  cl_assert_equal_i(s_m.cr & (M_CR_ALRAE | M_CR_ALRAIE | M_CR_WUTE | M_CR_WUTIE), 0);
  cl_assert_equal_i(s_m.isr & (M_ISR_ALRAF | M_ISR_WUTF | M_ISR_INIT), 0);
  cl_assert_equal_i(s_m.exti[M_EXTI_PR / 4U], 0);

  cl_assert(s_m.pwr_cr & M_PWR_CR_DBP);
  cl_assert(s_apb1_clocks & (1U << 28));
  cl_assert_equal_i(m_exti(M_EXTI_IMR), (1U << 3) | M_LINE_ALARM);
  cl_assert_equal_i(m_exti(M_EXTI_RTSR), (1U << 3) | M_LINE_ALARM);
  cl_assert_equal_i(m_exti(M_EXTI_FTSR), (1U << 3) | M_LINE_WAKEUP);
  cl_assert(s_irq_enabled[M_IRQ_ALARM]);
  cl_assert(!s_irq_enabled[M_IRQ_WKUP]);
  cl_assert_equal_i(s_m.wp, 0);
  cl_assert_equal_i(s_cs_hz, 256);
  cl_assert_equal_i(s_ce_hz, 256);
  cl_assert_equal_i(s_rtc_cs.mask, UINT32_MAX);
  cl_assert_equal_i(s_rtc_ce.min_delta, 3);
  cl_assert_equal_i(s_rtc_ce.max_delta, 3600 * 256);

  s_log_len = 0;
  cl_assert_equal_i(prv_init(&s_rtc_ce.dev), 0);
  cl_assert_equal_i(s_log_len, 0);
}

void test_timer_stm32_rtc_alarm__init_restores_stock_setup(void) {
  s_m.prer = (31U << 16) | 1023U;
  s_m.cr = M_CR_BYPSHAD | M_CR_FMT;
  s_m.ssr = 700U;
  const int64_t secs = s_m.secs;

  prv_init_both();

  cl_assert_equal_i(s_m.prer, M_PRER_STOCK);
  cl_assert_equal_i(s_m.cr & (M_CR_BYPSHAD | M_CR_FMT), 0);
  cl_assert(stm32_rtc_restored_stock_config());
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_PRER), 2);
  for (int i = 0, n = 0; i < s_log_len; i++) {
    if (s_log[i].write && s_log[i].addr == M_RTC_BASE + M_PRER) {
      cl_assert_equal_i(s_log[i].val, (n++ == 0) ? 255U : M_PRER_STOCK);
    }
  }
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_TR), 0);
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_DR), 0);
  m_assert_eq64(s_m.secs, secs);
  cl_assert_equal_i(s_m.isr & M_ISR_INIT, 0);
  cl_assert_equal_i(s_m.cr & (M_CR_ALRAE | M_CR_ALRAIE | M_CR_WUTE | M_CR_WUTIE), 0);
  cl_assert_equal_i(s_m.wp, 0);
}

void test_timer_stm32_rtc_alarm__init_after_backup_domain_reset(void) {
  s_m.bdcr = 0U;

  prv_init_both();

  cl_assert_equal_i(s_m.bdcr & (M_BDCR_LSEON | M_BDCR_LSEBYP | M_BDCR_RTCEN | M_BDCR_RTCSEL),
                    M_BDCR_LSEON | M_BDCR_LSEBYP | M_BDCR_RTCEN | (1U << 8));
  cl_assert_equal_i(m_writes(M_RTC_BASE + M_PRER), 0);
  cl_assert(!stm32_rtc_restored_stock_config());
}

void test_timer_stm32_rtc_alarm__init_restores_on_one_difference(void) {
  s_m.cr = M_CR_BYPSHAD;
  prv_init_both();
  cl_assert_equal_i(s_m.cr & (M_CR_BYPSHAD | M_CR_FMT), 0);
  cl_assert_equal_i(s_m.prer, M_PRER_STOCK);
  cl_assert(stm32_rtc_restored_stock_config());

  m_reset_stock();
  prv_reset_driver();
  s_m.cr = M_CR_FMT;
  prv_init_both();
  cl_assert_equal_i(s_m.cr & (M_CR_BYPSHAD | M_CR_FMT), 0);
  cl_assert(stm32_rtc_restored_stock_config());

  m_reset_stock();
  prv_reset_driver();
  s_m.prer = (127U << 16) | 1023U;
  prv_init_both();
  cl_assert_equal_i(s_m.prer, M_PRER_STOCK);
  cl_assert(stm32_rtc_restored_stock_config());
  cl_assert_equal_i(s_m.isr & M_ISR_INIT, 0);
}

static uint64_t prv_count_at(int year, int mon, int mday, int hour, int min, int sec,
                             uint32_t ssr) {
  m_set_calendar(year, mon, mday, hour, min, sec, ssr);
  return prv_count();
}

void test_timer_stm32_rtc_alarm__calendar_count(void) {
  prv_init_both();

  m_assert_eq64(prv_count_at(2000, 1, 1, 0, 0, 0, 255U), 0);
  m_assert_eq64(prv_count_at(2000, 1, 1, 0, 0, 1, 254U), 257);

  m_set_calendar(2026, 11, 20, 12, 34, 56, 200U);
  m_assert_eq64(prv_count(), m_count());
  m_assert_eq64(prv_count(), ((uint64_t)(1795178096 - M_EPOCH_2000) * 256U) + 55U);

  m_assert_eq64(prv_count_at(2026, 2, 1, 0, 0, 0, 255U) - prv_count_at(2026, 1, 31, 23, 59, 59, 0U),
                1);
  m_assert_eq64(
      prv_count_at(2026, 1, 1, 0, 0, 0, 255U) - prv_count_at(2025, 12, 31, 23, 59, 59, 0U), 1);
  m_assert_eq64(prv_count_at(2024, 3, 1, 0, 0, 0, 255U) - prv_count_at(2024, 2, 29, 23, 59, 59, 0U),
                1);
  m_set_calendar(2099, 12, 31, 23, 59, 59, 0U);
  m_assert_eq64(prv_count(), m_count());

  m_assert_eq64(prv_count_at(2026, 11, 20, 12, 34, 56, 0x300U),
                prv_count_at(2026, 11, 20, 12, 34, 56, 255U));
}

void test_timer_stm32_rtc_alarm__calendar_get(void) {
  uint32_t tr;
  uint32_t dr;
  uint32_t subsec;

  prv_init_both();
  m_set_calendar(2026, 11, 20, 12, 34, 56, 63U);

  stm32_rtc_calendar_get_subsec(&tr, &dr, &subsec);
  cl_assert_equal_i(tr, 0x123456);
  cl_assert_equal_i(dr & ~(7U << 13), 0x261120);
  cl_assert_equal_i(subsec, 192);

  stm32_rtc_calendar_get(&tr, &dr);
  cl_assert_equal_i(tr, 0x123456);

  s_m.ssr = 0x300U;
  stm32_rtc_calendar_get_subsec(&tr, &dr, &subsec);
  cl_assert_equal_i(subsec, 0);
}

void test_timer_stm32_rtc_alarm__calendar_read_drops_a_mixed_reading(void) {
  prv_init_both();
  m_set_calendar(2026, 11, 20, 12, 34, 56, 4U);
  s_counts_per_access = 64U;

  const uint64_t count = prv_count();
  s_counts_per_access = 0U;
  cl_assert(count <= m_count());
  cl_assert(m_count() - count <= 3U);
  cl_assert_equal_i(s_mixed, 2);
  cl_assert_equal_i(s_work_calls, 1);
}

void test_timer_stm32_rtc_alarm__clock_source_never_goes_back(void) {
  prv_init_both();
  m_advance_steps(100U);
  const uint32_t before = prv_cs_read();

  s_m.ssr += 40U;
  cl_assert_equal_i(prv_cs_read(), before);
  cl_assert_equal_i(s_backward, 1);
  cl_assert_equal_i(s_backward_steps, 40);
  cl_assert_equal_i(s_work_calls, 1);

  m_advance_steps(40U);
  cl_assert_equal_i(prv_cs_read(), before);
  m_advance_steps(1U);
  cl_assert_equal_i(prv_cs_read(), before + 1U);
  cl_assert_equal_i(s_backward, 1);
}

void test_timer_stm32_rtc_alarm__calendar_set_keeps_clock_source(void) {
  prv_init_both();
  m_advance_steps(1000U);
  const uint32_t before = prv_cs_read();

  cl_assert_equal_i(stm32_rtc_calendar_set(0x080000, 0x300601), 0);
  cl_assert_equal_i(m_tr_of(s_m.secs), 0x080000);
  cl_assert_equal_i(m_dr_of(s_m.secs) & ~(7U << 13), 0x300601);
  cl_assert_equal_i(s_m.isr & M_ISR_INIT, 0);
  cl_assert_equal_i(s_m.wp, 0);
  cl_assert_equal_i(prv_cs_read(), before);

  m_advance_steps(300U);
  cl_assert_equal_i(prv_cs_read(), before + 300U);

  cl_assert_equal_i(stm32_rtc_calendar_set(0x000000, 0x010101), 0);
  cl_assert_equal_i(prv_cs_read(), before + 300U);
  m_advance_steps(256U);
  cl_assert_equal_i(prv_cs_read(), before + 556U);
  cl_assert_equal_i(s_backward, 0);
}

static void prv_check_alarm(int year, int mon, int mday, int hour, int min, int sec, uint32_t ssr,
                            uint32_t delta, uint32_t alrmar, uint32_t ss) {
  prv_init_both();
  m_set_calendar(year, mon, mday, hour, min, sec, ssr);
  const uint64_t target = m_count() + delta;

  prv_arm(delta);
  cl_assert_equal_i(s_m.alrmar, (1U << 31) | alrmar);
  cl_assert_equal_i(s_m.alrmassr, (8U << 24) | ss);
  cl_assert_equal_i(s_m.cr & (M_CR_ALRAE | M_CR_ALRAIE), M_CR_ALRAE | M_CR_ALRAIE);
  cl_assert_equal_i(s_m.wp, 0);
  cl_assert(!s_irq_pending[M_IRQ_ALARM]);

  m_advance_steps(delta - 1U);
  cl_assert(!s_irq_pending[M_IRQ_ALARM]);
  m_advance_steps(1U);
  cl_assert(s_irq_pending[M_IRQ_ALARM]);
  m_assert_eq64(m_count(), target);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
  cl_assert_equal_i(s_m.isr & M_ISR_ALRAF, 0);
  cl_assert_equal_i(m_exti(M_EXTI_PR), 0);
}

void test_timer_stm32_rtc_alarm__within_a_second(void) {
  prv_check_alarm(2026, 11, 20, 12, 34, 56, 200U, 100U, 0x123456, 100U);
}

void test_timer_stm32_rtc_alarm__across_a_second(void) {
  prv_check_alarm(2026, 11, 20, 12, 34, 56, 10U, 20U, 0x123457, 246U);
}

void test_timer_stm32_rtc_alarm__across_a_minute(void) {
  prv_check_alarm(2026, 11, 20, 12, 34, 59, 5U, 10U, 0x123500, 251U);
}

void test_timer_stm32_rtc_alarm__across_midnight(void) {
  prv_check_alarm(2026, 12, 31, 23, 59, 59, 1U, 3U, 0x000000, 254U);
}

void test_timer_stm32_rtc_alarm__longest(void) {
  prv_check_alarm(2026, 11, 20, 23, 30, 0, 255U, 3600U * 256U, 0x003000, 255U);
}

void test_timer_stm32_rtc_alarm__deadline_gone_by_fires_once(void) {
  prv_init_both();
  s_counts_per_access = 64U;

  prv_arm(3U);
  cl_assert(s_irq_pending[M_IRQ_ALARM]);
  cl_assert_equal_i(s_m.isr & M_ISR_ALRAF, 0);

  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
  pbl_isr_41();
  cl_assert_equal_i(s_handler_calls, 1);
}

void test_timer_stm32_rtc_alarm__early_interrupt_is_ignored(void) {
  prv_init_both();
  prv_arm(100U);

  pbl_isr_41();
  cl_assert_equal_i(s_handler_calls, 0);

  m_advance_steps(100U);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
}

void test_timer_stm32_rtc_alarm__shutdown(void) {
  prv_init_both();
  prv_arm(10U);

  pbl_irq_lock();
  s_rtc_ce.ops->shutdown(&s_rtc_ce);
  pbl_irq_unlock();
  cl_assert_equal_i(s_m.cr & (M_CR_ALRAE | M_CR_ALRAIE), 0);
  cl_assert_equal_i(s_m.wp, 0);

  m_advance_steps(20U);
  cl_assert(!s_irq_pending[M_IRQ_ALARM]);
  pbl_isr_41();
  cl_assert_equal_i(s_handler_calls, 0);
}

void test_timer_stm32_rtc_alarm__rearm_replaces(void) {
  prv_init_both();
  prv_arm(10U);
  prv_arm(50U);

  m_advance_steps(49U);
  cl_assert(!s_irq_pending[M_IRQ_ALARM]);
  m_advance_steps(1U);
  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
}

void test_timer_stm32_rtc_alarm__interrupt_keeps_other_flags(void) {
  prv_init_both();
  prv_arm(10U);
  m_advance_steps(10U);
  s_m.isr |= M_ISR_WUTF | (1U << 11);

  prv_run_irq();
  cl_assert_equal_i(s_handler_calls, 1);
  cl_assert_equal_i(s_m.isr & (M_ISR_ALRAF | M_ISR_WUTF | (1U << 11)), M_ISR_WUTF | (1U << 11));
}
