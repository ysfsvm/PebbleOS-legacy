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

// The watchdog thread feeds the hardware watchdog every check period, or every grace period while
// a channel is expired; the lockup must be found before the hardware watchdog resets.
static_assert(TIMEOUT_MS > CONFIG_TASK_WDT_CHECK_PERIOD_MS && TIMEOUT_MS > CONFIG_TASK_WDT_GRACE_MS,
              "a lockup would be found between two feeds");
static_assert(TIMEOUT_MS < CONFIG_WATCHDOG_TIMEOUT_MS,
              "the hardware watchdog would reset before the lockup is found");

static uint32_t s_period_ms;
// Fed by threads, counted by the detector's interrupt, which only the NMI preempts
static volatile uint32_t s_awake_ms;
static bool s_reported;

void pbl_task_wdt_hardlockup_feed(void) {
  s_awake_ms = 0U;
}

void pbl_task_wdt_hardlockup_start(void) {
  s_reported = false;
  s_awake_ms = 0U;
  // Eight periods to the timeout: a lockup is found at most an eighth late
  s_period_ms = arch_hardlockup_start(TIMEOUT_MS / 8U);
}

void pbl_task_wdt_hardlockup_tick(uint32_t pc, uint32_t lr, uint32_t exception) {
  uint32_t awake_ms;

  if (s_reported) {
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
