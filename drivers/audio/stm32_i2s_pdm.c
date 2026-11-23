/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! PDM microphone through an STM32F4 SPI in I2S master receive mode, as the stock snowy firmware
//! did it: a 1.024 MHz PDM clock, OpenPDMFilter decimation by 64 to 16 kHz, in the DMA interrupt,
//! straight into the stream's periods. The microphone is powered from the PMIC's LDO3.

#include <errno.h>

#include <pbl/drivers/audio/stm32.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/drivers/pmic.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/sleep.h>
#include <pbl/util/misc.h>

#define SAMPLE_RATE 16000U
//! About 100 ms for the filter to settle.
#define SETTLE_SAMPLES (SAMPLE_RATE / 10U)

static const struct pbl_audio_stm32_i2s_pdm *prv_dev(const struct pbl_audio_dai *dai) {
  return container_of(dai, const struct pbl_audio_stm32_i2s_pdm, dai);
}

static void prv_gpio_enable(const struct pbl_audio_stm32_i2s_pdm *pdm) {
  pbl_gpio_stm32_af(&pdm->ck, pdm->gpio_af, 0, GPIO_High_Speed);
  pbl_gpio_stm32_af(&pdm->sd, pdm->gpio_af, 0, GPIO_High_Speed);
}

static void prv_gpio_disable(const struct pbl_audio_stm32_i2s_pdm *pdm) {
  // Clock held low, data floating in analog mode, for the lowest power.
  (void)pbl_gpio_configure(&pdm->ck, PBL_GPIO_OUTPUT_LOW);
  pbl_gpio_stm32_analog(&pdm->sd);
}

static int prv_i2s_configure(const struct pbl_audio_stm32_i2s_pdm *pdm) {
  SPI_I2S_DeInit(pdm->spi);
  I2S_InitTypeDef i2s;
  I2S_StructInit(&i2s);
  i2s.I2S_AudioFreq = I2S_AudioFreq_32k;
  i2s.I2S_Standard = I2S_Standard_LSB;
  i2s.I2S_DataFormat = I2S_DataFormat_16b;
  i2s.I2S_CPOL = I2S_CPOL_High;
  i2s.I2S_Mode = I2S_Mode_MasterRx;
  i2s.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
  I2S_Init(pdm->spi, &i2s);

  // The vendored I2S_Init() combines the HAL's packet length (16 bits, doubled for the LSB
  // standard) with the StdPeriph divisor of 32 * packet length, computes a prescaler of 0 and
  // falls back to 2: a ~9.6 MHz PDM clock. Set the prescaler for 32 bit clocks per 32 kHz frame
  // from the PLLI2S output.
  const uint32_t pll_in = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) ? HSE_VALUE : HSI_VALUE) /
                          (RCC->PLLCFGR & RCC_PLLCFGR_PLLM);
  const uint32_t plln = (RCC->PLLI2SCFGR & RCC_PLLI2SCFGR_PLLI2SN) >> 6;
  const uint32_t pllr = (RCC->PLLI2SCFGR & RCC_PLLI2SCFGR_PLLI2SR) >> 28;
  const uint32_t bit_clock = 32U * I2S_AudioFreq_32k;
  const uint32_t div = (((pll_in * plln) / pllr) + (bit_clock / 2U)) / bit_clock;
  if (div < 4U || div > 0x1ffU) {
    return -EIO;
  }
  pdm->spi->I2SPR = (uint16_t)((div / 2U) | ((div & 1U) << 8));
  return 0;
}

//! A half of the DMA buffer is full: filter it into the stream. From the DMA interrupt.
static void prv_dma_handler(void *arg, int status) {
  const struct pbl_audio_stm32_i2s_pdm *pdm = arg;
  struct pbl_audio_stm32_i2s_pdm_state *state = pdm->state;
  const struct pbl_audio *card = pbl_audio_dai_card(&pdm->dai);
  const struct pbl_audio_stream *stream = &card->state->stream;
  size_t residue;
  uint16_t *raw;
  uint8_t half;

  if (!state->running) {
    return;
  }

  if (status != 0) {
    state->running = false;
    I2S_Cmd(pdm->spi, DISABLE);
    SPI_I2S_DMACmd(pdm->spi, SPI_I2S_DMAReq_Rx, DISABLE);
    PBL_LOG_WRN("Microphone DMA failed: %d", status);
    return;
  }

  // Half that just ended
  if (pbl_dma_tx_status(pdm->dma, &residue) != 0 || residue == 0U) {
    return;
  }
  half = residue > sizeof(state->in_buffer[0]) ? 1U : 0U;
  if (half == state->half) {
    pbl_audio_report_xrun(card);
    return;
  }
  state->half = half;
  raw = state->in_buffer[half];

  // The filter takes the bytes the other way round.
  for (size_t i = 0; i < PBL_AUDIO_STM32_I2S_PDM_HALF_WORDS; i++) {
    raw[i] = __REV16(raw[i]);
  }

  // The filter puts out 1 ms (16 samples) per 64 words in.
  for (size_t i = 0; i < PBL_AUDIO_STM32_I2S_PDM_HALF_WORDS;
       i += PBL_AUDIO_STM32_I2S_PDM_DECIMATION) {
    uint16_t pcm[SAMPLE_RATE / 1000U];
    Open_PDM_Filter_64((uint8_t *)&raw[i], pcm, state->volume, &state->filter);

    for (size_t s = 0; s < ARRAY_LENGTH(pcm); s++) {
      if (state->settle > 0U) {
        state->settle--;
        continue;
      }
      int16_t *period = pbl_audio_period(card, state->period);
      period[state->frames++] = (int16_t)pcm[s];
      if (state->frames == stream->period_frames) {
        state->frames = 0U;
        state->period = (uint8_t)((state->period + 1U) % stream->periods);
        pbl_audio_period_elapsed(card);
      }
    }
  }
}

