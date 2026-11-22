/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

PBL_GPIO_STM32_DEFINE(dt_gpiob, "gpiob", 0x40020400, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 1)),
                      0x40013c00, 0x40013800, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 174)));
PBL_DEVICE_DEPENDS(dt_gpiob, &dt_gpiob.dev, &dt_rcc.dev);
PBL_GPIO_STM32_DEFINE(dt_gpiof, "gpiof", 0x40021400, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 5)),
                      0x40013c00, 0x40013800, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 174)),
                      PBL_GPIO_STM32_HOG(4, 0, PBL_GPIO_OUTPUT_INACTIVE));
PBL_DEVICE_DEPENDS(dt_gpiof, &dt_gpiof.dev, &dt_rcc.dev);
