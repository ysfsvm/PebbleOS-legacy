/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup task_wdt_arch Hard lockup detector architecture interface
 * @ingroup task_wdt_hardlockup
 * @brief Implemented by architectures with @c ARCH_HAS_HARDLOCKUP_DETECTOR.
 * @{
 */

/**
 * @brief Start a periodic timer interrupt not masked by the kernel's interrupt lock.
 *
 * @param period_ms Wanted period, in milliseconds.
 * @return The period started, in milliseconds, at least 1.
 */
uint32_t arch_hardlockup_start(uint32_t period_ms);

/**
 * @brief Count one period of the detector's timer.
 *
 * @param pc Program counter of the interrupted code.
 * @param lr Link register of the interrupted code.
 * @param exception Exception the interrupted code ran in, 0 for a thread.
 */
void pbl_task_wdt_hardlockup_tick(uint32_t pc, uint32_t lr, uint32_t exception);

/** @} */
