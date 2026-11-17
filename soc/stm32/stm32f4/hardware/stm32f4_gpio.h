/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define STM32F4_GPIO_MODER_OFFSET   0x00
#define STM32F4_GPIO_OTYPER_OFFSET  0x04
#define STM32F4_GPIO_OSPEEDR_OFFSET 0x08
#define STM32F4_GPIO_PUPDR_OFFSET   0x0c
#define STM32F4_GPIO_IDR_OFFSET     0x10
#define STM32F4_GPIO_BSRR_OFFSET    0x18
/* AFRL for pins 0-7, AFRH for 8-15 */
#define STM32F4_GPIO_AFR_OFFSET(pin) (0x20 + 4 * ((pin) / 8))

/* Register bit definitions */

/* Two bits a pin in MODER, OSPEEDR and PUPDR, four in AFR */
#define STM32F4_GPIO_FIELD2(pin) PBL_GENMASK(2 * (pin) + 1, 2 * (pin))
#define STM32F4_GPIO_FIELD4(pin) PBL_GENMASK(4 * ((pin) % 8) + 3, 4 * ((pin) % 8))

#define STM32F4_GPIO_MODE_INPUT  0U
#define STM32F4_GPIO_MODE_OUTPUT 1U
#define STM32F4_GPIO_MODE_AF     2U
#define STM32F4_GPIO_MODE_ANALOG 3U

#define STM32F4_GPIO_PULL_NONE 0U
#define STM32F4_GPIO_PULL_UP   1U
#define STM32F4_GPIO_PULL_DOWN 2U

#define STM32F4_GPIO_BSRR_SET(pin)   PBL_BIT(pin)
#define STM32F4_GPIO_BSRR_RESET(pin) PBL_BIT((pin) + 16)

/* The ports are 1 KiB apart from port A */
#define STM32F4_GPIO_PORT_SIZE 0x400U
