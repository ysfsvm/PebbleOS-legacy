/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* OTP blocks and their lock bytes */

#define STM32F4_OTP_BASE       0x1fff7800U
#define STM32F4_OTP_LOCK_BASE  0x1fff7a00U
#define STM32F4_OTP_BLOCK_SIZE 32U
#define STM32F4_OTP_NUM_BLOCKS 16U

/* Lock byte of a locked block */
#define STM32F4_OTP_LOCKED 0x00U
