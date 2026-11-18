/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/reboot.h>
#include <pbl/task_wdt/arch.h>
#include <pbl/task_wdt/hardlockup.h>

#ifdef CONFIG_COREDUMP
#include <pbl/coredump/coredump.h>
#endif

#define TIMEOUT_MS CONFIG_TASK_WDT_HARDLOCKUP_TIMEOUT_MS

// The timer's period is at most a sixteenth of the timeout. The first tick after a feed is not
// counted, as it may come at once: a lockup is reported after TIMEOUT_MS to TIMEOUT_MS plus two
// periods of running without a feed, never before.
#define PERIOD_DIV 16U
#define LATEST_MS  (TIMEOUT_MS + 2U * (TIMEOUT_MS / PERIOD_DIV))

// The watchdog thread feeds the detector every check period, or every grace period while a
// channel is expired, and the lockup must be found before the hardware watchdog resets.
static_assert(TIMEOUT_MS > CONFIG_TASK_WDT_CHECK_PERIOD_MS && TIMEOUT_MS > CONFIG_TASK_WDT_GRACE_MS,
              "a lockup would be found between two feeds");
static_assert(LATEST_MS < CONFIG_TASK_WDT_HARDLOCKUP_WATCHDOG_MIN_MS,
              "the hardware watchdog may reset before the lockup is found");

static uint32_t s_period_ms;
// Written by threads, read by the detector's interrupt, which only the NMI preempts
static volatile bool s_fed;
static uint32_t s_awake_ms;
static bool s_reported;

void pbl_task_wdt_hardlockup_feed(void) {
  s_fed = true;
}

void pbl_task_wdt_hardlockup_start(void) {
  s_reported = false;
  s_fed = true;
  s_awake_ms = 0U;
  s_period_ms = arch_hardlockup_start(TIMEOUT_MS / PERIOD_DIV);
}

void pbl_task_wdt_hardlockup_tick(uint32_t pc, uint32_t lr, uint32_t exception) {
  uint32_t awake_ms;

  if (s_reported) {
    return;
  }

  if (s_fed) {
    s_fed = false;
    s_awake_ms = 0U;
    return;
  }

  awake_ms = s_awake_ms + s_period_ms;
  s_awake_ms = awake_ms;
  if (awake_ms < TIMEOUT_MS) {
    return;
  }

  s_reported = true;
  pbl_task_wdt_hardlockup(&(struct pbl_task_wdt_hardlockup){
    .pc = pc,
    .lr = lr,
    .exception = exception,
    .awake_ms = awake_ms,
  });
}

PBL_WEAK void pbl_task_wdt_hardlockup(const struct pbl_task_wdt_hardlockup *lockup) {
#ifdef CONFIG_COREDUMP
  pbl_coredump_trigger(false);
#else
  pbl_soc_reset();
#endif
}
