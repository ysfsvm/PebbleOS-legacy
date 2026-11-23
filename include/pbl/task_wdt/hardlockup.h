/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup task_wdt_hardlockup Hard lockup detector
 * @ingroup task_wdt
 * @brief Reports code that starves the task watchdog thread.
 * @{
 */

/** @brief The code a hard lockup was found in. */
struct pbl_task_wdt_hardlockup {
  /** Program counter of the interrupted code. */
  uint32_t pc;
  /** Link register of the interrupted code. */
  uint32_t lr;
  /** Exception number, 0 for a thread. */
  uint32_t exception;
  /** CPU time since the last feed, in milliseconds. */
  uint32_t awake_ms;
};

#ifdef CONFIG_TASK_WDT_HARDLOCKUP

/**
 * @brief Note that the system is not locked up.
 */
void pbl_task_wdt_hardlockup_feed(void);

/** @brief Start the detector. */
void pbl_task_wdt_hardlockup_start(void);

#else

static inline void pbl_task_wdt_hardlockup_feed(void) {
}

static inline void pbl_task_wdt_hardlockup_start(void) {
}

#endif

/**
 * @brief Report a hard lockup.
 *
 * Weak; runs in the detector's interrupt and must not call the kernel.
 *
 * @param lockup The interrupted code.
 */
void pbl_task_wdt_hardlockup(const struct pbl_task_wdt_hardlockup *lockup);

/** @} */
