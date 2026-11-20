/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define STM32F4_PWR_CR_OFFSET 0x00

/* CR */
#define STM32F4_PWR_CR_LPDS PBL_BIT(0)
#define STM32F4_PWR_CR_PDDS PBL_BIT(1)
/* Backup domain write access */
#define STM32F4_PWR_CR_DBP PBL_BIT(8)
