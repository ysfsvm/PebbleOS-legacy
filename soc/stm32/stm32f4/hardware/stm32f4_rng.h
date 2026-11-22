/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define STM32F4_RNG_CR_OFFSET 0x00
#define STM32F4_RNG_SR_OFFSET 0x04
#define STM32F4_RNG_DR_OFFSET 0x08

/* CR */
#define STM32F4_RNG_CR_RNGEN PBL_BIT(2)
#define STM32F4_RNG_CR_IE    PBL_BIT(3)

/* SR */
#define STM32F4_RNG_SR_DRDY PBL_BIT(0)
#define STM32F4_RNG_SR_CECS PBL_BIT(1)
#define STM32F4_RNG_SR_SECS PBL_BIT(2)
#define STM32F4_RNG_SR_CEIS PBL_BIT(5)
#define STM32F4_RNG_SR_SEIS PBL_BIT(6)
