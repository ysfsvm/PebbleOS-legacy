/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/stm32f4.h>

#include <devicetree/devices.h>

PBL_RNG_STM32_DEFINE(pbl_rng_stm32, "rng", 0x50060800, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 38)));
PBL_DEVICE_DEPENDS(pbl_rng_stm32, &pbl_rng_stm32, &dt_rcc.dev);
