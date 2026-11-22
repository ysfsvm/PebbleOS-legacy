/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/accel.h>
#include <pbl/drivers/spi.h>

/**
 * @defgroup drivers_accel_bmi160 BMI160
 * @ingroup drivers_accel
 * @brief Bosch BMI160 @ref drivers_accel driver.
 *
 * On SPI; interrupt lines and axis mapping from BOARD_CONFIG_ACCEL.
 *
 * @code{.c}
 * PBL_BMI160_DEFINE(s_bmi160, &s_spi1, 0, 5000000, PBL_SPI_MODE_0);
 * @endcode
 * @{
 */

/** @brief BMI160 accelerometer. */
struct pbl_bmi160 {
  /** Accelerometer. */
  struct pbl_accel accel;
  /** SPI device. */
  struct pbl_spi_dev spi;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_accel_ops pbl_bmi160_ops;
/** @endcond */

/**
 * @brief Define a BMI160.
 *
 * @param sym Symbol of the accelerometer.
 * @param _bus SPI bus.
 * @param _cs Chip select index.
 * @param _max_hz Maximum clock frequency, in Hz.
 * @param _mode SPI mode flags.
 */
#define PBL_BMI160_DEFINE(sym, _bus, _cs, _max_hz, _mode)                  \
  PBL_ACCEL_STATE_DEFINE(sym);                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_bmi160, struct pbl_accel, accel) {   \
    .accel = PBL_ACCEL_INIT(sym, "bmi160", &(_bus)->dev, &pbl_bmi160_ops), \
    .spi = PBL_SPI_DEV(_bus, _cs, _max_hz, _mode),                         \
  }

/** @} */
