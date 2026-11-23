/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/kernel.h>
#include <pbl/task_wdt/hardlockup.h>
#include <pbl/task_wdt/task_wdt.h>

#include <clar.h>
#include <fff.h>
#include <kernel_test.h>

void pbl_task_wdt_reset_for_test(void);

#define PERIOD_MS CONFIG_TASK_WDT_CHECK_PERIOD_MS
#define CHECKS    10

static uint8_t s_stack[4096] __attribute__((aligned(8)));
static struct pbl_thread s_thread;

static int s_starts;
static char s_start_thread[PBL_THREAD_NAME_LEN];
static int s_detector_feeds;

// ---- stubs ------------------------------------------------------------------

void pbl_log_sync(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
                  ...) {
}

void pbl_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
             ...) {
}

FAKE_VOID_FUNC(watchdog_feed);

void pbl_task_wdt_hardlockup_start(void) {
  const struct pbl_thread *t = pbl_thread_current();

  s_starts++;
  strncpy(s_start_thread, t != nullptr ? pbl_thread_name(t) : "", sizeof(s_start_thread) - 1U);
}

void pbl_task_wdt_hardlockup_feed(void) {
  s_detector_feeds++;
}

void pbl_task_wdt_stalled(const struct pbl_task_wdt_stall *stall) {
}

void pbl_task_wdt_recovered(void) {
}

[[noreturn]] void pbl_task_wdt_expired(void) {
  cl_fail("expired");
  for (;;) {
  }
}

// ---- tests ------------------------------------------------------------------

static void prv_idle_entry(void *arg) {
  pbl_thread_sleep(PBL_MSEC(CHECKS * PERIOD_MS + PERIOD_MS / 2));
  pbl_test_kernel_stop();
}

void test_task_wdt_detector__initialize(void) {
  memset(&s_thread, 0, sizeof(s_thread));
  memset(s_start_thread, 0, sizeof(s_start_thread));
  s_starts = 0;
  s_detector_feeds = 0;
  RESET_FAKE(watchdog_feed);
  pbl_task_wdt_reset_for_test();
  pbl_task_wdt_init();

  struct pbl_thread_attr attr = {
    .name = "idle",
    .entry = prv_idle_entry,
    .prio = 2,
    .privileged = true,
    .stack = s_stack,
    .stack_size = sizeof(s_stack),
  };
  cl_assert_equal_i(pbl_thread_create(&s_thread, &attr), 0);
  pbl_test_kernel_run();
}

void test_task_wdt_detector__the_watchdog_thread_starts_it(void) {
  cl_assert_equal_i(s_starts, 1);
  cl_assert_equal_s(s_start_thread, "TaskWDT");
}

void test_task_wdt_detector__every_check_feeds_it(void) {
  cl_assert(watchdog_feed_fake.call_count >= CHECKS);
  cl_assert(s_detector_feeds >= CHECKS);
}
