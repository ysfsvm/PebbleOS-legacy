/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Register offsets, one bit a line in each */

#define STM32F4_EXTI_IMR_OFFSET  0x00
#define STM32F4_EXTI_RTSR_OFFSET 0x08
#define STM32F4_EXTI_FTSR_OFFSET 0x0c
#define STM32F4_EXTI_PR_OFFSET   0x14

/* Lines 0-15: pins */
#define STM32F4_EXTI_NUM_PIN_LINES 16U

/* RTC alarm and wakeup lines */
#define STM32F4_EXTI_LINE_RTC_ALARM  (1U << 17)
#define STM32F4_EXTI_LINE_RTC_WAKEUP (1U << 22)