static int prv_init(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_stm32_i2s_pdm *pdm = prv_dev(dai);
  const struct pbl_dma_slave_config cfg = {
    .src_addr = (uintptr_t)&pdm->spi->DR,
    .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_2_BYTES,
  };
  int ret;

  if (pbl_device_init(&pdm->ck.port->dev) != 0 || pbl_device_init(&pdm->sd.port->dev) != 0) {
    return -ENODEV;
  }

  ret = pbl_device_init(&pdm->dma->ctrl->dev);
  if (ret != 0) {
    return ret;
  }
  ret = pbl_dma_slave_config(pdm->dma, &cfg);
  if (ret != 0) {
    return ret;
  }

  prv_gpio_disable(pdm);
  return 0;
}

static void prv_get_caps(const struct pbl_audio_dai *dai, struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 1U;
  // The filter volume is fixed by the board.
  caps->gain_min_mdb = 0;
  caps->gain_max_mdb = 0;
  caps->gain_step_mdb = 0;
  caps->gain_default_mdb = 0;
}

static int prv_start(const struct pbl_audio_dai *dai, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_stm32_i2s_pdm *pdm = prv_dev(dai);
  struct pbl_audio_stm32_i2s_pdm_state *state = pdm->state;
  int ret;

  if (stream->rate != SAMPLE_RATE || stream->channels != 1U) {
    return -EINVAL;
  }

  state->filter = (TPDMFilter_InitStruct){
    .Fs = SAMPLE_RATE,
    .LP_HZ = 8000,
    .HP_HZ = 10,
    .Out_MicChannels = 1,
    .In_MicChannels = 1,
    .Decimation = PBL_AUDIO_STM32_I2S_PDM_DECIMATION,
    .MaxVolume = 64,
  };
  Open_PDM_Filter_Init(&state->filter);
  state->half = 1U;
  state->period = 0U;
  state->frames = 0U;
  state->settle = SETTLE_SAMPLES;
  state->volume = mute ? 0U : pdm->volume;

  set_ldo3_power_state(true);

  // The I2S, its PLL and the DMA stop in STOP mode.
  soc_stm32_stop_block();

  RCC_PLLI2SCmd(ENABLE);
  while (RCC_GetFlagStatus(RCC_FLAG_PLLI2SRDY) == RESET) {
  }

  prv_gpio_enable(pdm);
  soc_stm32_clock_set(SOC_STM32_APB1, pdm->spi_rcc, true);
  ret = prv_i2s_configure(pdm);
  if (ret != 0) {
    goto err_off;
  }

  state->running = true;
  ret = pbl_dma_prep_dma_cyclic(pdm->dma, state->in_buffer, sizeof(state->in_buffer),
                                sizeof(state->in_buffer[0]), PBL_DMA_DEV_TO_MEM, prv_dma_handler,
                                (void *)pdm);
  if (ret != 0) {
    state->running = false;
    goto err_off;
  }
  SPI_I2S_DMACmd(pdm->spi, SPI_I2S_DMAReq_Rx, ENABLE);
  I2S_Cmd(pdm->spi, ENABLE);

  return 0;

err_off:
  soc_stm32_clock_set(SOC_STM32_APB1, pdm->spi_rcc, false);
  RCC_PLLI2SCmd(DISABLE);
  prv_gpio_disable(pdm);
  soc_stm32_stop_release();
  set_ldo3_power_state(false);
  return ret;
}

static int prv_stop(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_stm32_i2s_pdm *pdm = prv_dev(dai);

  pdm->state->running = false;
  (void)pbl_dma_terminate(pdm->dma);
  I2S_Cmd(pdm->spi, DISABLE);
  soc_stm32_clock_set(SOC_STM32_APB1, pdm->spi_rcc, false);
  RCC_PLLI2SCmd(DISABLE);
  prv_gpio_disable(pdm);
  set_ldo3_power_state(false);
  soc_stm32_stop_release();

  return 0;
}

const struct pbl_audio_dai_ops pbl_audio_stm32_i2s_pdm_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
};
