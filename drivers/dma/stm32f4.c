/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/dma/stm32f4.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32f4_dma.h>

static_assert(PBL_DMA_STM32F4_NUM_STREAMS == STM32F4_DMA_NUM_STREAMS);

#define MAX_REQUEST 7U

// Reads of the enable bit a stream gets to stop; it ends the data item it is moving first
#define DISABLE_POLLS 1000U

// Of the binding's configuration cell: the memory increment, which every transfer has, and the
// priority
#define CONFIG_MSK (STM32F4_DMA_SXCR_MINC | STM32F4_DMA_SXCR_PL_MSK)

static const struct pbl_dma_stm32f4 *prv_dmac(const struct pbl_dma_controller *ctrl) {
  return container_of(ctrl, const struct pbl_dma_stm32f4, ctrl);
}

static uint32_t prv_getreg(const struct pbl_dma_controller *ctrl, uint32_t offset) {
  return pbl_readl(prv_dmac(ctrl)->base + offset);
}

static void prv_putreg(const struct pbl_dma_controller *ctrl, uint32_t offset, uint32_t value) {
  pbl_writel(value, prv_dmac(ctrl)->base + offset);
}

static void prv_clear_flags(const struct pbl_dma_controller *ctrl, uint8_t stream) {
  prv_putreg(ctrl, STM32F4_DMA_IFCR_OFFSET(stream),
             STM32F4_DMA_FLAGS << STM32F4_DMA_FLAGS_SHIFT(stream));
}

// The controllers reach the SRAMs and flash, not the core-coupled memory
static bool prv_reachable(const void *buf, size_t len) {
  uintptr_t start = (uintptr_t)buf;

  if (len > UINTPTR_MAX - start) {
    return false;
  }

#if CONFIG_CCM_SIZE
  return (start < CONFIG_CCM_BASE && len <= CONFIG_CCM_BASE - start) ||
         start >= CONFIG_CCM_BASE + CONFIG_CCM_SIZE;
#else
  return true;
#endif
}

// Clear the enable bit and the interrupt enables, wait for the stream to stop, clear its flags
static int prv_disable(const struct pbl_dma_controller *ctrl, uint8_t stream) {
  uint32_t cr = prv_getreg(ctrl, STM32F4_DMA_SXCR_OFFSET(stream));

  prv_putreg(ctrl, STM32F4_DMA_SXCR_OFFSET(stream),
             cr & ~(STM32F4_DMA_SXCR_EN | STM32F4_DMA_SXCR_IRQS));
  for (uint32_t i = 0U;
       (prv_getreg(ctrl, STM32F4_DMA_SXCR_OFFSET(stream)) & STM32F4_DMA_SXCR_EN) != 0U; i++) {
    if (i == DISABLE_POLLS) {
      return -ETIMEDOUT;
    }
  }
  prv_clear_flags(ctrl, stream);

  return 0;
}

// With the interrupt lock held
static void prv_release(const struct pbl_dma_controller *ctrl, uint8_t stream) {
  struct pbl_dma_stm32f4_stream *state = &prv_dmac(ctrl)->streams[stream];

  if (state->running) {
    state->running = false;
    (void)pbl_clk_disable(&prv_dmac(ctrl)->clk);
  }
}

// Bus width of the peripheral side of a transfer
static uint32_t prv_width(const struct pbl_dma_controller *ctrl, uint8_t stream,
                          enum pbl_dma_dir dir) {
  const struct pbl_dma_chan_state *chan = &ctrl->chans[stream];

  return dir == PBL_DMA_DEV_TO_MEM ? chan->cfg.src_addr_width : chan->cfg.dst_addr_width;
}

static int prv_slave_config(const struct pbl_dma_controller *ctrl, const struct pbl_dma_chan *chan,
                            const struct pbl_dma_slave_config *cfg) {
  struct pbl_dma_stm32f4_stream *state = &prv_dmac(ctrl)->streams[chan->channel];
  int ret = 0;

  if (chan->request > MAX_REQUEST || (chan->config & ~CONFIG_MSK) != 0U) {
    return -EINVAL;
  }

  pbl_irq_lock();
  if (state->running) {
    ret = -EBUSY;
  } else {
    state->cr = PBL_FIELD_PREP(STM32F4_DMA_SXCR_CHSEL_MSK, chan->request) |
                (chan->config & STM32F4_DMA_SXCR_PL_MSK);
  }
  pbl_irq_unlock();

  return ret;
}

