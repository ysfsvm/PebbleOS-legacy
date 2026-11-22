/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

PBL_GPIO_STM32_DEFINE(dt_gpiob, "gpiob", 0x40020400, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 1)),
                      0x40013c00, 0x40013800, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 174)));
PBL_GPIO_STM32_DEFINE(dt_gpiof, "gpiof", 0x40021400, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 5)),
                      0x40013c00, 0x40013800, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 174)));
PBL_GPIO_STM32_DEFINE(dt_gpiog, "gpiog", 0x40021800, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 6)),
                      0x40013c00, 0x40013800, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 174)));
