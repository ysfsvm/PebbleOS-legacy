/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Clocks of the STM32F4 RCC other than the gates, with the values of the upstream binding */
#define SYSTICK         0
#define FCLK            1
#define CLK_LSI         2
#define CLK_LSE         3
#define CLK_HSE_RTC     4
#define CLK_RTC         5
#define PLL_VCO_I2S     6
#define PLL_VCO_SAI     7
#define CLK_LCD         8
#define CLK_I2S         9
#define CLK_SAI1        10
#define CLK_SAI2        11
#define CLK_I2SQ_PDIV   12
#define CLK_SAIQ_PDIV   13
#define CLK_HSI         14
#define CLK_SYSCLK      15
#define END_PRIMARY_CLK 17