static int prv_start(const struct pbl_dma_controller *ctrl, uint8_t stream, void *buf, size_t len,
                     enum pbl_dma_dir dir, uint32_t mode) {
  const struct pbl_dma_chan_state *chan = &ctrl->chans[stream];
  struct pbl_dma_stm32f4_stream *state = &prv_dmac(ctrl)->streams[stream];
  uint32_t width = prv_width(ctrl, stream, dir);
  uintptr_t par = dir == PBL_DMA_DEV_TO_MEM ? chan->cfg.src_addr : chan->cfg.dst_addr;
  uint32_t cr;
  int ret;

  if ((len % width) != 0U || len / width > STM32F4_DMA_SXNDTR_MAX || !prv_reachable(buf, len)) {
    return -EINVAL;
  }

  // 1, 2 or 4 bytes: 0, 1 or 2, the same on both sides in direct mode
  cr = state->cr | PBL_FIELD_PREP(STM32F4_DMA_SXCR_PSIZE_MSK, width >> 1) |
       PBL_FIELD_PREP(STM32F4_DMA_SXCR_MSIZE_MSK, width >> 1) | STM32F4_DMA_SXCR_MINC |
       STM32F4_DMA_SXCR_TEIE | mode |
       PBL_FIELD_PREP(STM32F4_DMA_SXCR_DIR_MSK, dir == PBL_DMA_MEM_TO_DEV
                                                    ? STM32F4_DMA_SXCR_DIR_M2P
                                                    : STM32F4_DMA_SXCR_DIR_P2M);

  pbl_irq_lock();
  if (state->running) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  ret = pbl_clk_enable(&prv_dmac(ctrl)->clk);
  if (ret != 0) {
    pbl_irq_unlock();
    return ret;
  }
  state->running = true;
  state->seq++;

  ret = prv_disable(ctrl, stream);
  if (ret != 0) {
    prv_release(ctrl, stream);
    pbl_irq_unlock();
    return ret;
  }

  prv_putreg(ctrl, STM32F4_DMA_SXPAR_OFFSET(stream), par);
  prv_putreg(ctrl, STM32F4_DMA_SXM0AR_OFFSET(stream), (uintptr_t)buf);
  prv_putreg(ctrl, STM32F4_DMA_SXNDTR_OFFSET(stream), len / width);
  prv_putreg(ctrl, STM32F4_DMA_SXFCR_OFFSET(stream), 0U);
  prv_putreg(ctrl, STM32F4_DMA_SXCR_OFFSET(stream), cr);
  prv_putreg(ctrl, STM32F4_DMA_SXCR_OFFSET(stream), cr | STM32F4_DMA_SXCR_EN);
  pbl_irq_unlock();

  return 0;
}

static int prv_prep_slave_single(const struct pbl_dma_controller *ctrl, uint8_t stream, void *buf,
                                 size_t len, enum pbl_dma_dir dir) {
  return prv_start(ctrl, stream, buf, len, dir, STM32F4_DMA_SXCR_TCIE);
}

// The half and full transfer interrupts end the periods of a ring of one or two
static int prv_prep_dma_cyclic(const struct pbl_dma_controller *ctrl, uint8_t stream, void *buf,
                               size_t buf_len, size_t period_len, enum pbl_dma_dir dir) {
  uint32_t mode = STM32F4_DMA_SXCR_CIRC | STM32F4_DMA_SXCR_TCIE;

  if ((period_len % prv_width(ctrl, stream, dir)) != 0U) {
    return -EINVAL;
  }

  if (buf_len == 2U * period_len) {
    mode |= STM32F4_DMA_SXCR_HTIE;
  } else if (buf_len != period_len) {
    return -EINVAL;
  }

  return prv_start(ctrl, stream, buf, buf_len, dir, mode);
}

static int prv_terminate(const struct pbl_dma_controller *ctrl, uint8_t stream) {
  int ret = 0;

  pbl_irq_lock();
  if (prv_dmac(ctrl)->streams[stream].running) {
    ret = prv_disable(ctrl, stream);
    prv_release(ctrl, stream);
  }
  pbl_irq_unlock();

  return ret;
}

