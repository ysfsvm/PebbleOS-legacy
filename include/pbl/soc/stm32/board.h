/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// clang-format off

#include <stdint.h>

#include <pbl/drivers/dma.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/input/input.h>

#include <pbl/soc/stm32/cmsis.h>
// clang-format on

typedef enum {
  AccelThresholdLow,  ///< A sensitive state used for stationary mode
  AccelThresholdHigh, ///< The accelerometer's default sensitivity
  AccelThreshold_Num,
} AccelThreshold;

typedef struct {
  int axes_offsets[3];
  bool axes_inverts[3];
  uint32_t shake_thresholds[AccelThreshold_Num];
  uint32_t double_tap_threshold;
} AccelConfig;

typedef struct {
  const AccelConfig accel_config;
  const struct pbl_gpio accel_int_gpios[2];
} BoardConfigAccel;

//! Display driven through an iCE40LP framebuffer FPGA on a write-only SPI.
typedef struct {
  SPI_TypeDef *spi;
  uint32_t spi_rcc; ///< RCC_APB2Periph_SPIx
  uint8_t spi_af;   ///< GPIO_AF_SPIx
  //! SPI TX DMA channel
  struct pbl_dma_chan tx_dma;

  const struct pbl_gpio sclk;
  const struct pbl_gpio mosi;
  const struct pbl_gpio scs; ///< Active low

  const struct pbl_gpio creset; ///< Open drain, active high (released = run)
  const struct pbl_gpio cdone;
  const struct pbl_gpio busy;

  //! Panel supplies, in power-up order.
  const struct pbl_gpio rail_6v6; ///< VDDC
  const struct pbl_gpio rail_4v5; ///< VDDP

  //! SPI clock while programming the FPGA, then for frames.
  uint32_t base_spi_frequency;
  uint32_t fast_spi_frequency;
} BoardConfigIce40Display;

typedef const struct UARTDevice UARTDevice;
