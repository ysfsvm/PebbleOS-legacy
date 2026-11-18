/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_clk_stm32f4 STM32F4 RCC
 * @ingroup drivers_clk
 * @brief Clocks of the STM32F4 RCC.
 *
 * A clock is named in the tree by the two cells of the upstream binding: 0 and the gate of a
 * peripheral, @c STM32F4_AHB1_CLOCK(GPIOA) and the like, or 1 and a clock of
 * <dt-bindings/clock/stm32fx-clock.h>. A gate runs at the rate of its bus, a timer gate at
 * the timer clock of its bus (twice the bus rate when the bus is divided).
 * @{
 */

/** @cond INTERNAL_HIDDEN */
/* The gates, packed: the binding leaves a gap of one bus between AHB3 and APB1. */
#define STM32F4_CLK_NUM_GATES 0xa0U
/** @endcond */

/** @brief Clock number on the controller of the cells @p _type and @p _index of the binding. */
#define STM32F4_CLK_ID(_type, _index)                        \
  ((_type) != 0 ? STM32F4_CLK_NUM_GATES + (uint32_t)(_index) \
                : ((_index) < 0x80 ? (uint32_t)(_index) : (uint32_t)(_index) - 0x20U))

/** @} */