static int prv_tx_status(const struct pbl_dma_controller *ctrl, uint8_t stream, size_t *residue) {
  uint32_t cr;

  // The registers read as zero while the clock is off
  pbl_irq_lock();
  if (!prv_dmac(ctrl)->streams[stream].running) {
    pbl_irq_unlock();
    *residue = 0U;
    return 0;
  }
  cr = prv_getreg(ctrl, STM32F4_DMA_SXCR_OFFSET(stream));
  *residue = (size_t)prv_getreg(ctrl, STM32F4_DMA_SXNDTR_OFFSET(stream))
             << PBL_FIELD_GET(STM32F4_DMA_SXCR_PSIZE_MSK, cr);
  pbl_irq_unlock();

  return 0;
}

void pbl_dma_stm32f4_irq_handler(const struct pbl_dma_stm32f4_stream_irq *irq) {
  const struct pbl_dma_controller *ctrl = &irq->dmac->ctrl;
  struct pbl_dma_stm32f4_stream *state = &irq->dmac->streams[irq->stream];
  uint8_t stream = irq->stream;
  uint32_t flags;
  uint32_t seq;
  uint32_t cr;

  // Locked, so that a more urgent interrupt cannot stop the stream and start another transfer
  // between the flags read and the release
  pbl_irq_lock();
  if (!state->running) {
    pbl_irq_unlock();
    return;
  }

  flags = (prv_getreg(ctrl, STM32F4_DMA_ISR_OFFSET(stream)) >> STM32F4_DMA_FLAGS_SHIFT(stream)) &
          STM32F4_DMA_FLAGS;
  prv_putreg(ctrl, STM32F4_DMA_IFCR_OFFSET(stream), flags << STM32F4_DMA_FLAGS_SHIFT(stream));
  cr = prv_getreg(ctrl, STM32F4_DMA_SXCR_OFFSET(stream));
  seq = state->seq;

  // A transfer error disables the stream. The FIFO error, which direct mode raises on an overrun
  // or underrun, does not; its interrupt is not enabled and its flag is cleared with the others.
  // At the end of a transfer that is not a ring the stream disabled itself.
  if ((flags & STM32F4_DMA_TEIF) != 0U) {
    (void)prv_disable(ctrl, stream);
    prv_release(ctrl, stream);
  } else if ((cr & STM32F4_DMA_SXCR_CIRC) == 0U && (flags & STM32F4_DMA_TCIF) != 0U) {
    prv_release(ctrl, stream);
  }
  pbl_irq_unlock();

  if ((flags & STM32F4_DMA_TEIF) != 0U) {
    pbl_dma_complete(ctrl, stream, -EIO);
    return;
  }

  if ((cr & STM32F4_DMA_SXCR_CIRC) != 0U) {
    if ((flags & STM32F4_DMA_HTIF) != 0U && (cr & STM32F4_DMA_SXCR_HTIE) != 0U) {
      pbl_dma_complete(ctrl, stream, 0);
    }
    // Not if the callback stopped the ring, or started another transfer
    if ((flags & STM32F4_DMA_TCIF) != 0U && state->running && state->seq == seq) {
      pbl_dma_complete(ctrl, stream, 0);
    }
    return;
  }

  // The callback may start the next transfer
  if ((flags & STM32F4_DMA_TCIF) != 0U) {
    pbl_dma_complete(ctrl, stream, 0);
  }
}

// Streams left running or with their interrupts enabled before boot are stopped, so that none
// interrupts once its line is enabled
int pbl_dma_stm32f4_init(const struct pbl_device *dev) {
  const struct pbl_dma_stm32f4 *dmac = container_of(dev, const struct pbl_dma_stm32f4, ctrl.dev);
  int ret = pbl_device_init(&dmac->clk.ctrl->dev);

  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&dmac->clk);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t stream = 0U; stream < STM32F4_DMA_NUM_STREAMS && ret == 0; stream++) {
    ret = prv_disable(&dmac->ctrl, stream);
    prv_putreg(&dmac->ctrl, STM32F4_DMA_SXFCR_OFFSET(stream), 0U);
  }

  (void)pbl_clk_disable(&dmac->clk);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t stream = 0U; stream < STM32F4_DMA_NUM_STREAMS; stream++) {
    pbl_irq_enable(dmac->irqs[stream]);
  }

  return 0;
}

const struct pbl_dma_controller_ops pbl_dma_stm32f4_ops = {
  .slave_config = prv_slave_config,
  .prep_slave_single = prv_prep_slave_single,
  .prep_dma_cyclic = prv_prep_dma_cyclic,
  .terminate = prv_terminate,
  .tx_status = prv_tx_status,
};

#include PBL_DT_INSTANCES(st_stm32_dma)
