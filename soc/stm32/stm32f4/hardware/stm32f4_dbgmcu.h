/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Private peripheral bus */
#define STM32F4_DBGMCU_BASE 0xe0042000U

/* Register offsets */

#define STM32F4_DBGMCU_CR_OFFSET     0x04
#define STM32F4_DBGMCU_APB1FZ_OFFSET 0x08

/* APB1FZ */
#define STM32F4_DBGMCU_APB1FZ_IWDG_STOP PBL_BIT(12)
