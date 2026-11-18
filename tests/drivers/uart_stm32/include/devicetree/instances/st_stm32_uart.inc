/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

PBL_UART_STM32_DEFINE(dt_usart3, "usart3", 0x40004800, 39, PBL_CLK(&s_rcc, CLK_PCLK1), 115200,
                      false, &s_usart3_default);
PBL_UART_STM32_DEFINE(dt_usart1, "usart1", 0x40011000, 37, PBL_CLK(&s_rcc, CLK_PCLK2), 115200, true,
                      &s_usart1_default);
PBL_UART_STM32_DEFINE(dt_usart2, "usart2", 0x40004400, 38, PBL_CLK(&s_rcc, CLK_PCLK1), 230400,
                      false);
