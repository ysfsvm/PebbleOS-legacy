/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <devicetree/devices.h>

PBL_SPI_STM32_DEFINE(dt_spi1, "spi1", 0x40013000,
                     PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB2_CLOCK(SPI1))),
                     PBL_RESET(&dt_rcc_reset, STM32F4_APB2_RESET(SPI1)), &dt_spi1_default,
                     PBL_GPIO(&dt_gpioa, 4, PBL_GPIO_ACTIVE_LOW));
