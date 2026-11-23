/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/reset.h>
#include <pbl/drivers/spi.h>

/**
 * @defgroup drivers_spi_stm32 STM32F4 SPI
 * @ingroup drivers_spi
 * @brief @ref drivers_spi bus driver for the STM32F4 SPI block, in master mode.
 *
 * Polled 8-bit transfers; chip selects are GPIOs:
 *
 * @code{.c}
 * PBL_SPI_STM32_DEFINE(s_spi1, "spi1", 0x40013000,
 *                      PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB2_CLOCK(SPI1))),
 *                      PBL_RESET(&dt_rcc_reset, STM32F4_APB2_RESET(SPI1)), &s_spi1_default,
 *                      PBL_GPIO(&dt_gpioa, 4, PBL_GPIO_ACTIVE_LOW));
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_spi_stm32_state {
  const struct pbl_spi_dev *dev;
  uint32_t cr1;
  bool pending;
#ifdef CONFIG_SPI_STM32_STATS
  uint32_t start_cycles;
#endif
};
/** @endcond */

/** @brief STM32F4 SPI bus. */
struct pbl_spi_stm32 {
  /** SPI bus. */
  struct pbl_spi_bus bus;
  /** Driver runtime state. */
  struct pbl_spi_stm32_state *state;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** Reset line, or none. */
  struct pbl_reset reset;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_spi_bus_ops pbl_spi_stm32_ops;
/** @endcond */

/**
 * @brief Define an STM32F4 SPI bus.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 * @param _reset Reset line, from PBL_RESET(), or PBL_RESET(nullptr, 0).
 * @param _pinctrl Default pin control state.
 * @param ... Chip select pins, from PBL_GPIO(), by chip select number.
 */
#define PBL_SPI_STM32_DEFINE(sym, _name, _base, _clk, _reset, _pinctrl, ...)                       \
  PBL_SPI_BUS_STATE_DEFINE(sym, __VA_ARGS__);                                                      \
  static struct pbl_spi_stm32_state sym##_stm32_state;                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_spi_stm32, struct pbl_spi_bus, bus) {                        \
    .bus = PBL_SPI_BUS_INIT(sym, _name, &pbl_spi_stm32_ops, nullptr), .state = &sym##_stm32_state, \
    .base = (_base), .clk = _clk, .reset = _reset, .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),   \
  }

#ifdef CONFIG_SPI_STM32_STATS
/**
 * @brief Transfers of all STM32F4 SPI buses since boot, and the longest one.
 *
 * @param[out] count Transfers, chip select to chip select.
 * @param[out] max_us Longest transfer, in microseconds.
 */
void pbl_spi_stm32_stats(uint32_t *count, uint32_t *max_us);
#endif

/** @} */
