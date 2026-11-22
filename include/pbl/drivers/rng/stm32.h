/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/clk.h>

/**
 * @defgroup drivers_rng_stm32 STM32F4
 * @ingroup drivers_rng
 * @brief True random number generator of the STM32F4, behind rng_rand().
 *
 * @code{.c}
 * PBL_RNG_STM32_DEFINE(pbl_rng_stm32, "rng", 0x50060800,
 *                      PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_AHB2_CLOCK(RNG))));
 * @endcode
 * @{
 */

/** @brief STM32F4 RNG. */
struct pbl_rng_stm32 {
  /** Device. */
  struct pbl_device dev;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
};

/**
 * @brief Define the STM32F4 RNG.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 */
#define PBL_RNG_STM32_DEFINE(sym, _name, _base, _clk)                              \
  PBL_DEVICE_STATE_DEFINE(sym);                                                    \
  extern const struct pbl_rng_stm32 sym##_inst;                                    \
  PBL_DEVICE_REGISTER(sym##_inst, &sym##_inst.dev);                                \
  PBL_DEVICE_ALIAS(sym, sym##_inst, struct pbl_rng_stm32, struct pbl_device, dev); \
  const struct pbl_rng_stm32 sym##_inst = {                                        \
    .dev = PBL_DEVICE_INIT(sym, _name, nullptr, nullptr, nullptr),                 \
    .base = (_base),                                                               \
    .clk = _clk,                                                                   \
  }

/** @} */
