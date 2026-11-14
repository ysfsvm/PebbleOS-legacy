/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/dma/stm32.h>
#include <pbl/drivers/gpio.h>
#include <pbl/soc/stm32/cmsis.h>

#include <OpenPDMFilter.h>

//! @file stm32.h
//! PDM microphone sampled by an STM32F4 SPI in I2S master receive mode, decimated to 16 kHz by
//! the OpenPDMFilter (Pebble Time, Time Round). A capture DAI, for a card without a codec: it
//! powers the microphone from the PMIC's LDO3 itself.

//! PDM bits per output sample.
#define PBL_AUDIO_STM32_I2S_PDM_DECIMATION 64U
//! Output samples per DMA half buffer.
#define PBL_AUDIO_STM32_I2S_PDM_BATCH 8U
//! 16-bit words per DMA half buffer.
#define PBL_AUDIO_STM32_I2S_PDM_HALF_WORDS \
  (PBL_AUDIO_STM32_I2S_PDM_DECIMATION * PBL_AUDIO_STM32_I2S_PDM_BATCH)

/** @cond INTERNAL_HIDDEN */
struct pbl_audio_stm32_i2s_pdm_state {
  //! Raw PDM, two halves filled by circular DMA. In SRAM, which DMA reaches.
  uint16_t in_buffer[2][PBL_AUDIO_STM32_I2S_PDM_HALF_WORDS];
  TPDMFilter_InitStruct filter;
  //! Period being filled and frames already in it.
  uint8_t period;
  size_t frames;
  //! Output still discarded while the filter settles, in samples.
  uint32_t settle;
  uint16_t volume;
  bool running;
};
/** @endcond */

/** @brief STM32 I2S PDM microphone. */
struct pbl_audio_stm32_i2s_pdm {
  /** DAI. */
  struct pbl_audio_dai dai;
  /** SPI in I2S mode. */
  SPI_TypeDef *spi;
  /** Its RCC_APB1Periph_SPIx enable bit. */
  uint32_t spi_rcc;
  /** RX DMA stream. */
  const struct pbl_dma_stm32_stream *dma;
  /** Clock and data pins, and their alternate function. */
  struct pbl_gpio ck;
  struct pbl_gpio sd;
  uint8_t gpio_af;
  /** OpenPDMFilter volume (the stock firmware used 250). */
  uint16_t volume;
  /** Driver runtime state. */
  struct pbl_audio_stm32_i2s_pdm_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_dai_ops pbl_audio_stm32_i2s_pdm_ops;
/** @endcond */

/**
 * @brief Define the microphone, a capture DAI.
 *
 * @param sym Symbol of the DAI.
 * @param _name Device name.
 * @param _spi SPI in I2S mode.
 * @param _spi_rcc Its RCC_APB1Periph_SPIx enable bit.
 * @param _dma RX DMA stream.
 * @param _ck Clock pin, from PBL_GPIO().
 * @param _sd Data pin, from PBL_GPIO().
 * @param _af Alternate function of both pins (GPIO_AF_SPIx).
 * @param _volume OpenPDMFilter volume.
 */
#define PBL_AUDIO_STM32_I2S_PDM_DEFINE(sym, _name, _spi, _spi_rcc, _dma, _ck, _sd, _af, _volume)  \
  PBL_AUDIO_DAI_STATE_DEFINE(sym);                                                                \
  static struct pbl_audio_stm32_i2s_pdm_state sym##_pdm_state;                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_audio_stm32_i2s_pdm, struct pbl_audio_dai, dai) {           \
    .dai =                                                                                        \
        PBL_AUDIO_DAI_INIT(sym, _name, PBL_AUDIO_CAPTURE, &pbl_audio_stm32_i2s_pdm_ops, nullptr), \
    .spi = (_spi), .spi_rcc = (_spi_rcc), .dma = (_dma), .ck = _ck, .sd = _sd, .gpio_af = (_af),  \
    .volume = (_volume), .state = &sym##_pdm_state,                                               \
  }
