/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @file stm32_rtc.h
//! The STM32F4 RTC calendar, shared by the system timer and the RTC device.

//! Sub-second counter rate: the stock 127/255 prescalers, which the stock bootloader, recovery
//! firmware and firmware assume (the RTC set-up outlives this firmware in the backup domain).
#define STM32_RTC_SUBSEC_HZ 256U

/**
 * @brief Wait for the calendar's shadow registers to resynchronize, after STOP mode.
 *
 * Interrupts masked.
 */
void stm32_rtc_resync(void);

#ifdef CONFIG_TIMER_STM32_RTC_WUT
//! Before the CPU idles, with interrupts masked: make sure an armed clock event is running.
void stm32_rtc_event_check(void);
#else
static inline void stm32_rtc_event_check(void) {
}
#endif

/**
 * @brief Whether the RTC was found in another configuration at init and reset to the stock one.
 */
bool stm32_rtc_restored_stock_config(void);

/**
 * @brief Set the calendar, keeping the system clock source continuous.
 *
 * @param tr Time register value (BCD, 24-hour).
 * @param dr Date register value (BCD).
 * @return 0 or a negative errno.
 */
int stm32_rtc_calendar_set(uint32_t tr, uint32_t dr);

/**
 * @brief Read the calendar.
 *
 * @param[out] tr Time register value (BCD, 24-hour).
 * @param[out] dr Date register value (BCD).
 */
void stm32_rtc_calendar_get(uint32_t *tr, uint32_t *dr);

/**
 * @brief Read the calendar and the fraction of its current second.
 *
 * @param[out] tr Time register value (BCD, 24-hour).
 * @param[out] dr Date register value (BCD).
 * @param[out] subsec Time into the current second, in 1/STM32_RTC_SUBSEC_HZ s.
 */
void stm32_rtc_calendar_get_subsec(uint32_t *tr, uint32_t *dr, uint32_t *subsec);
