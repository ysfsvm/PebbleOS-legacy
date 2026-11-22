/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/reset.h>

/**
 * @defgroup drivers_reset_stm32f4 STM32F4 RCC resets
 * @ingroup drivers_reset
 * @brief @ref drivers_reset controller of the STM32F4 RCC.
 *
 * Lines from STM32F4_*_RESET().
 *
 * @code{.c}
 * PBL_RESET_STM32F4_DEFINE(s_rcc_reset, "rcc", 0x40023800);
 *
 * static const struct pbl_reset s_spi1_reset = PBL_RESET(&s_rcc_reset, STM32F4_APB2_RESET(SPI1));
 * @endcode
 * @{
 */

/** @brief STM32F4 RCC reset controller. */
struct pbl_reset_stm32f4 {
  /** Reset controller. */
  struct pbl_reset_controller ctrl;
  /** RCC registers. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_reset_controller_ops pbl_reset_stm32f4_ops;

/* Past APB2RSTR, the last reset register */
#define PBL_RESET_STM32F4_NUM_LINES ((0x24U + 4U) * 8U)
/** @endcond */

/**
 * @brief Define an STM32F4 RCC reset controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name of the RCC.
 * @param _base RCC registers.
 */
#define PBL_RESET_STM32F4_DEFINE(sym, _name, _base)                                         \
  PBL_RESET_CONTROLLER_STATE_DEFINE(sym);                                                   \
  PBL_DEVICE_INSTANCE(sym, struct pbl_reset_stm32f4, struct pbl_reset_controller, ctrl) {   \
    .ctrl = PBL_RESET_CONTROLLER_INIT(sym, _name "_reset", nullptr, nullptr,                \
                                      &pbl_reset_stm32f4_ops, PBL_RESET_STM32F4_NUM_LINES), \
    .base = (_base),                                                                        \
  }

/** @} */
