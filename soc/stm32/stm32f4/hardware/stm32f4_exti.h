/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Register offsets, one bit a line in each */

#define STM32F4_EXTI_IMR_OFFSET  0x00
#define STM32F4_EXTI_RTSR_OFFSET 0x08
#define STM32F4_EXTI_FTSR_OFFSET 0x0c
#define STM32F4_EXTI_PR_OFFSET   0x14

/* Lines 0-15 are the pins: line N takes pin N of the port SYSCFG routes to it */
#define STM32F4_EXTI_NUM_PIN_LINES 16U
