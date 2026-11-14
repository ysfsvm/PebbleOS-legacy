/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @file clock.h
//! Peripheral clock gates of the STM32.
//!
//! The RCC enable registers are shared by every driver and are changed from threads and from
//! interrupt handlers. A plain read-modify-write of one can be interrupted by another and write
//! a stale value back, switching off a clock that was just enabled. Every change goes through
//! soc_stm32_clock_set(), which makes it with interrupts masked.

enum soc_stm32_bus {
  SOC_STM32_AHB1,
  SOC_STM32_AHB2,
  SOC_STM32_AHB3,
  SOC_STM32_APB1,
  SOC_STM32_APB2,
};

/**
 * @brief Enable or disable peripheral clocks.
 *
 * Callable from any context.
 *
 * @param bus Bus the peripherals are on.
 * @param mask Enable bits of the bus's RCC enable register (the RCC_*ENR_* or RCC_*Periph_*
 *             constants).
 * @param enable Whether to enable or disable them.
 */
void soc_stm32_clock_set(enum soc_stm32_bus bus, uint32_t mask, bool enable);
