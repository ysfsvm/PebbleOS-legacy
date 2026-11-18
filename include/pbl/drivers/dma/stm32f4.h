/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_dma_stm32f4 STM32F4 DMA controller
 * @ingroup drivers_dma
 * @brief @ref drivers_dma controller of the STM32F4 DMA1 and DMA2.
 *
 * A channel is a stream, 0 to 7, and its request is the stream's channel selection, 0 to 7, as in
 * the reference manual's request mapping. The configuration is the third cell of the upstream
 * binding, of which the priority is used: transfers always step through memory and never through
 * the peripheral. The fourth cell (FIFO threshold, direct mode) is not passed on and may hold any
 * value: transfers always run in direct mode, also for the 0 (FIFO mode) of the upstream USART
 * entries:
 *
 * @code{.c}
 * // SPI6_TX: DMA2 stream 5, channel 1
 * static const struct pbl_dma_chan s_tx =
 *     PBL_DMA_CHAN(&dt_dma2, 5, 1, PBL_DMA_STM32F4_PRIORITY(PBL_DMA_STM32F4_PRIORITY_VERY_HIGH));
 * @endcode
 *
 * The controller's clock is on while a stream runs. The controllers cannot reach the
 * core-coupled memory, so a buffer there fails with @c -EINVAL. Stopping a stream that does not
 * stop fails with @c -ETIMEDOUT.
 * @{
 */

/** @brief Priority of a stream, in the configuration. */
#define PBL_DMA_STM32F4_PRIORITY(pl) PBL_FIELD_PREP(PBL_GENMASK(17, 16), pl)
/** Low priority. */
#define PBL_DMA_STM32F4_PRIORITY_LOW 0U
/** Medium priority. */
#define PBL_DMA_STM32F4_PRIORITY_MEDIUM 1U
/** High priority. */
#define PBL_DMA_STM32F4_PRIORITY_HIGH 2U
/** Very high priority. */
#define PBL_DMA_STM32F4_PRIORITY_VERY_HIGH 3U

/** @cond INTERNAL_HIDDEN */
#define PBL_DMA_STM32F4_NUM_STREAMS 8U

struct pbl_dma_stm32f4_stream {
  /* Channel selection and priority, from the channel */
  uint32_t cr;
  /* Count of the transfers started */
  uint32_t seq;
  /* A transfer is running, holding the controller's clock */
  bool running;
};

struct pbl_dma_stm32f4 {
  struct pbl_dma_controller ctrl;
  uintptr_t base;
  struct pbl_clk clk;
  struct pbl_dma_stm32f4_stream *streams;
  pbl_irq_t irqs[PBL_DMA_STM32F4_NUM_STREAMS];
};

struct pbl_dma_stm32f4_stream_irq {
  const struct pbl_dma_stm32f4 *dmac;
  uint8_t stream;
};

extern const struct pbl_dma_controller_ops pbl_dma_stm32f4_ops;
int pbl_dma_stm32f4_init(const struct pbl_device *dev);
void pbl_dma_stm32f4_irq_handler(const struct pbl_dma_stm32f4_stream_irq *irq);

#define PBL_DMA_STM32F4_STREAM_IRQ(sym, _stream, _line)                                       \
  static const struct pbl_dma_stm32f4_stream_irq sym##_irq##_stream = {&sym##_inst, _stream}; \
  PBL_IRQ_CONNECT_NUM(_line, CONFIG_DMA_STM32F4_IRQ_PRIO, pbl_dma_stm32f4_irq_handler,        \
                      &sym##_irq##_stream, 0)
/** @endcond */

/**
 * @brief Define an STM32F4 DMA controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 * @param _irq0 Interrupt line of stream 0, followed by those of streams 1 to 7.
 */
#define PBL_DMA_STM32F4_DEFINE(sym, _name, _base, _clk, _irq0, _irq1, _irq2, _irq3, _irq4, _irq5, \
                               _irq6, _irq7)                                                      \
  PBL_DMA_CONTROLLER_STATE_DEFINE(sym, PBL_DMA_STM32F4_NUM_STREAMS);                              \
  static struct pbl_dma_stm32f4_stream sym##_streams[PBL_DMA_STM32F4_NUM_STREAMS];                \
  PBL_DEVICE_INSTANCE(sym, struct pbl_dma_stm32f4, struct pbl_dma_controller, ctrl){              \
    .ctrl = PBL_DMA_CONTROLLER_INIT(sym, _name, pbl_dma_stm32f4_init, &pbl_dma_stm32f4_ops),      \
    .base = (_base),                                                                              \
    .clk = _clk,                                                                                  \
    .streams = sym##_streams,                                                                     \
    .irqs = {_irq0, _irq1, _irq2, _irq3, _irq4, _irq5, _irq6, _irq7},                             \
  };                                                                                              \
  PBL_DMA_STM32F4_STREAM_IRQ(sym, 0, _irq0);                                                      \
  PBL_DMA_STM32F4_STREAM_IRQ(sym, 1, _irq1);                                                      \
  PBL_DMA_STM32F4_STREAM_IRQ(sym, 2, _irq2);                                                      \
  PBL_DMA_STM32F4_STREAM_IRQ(sym, 3, _irq3);                                                      \
  PBL_DMA_STM32F4_STREAM_IRQ(sym, 4, _irq4);                                                      \
  PBL_DMA_STM32F4_STREAM_IRQ(sym, 5, _irq5);                                                      \
  PBL_DMA_STM32F4_STREAM_IRQ(sym, 6, _irq6);                                                      \
  PBL_DMA_STM32F4_STREAM_IRQ(sym, 7, _irq7)

/** @} */
