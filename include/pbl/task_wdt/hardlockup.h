/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup task_wdt_hardlockup Hard lockup detector
 * @ingroup task_wdt
 * @brief Backstop of the task watchdog for what starves its thread.
 *
 * An interrupt handler that never returns, or that is taken again and again, and a thread that
 * spins with the kernel's interrupts locked keep the watchdog thread from running: the hardware
 * watchdog then resets the system without a core dump. With @c CONFIG_TASK_WDT_HARDLOCKUP, a
 * timer of the architecture that the kernel's interrupt lock does not mask counts the time the
 * CPU spends out of deep sleep since the hardware watchdog was last fed. Past
 * @c CONFIG_TASK_WDT_HARDLOCKUP_TIMEOUT_MS, it reports the interrupted code to
 * pbl_task_wdt_hardlockup(), which by default writes a core dump.
 *
 * It does not see code spinning with every interrupt masked, nor a system that never wakes up
 * from deep sleep, where its timer stops: the hardware watchdog still resets those.
 * @{
 */

/** @brief The code a hard lockup was found in. */
struct pbl_task_wdt_hardlockup {
  /** Program counter of the interrupted code. */
  uint32_t pc;
  /** Link register of the interrupted code. */
  uint32_t lr;
  /** Exception the interrupted code ran in, as the architecture numbers it, 0 for a thread. */
  uint32_t exception;
  /** Time out of deep sleep since the hardware watchdog was last fed, in milliseconds. */
  uint32_t awake_ms;
};

#ifdef CONFIG_TASK_WDT_HARDLOCKUP

/**
 * @brief Note that the hardware watchdog was fed.
 *
 * Hardware watchdog drivers call it from watchdog_feed(), so feeds outside the watchdog thread
 * count too.
 */
void pbl_task_wdt_hardlockup_feed(void);

/** @brief Start the detector, once the hardware watchdog runs. */
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
 * Called once, from the detector's interrupt, which the kernel's interrupt lock does not mask:
 * it must not call the kernel. Weak: the default writes a core dump, or resets the system
 * without @c CONFIG_COREDUMP. An override records the lockup, then does the same.
 *
 * @param lockup The interrupted code.
 */
void pbl_task_wdt_hardlockup(const struct pbl_task_wdt_hardlockup *lockup);

/** @} */
