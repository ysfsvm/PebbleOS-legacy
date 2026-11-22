/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <devicetree/devices.h>

PBL_SPI_STM32_DEFINE(dt_spi1, "spi1", 0x40013000,
                     PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB2_CLOCK(SPI1))),
                     PBL_RESET(&dt_rcc_reset, STM32F4_APB2_RESET(SPI1)), nullptr,
                     PBL_GPIO(nullptr, 0, 0));
PBL_SPI_STM32_DEFINE(dt_spi2, "spi2", 0x40003800,
                     PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB1_CLOCK(SPI2))),
                     PBL_RESET(nullptr, 0), nullptr, PBL_GPIO(nullptr, 0, 0));
