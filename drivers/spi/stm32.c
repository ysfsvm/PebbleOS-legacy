/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/spi/stm32.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32f4_spi.h>

#define MHZ_PER_HZ 1000ULL

#define BUSY_POLLS_MAX 1000U

static const struct pbl_spi_stm32 *prv_spi(const struct pbl_spi_bus *bus) {
  return container_of(bus, const struct pbl_spi_stm32, bus);
}

static inline uint32_t prv_getreg(const struct pbl_spi_stm32 *spi, uint32_t offset) {
  return pbl_readl(spi->base + offset);
}

static inline void prv_putreg(const struct pbl_spi_stm32 *spi, uint32_t offset, uint32_t value) {
  pbl_writel(value, spi->base + offset);
}

static int prv_init(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_stm32 *spi = prv_spi(bus);
  int ret;

  ret = pbl_device_init(&spi->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&spi->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  *spi->state = (struct pbl_spi_stm32_state){0};

  if (spi->reset.ctrl == nullptr) {
    return 0;
  }

  ret = pbl_device_init(&spi->reset.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&spi->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_reset_reset(&spi->reset);
  (void)pbl_clk_disable(&spi->clk);

  return ret;
}

static int prv_configure(const struct pbl_spi_bus *bus, const struct pbl_spi_dev *dev) {
  const struct pbl_spi_stm32 *spi = prv_spi(bus);
  struct pbl_spi_stm32_state *state = spi->state;
  uint32_t cr1 = STM32F4_SPI_CR1_MSTR | STM32F4_SPI_CR1_SSM | STM32F4_SPI_CR1_SSI;
  uint64_t rate_mhz;
  uint32_t br;
  int ret;

  if (state->dev == dev) {
    return 0;
  }

  if (dev->max_hz == 0U) {
    return -EINVAL;
  }

  ret = pbl_clk_get_rate(&spi->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  for (br = 0U; br < STM32F4_SPI_CR1_BR_MAX; br++) {
    if ((rate_mhz / MHZ_PER_HZ) >> (br + 1U) <= dev->max_hz) {
      break;
    }
  }

  cr1 |= PBL_FIELD_PREP(STM32F4_SPI_CR1_BR_MASK, br);
  if ((dev->mode & PBL_SPI_CPHA) != 0U) {
    cr1 |= STM32F4_SPI_CR1_CPHA;
  }
  if ((dev->mode & PBL_SPI_CPOL) != 0U) {
    cr1 |= STM32F4_SPI_CR1_CPOL;
  }
  if ((dev->mode & PBL_SPI_LSB_FIRST) != 0U) {
    cr1 |= STM32F4_SPI_CR1_LSBFIRST;
  }

  state->dev = dev;
  state->pending = state->pending || cr1 != state->cr1;
  state->cr1 = cr1;

  return 0;
}

#ifdef CONFIG_SPI_STM32_STATS
static volatile uint32_t s_xfers;
static volatile uint32_t s_xfer_max_us;

void pbl_spi_stm32_stats(uint32_t *count, uint32_t *max_us) {
  *count = s_xfers;
  *max_us = s_xfer_max_us;
}

static void prv_stats_start(const struct pbl_spi_stm32 *spi) {
  s_xfers++;
  spi->state->start_cycles = pbl_cycle_get_32();
}

static void prv_stats_end(const struct pbl_spi_stm32 *spi) {
  const uint32_t us = (uint32_t)((uint64_t)(pbl_cycle_get_32() - spi->state->start_cycles) *
                                 1000000U / pbl_cycles_per_sec());

  if (us > s_xfer_max_us) {
    s_xfer_max_us = us;
  }
}
#else
static inline void prv_stats_start(const struct pbl_spi_stm32 *spi) {
}

static inline void prv_stats_end(const struct pbl_spi_stm32 *spi) {
}
#endif

// CR1 with the block off, then SPE
static void prv_enable(const struct pbl_spi_bus *bus) {
  const struct pbl_spi_stm32 *spi = prv_spi(bus);
  struct pbl_spi_stm32_state *state = spi->state;

  (void)pbl_clk_enable(&spi->clk);
  prv_stats_start(spi);

  if (state->pending) {
    prv_putreg(spi, STM32F4_SPI_CR1_OFFSET, state->cr1);
    prv_putreg(spi, STM32F4_SPI_CR1_OFFSET, state->cr1 | STM32F4_SPI_CR1_SPE);
    state->pending = false;
  }
}

static void prv_disable(const struct pbl_spi_bus *bus) {
  prv_stats_end(prv_spi(bus));
  (void)pbl_clk_disable(&prv_spi(bus)->clk);
}

static int prv_start(const struct pbl_spi_bus *bus, const struct pbl_spi_xfer *xfer) {
  const struct pbl_spi_stm32 *spi = prv_spi(bus);
  const uint8_t *tx = xfer->tx_buf;
  uint8_t *rx = xfer->rx_buf;
  uint32_t data;

  for (uint32_t i = 0U; i < xfer->len; i++) {
    while ((prv_getreg(spi, STM32F4_SPI_SR_OFFSET) & STM32F4_SPI_SR_TXE) == 0U) {
    }
    prv_putreg(spi, STM32F4_SPI_DR_OFFSET, (tx != nullptr) ? tx[i] : 0U);
    while ((prv_getreg(spi, STM32F4_SPI_SR_OFFSET) & STM32F4_SPI_SR_RXNE) == 0U) {
    }
    data = prv_getreg(spi, STM32F4_SPI_DR_OFFSET);
    if (rx != nullptr) {
      rx[i] = (uint8_t)data;
    }
  }

  // Until the last bit is out
  for (uint32_t i = 0U; i < BUSY_POLLS_MAX; i++) {
    if ((prv_getreg(spi, STM32F4_SPI_SR_OFFSET) & STM32F4_SPI_SR_BSY) == 0U) {
      break;
    }
  }

  pbl_spi_bus_complete(bus, 0);

  return 0;
}

const struct pbl_spi_bus_ops pbl_spi_stm32_ops = {
  .init = prv_init,
  .configure = prv_configure,
  .enable = prv_enable,
  .disable = prv_disable,
  .start = prv_start,
};

#include PBL_DT_INSTANCES(st_stm32f4_spi)
