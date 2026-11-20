/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define STM32F4_IWDG_KR_OFFSET  0x00
#define STM32F4_IWDG_PR_OFFSET  0x04
#define STM32F4_IWDG_RLR_OFFSET 0x08
#define STM32F4_IWDG_SR_OFFSET  0x0c

/* KR keys */
#define STM32F4_IWDG_KR_LOCK   0x0000U
#define STM32F4_IWDG_KR_UNLOCK 0x5555U
#define STM32F4_IWDG_KR_RELOAD 0xaaaaU
#define STM32F4_IWDG_KR_START  0xccccU

/* PR: the LSI divided by 4 << PR */
#define STM32F4_IWDG_PR_MASK  PBL_GENMASK(2, 0)
#define STM32F4_IWDG_PR_DIV64 4U

/* RLR */
#define STM32F4_IWDG_RLR_MASK PBL_GENMASK(11, 0)
