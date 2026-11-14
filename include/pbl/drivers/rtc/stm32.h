/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/rtc.h>

extern const struct pbl_rtc pbl_rtc_stm32;

//! The STM32F4 RTC, for a board's RTC.
#define STM32_RTC_DEVICE (&pbl_rtc_stm32)
