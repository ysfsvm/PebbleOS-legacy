/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/task_wdt/arch.h>
#include <pbl/task_wdt/hardlockup.h>

#include <clar.h>

#define TIMEOUT_MS 4500U
// A SysTick capped below the period asked for, as on a 96 MHz Cortex-M4
#define PERIOD_MS 174U

static uint32_t s_period_asked;
static int s_reports;
static struct pbl_task_wdt_hardlockup s_lockup;

uint32_t arch_hardlockup_start(uint32_t period_ms) {
  s_period_asked = period_ms;
  return PERIOD_MS;
}

void pbl_task_wdt_hardlockup(const struct pbl_task_wdt_hardlockup *lockup) {
  s_reports++;
  s_lockup = *lockup;
}

[[noreturn]] void pbl_soc_reset(void) {
  cl_fail("the default report ran");
  for (;;) {
  }
}

static void prv_ticks(unsigned int n) {
  for (unsigned int i = 0; i < n; i++) {
    pbl_task_wdt_hardlockup_tick(0x08001234U, 0x08005678U, 56U);
  }
}

// Counted periods before the awake time reaches the timeout
#define PERIODS_TO_TIMEOUT ((TIMEOUT_MS + PERIOD_MS - 1U) / PERIOD_MS)

void test_task_wdt_hardlockup__initialize(void) {
  s_period_asked = 0U;
  s_reports = 0;
  s_lockup = (struct pbl_task_wdt_hardlockup){};
  pbl_task_wdt_hardlockup_start();
}

void test_task_wdt_hardlockup__asks_for_a_sixteenth_of_the_timeout(void) {
  cl_assert_equal_i(s_period_asked, TIMEOUT_MS / 16U);
}

void test_task_wdt_hardlockup__reports_at_the_timeout(void) {
  // The first tick after the start may come at once: it does not count
  prv_ticks(PERIODS_TO_TIMEOUT);
  cl_assert_equal_i(s_reports, 0);

  prv_ticks(1U);
  cl_assert_equal_i(s_reports, 1);
  cl_assert_equal_i(s_lockup.pc, 0x08001234U);
  cl_assert_equal_i(s_lockup.lr, 0x08005678U);
  cl_assert_equal_i(s_lockup.exception, 56U);
  cl_assert_equal_i(s_lockup.awake_ms, PERIODS_TO_TIMEOUT * PERIOD_MS);
}

void test_task_wdt_hardlockup__reports_once(void) {
  prv_ticks(PERIODS_TO_TIMEOUT * 3U);
  cl_assert_equal_i(s_reports, 1);
}

//! A feed just before a tick: counting that tick as a whole period would report a lockup a
//! period before the timeout.
void test_task_wdt_hardlockup__never_reports_early(void) {
  prv_ticks(5U);
  pbl_task_wdt_hardlockup_feed();
  prv_ticks(PERIODS_TO_TIMEOUT);
  cl_assert_equal_i(s_reports, 0);

  prv_ticks(1U);
  cl_assert_equal_i(s_reports, 1);
}

void test_task_wdt_hardlockup__a_feed_restarts_the_count(void) {
  prv_ticks(PERIODS_TO_TIMEOUT);
  pbl_task_wdt_hardlockup_feed();
  prv_ticks(PERIODS_TO_TIMEOUT);
  cl_assert_equal_i(s_reports, 0);

  prv_ticks(1U);
  cl_assert_equal_i(s_reports, 1);
}

void test_task_wdt_hardlockup__feeds_every_check_period_never_report(void) {
  const unsigned int per_check = 3000U / PERIOD_MS;

  for (int checks = 0; checks < 100; checks++) {
    prv_ticks(per_check);
    pbl_task_wdt_hardlockup_feed();
  }
  cl_assert_equal_i(s_reports, 0);
}
