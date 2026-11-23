/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_clk_stm32f4 STM32F4 RCC
 * @ingroup drivers_clk
 * @brief Clocks of the STM32F4 RCC.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
/* Packed gates */
#define STM32F4_CLK_NUM_GATES 0xa0U
/** @endcond */

/** @brief Clock number from the binding's two cells. */
#define STM32F4_CLK_ID(_type, _index)                        \
  ((_type) != 0 ? STM32F4_CLK_NUM_GATES + (uint32_t)(_index) \
                : ((_index) < 0x80 ? (uint32_t)(_index) : (uint32_t)(_index) - 0x20U))

/** @} */
