/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

/* Ports A and G of the STM32F4, at their addresses, which the test models */
PBL_GPIO_STM32_DEFINE(dt_gpioa, "gpioa", 0x40020000, PBL_CLK(nullptr, 0), 0x40013c00, 0x40013800,
                      PBL_CLK(nullptr, 0));
PBL_GPIO_STM32_DEFINE(dt_gpiog, "gpiog", 0x40021800, PBL_CLK(nullptr, 0), 0x40013c00, 0x40013800,
                      PBL_CLK(nullptr, 0));
