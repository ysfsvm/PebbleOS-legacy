/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

/* EXTICR1-4 */
#define STM32F4_SYSCFG_EXTICR_OFFSET(line) (0x08 + 4 * ((line) / 4))

/* Register bit definitions */

/* Four bits a line */
#define STM32F4_SYSCFG_EXTICR_FIELD(line) PBL_GENMASK(4 * ((line) % 4) + 3, 4 * ((line) % 4))
