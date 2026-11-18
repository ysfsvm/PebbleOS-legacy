/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/uart/stm32.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32f4_usart.h>

#define MHZ_PER_HZ 1000ULL

#define CR1_FRAME (STM32F4_USART_CR1_M | STM32F4_USART_CR1_PCE | STM32F4_USART_CR1_PS)
#define CR3_FLOW  (STM32F4_USART_CR3_RTSE | STM32F4_USART_CR3_CTSE)

static const struct pbl_uart_stm32 *prv_stm32(const struct pbl_uart *uart) {
  return container_of(uart, const struct pbl_uart_stm32, uart);
}

static inline uintptr_t prv_reg(const struct pbl_uart_stm32 *usart, uint32_t offset) {
  return usart->base + offset;
}

static inline uint32_t prv_getreg(const struct pbl_uart_stm32 *usart, uint32_t offset) {
  return pbl_readl(prv_reg(usart, offset));
}

static inline void prv_putreg(const struct pbl_uart_stm32 *usart, uint32_t offset, uint32_t value) {
  pbl_writel(value, prv_reg(usart, offset));
}

// Waits only for bytes sent from here
static void prv_flush(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);

  if (!usart->state->tx_pending) {
    return;
  }

  while ((prv_getreg(usart, STM32F4_USART_SR_OFFSET) & STM32F4_USART_SR_TC) == 0U) {
  }
  usart->state->tx_pending = false;
}

static int prv_setup(const struct pbl_uart_stm32 *usart, uint32_t cr1) {
  int ret;

  ret = pbl_device_init(&usart->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&usart->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&usart->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0 && ret != -ENOENT) {
    return ret;
  }

  prv_flush(&usart->uart);

  prv_putreg(usart, STM32F4_USART_CR1_OFFSET, 0U);
  prv_putreg(usart, STM32F4_USART_CR2_OFFSET, 0U);
  prv_putreg(usart, STM32F4_USART_CR3_OFFSET, 0U);

  // Clear stale errors and data
  (void)prv_getreg(usart, STM32F4_USART_SR_OFFSET);
  (void)prv_getreg(usart, STM32F4_USART_DR_OFFSET);

  prv_putreg(usart, STM32F4_USART_CR1_OFFSET, cr1 | STM32F4_USART_CR1_UE);

  return 0;
}

static int prv_early_init(const struct pbl_uart *uart) {
  return prv_setup(prv_stm32(uart), STM32F4_USART_CR1_TE);
}

static int prv_init(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);
  int ret;

  ret = prv_setup(usart, STM32F4_USART_CR1_TE | STM32F4_USART_CR1_RE);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(usart->irq);

  return 0;
}

// USARTDIV, 8x oversampling for high rates
static int prv_brr(uint32_t clk_hz, uint32_t baudrate, uint32_t *brr, bool *over8) {
  uint32_t div = (clk_hz + baudrate / 2U) / baudrate;

  if (div > (STM32F4_USART_BRR_MANTISSA_MASK | STM32F4_USART_BRR_FRACTION_MASK) || div < 8U) {
    return -ENOTSUP;
  }

  *over8 = div < 16U;
  *brr = *over8 ? PBL_FIELD_PREP(STM32F4_USART_BRR_MANTISSA_MASK, div >> 3) | (div & 0x7U) : div;

  return 0;
}

static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);
  uint32_t cr1 = 0U;
  uint32_t cr3 = 0U;
  uint64_t rate_mhz;
  uint32_t brr;
  bool over8;
  int ret;

  switch (cfg->parity) {
    case PBL_UART_PARITY_NONE:
      break;
    case PBL_UART_PARITY_ODD:
      cr1 = STM32F4_USART_CR1_M | STM32F4_USART_CR1_PCE | STM32F4_USART_CR1_PS;
      break;
    case PBL_UART_PARITY_EVEN:
      cr1 = STM32F4_USART_CR1_M | STM32F4_USART_CR1_PCE;
      break;
    default:
      return -ENOTSUP;
  }

  switch (cfg->flow_ctrl) {
    case PBL_UART_FLOW_CTRL_NONE:
      break;
    case PBL_UART_FLOW_CTRL_RTS_CTS:
      if (!usart->rtscts) {
        return -ENOTSUP;
      }
      cr3 = CR3_FLOW;
      break;
    default:
      return -ENOTSUP;
  }

  ret = pbl_clk_get_rate(&usart->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  ret = prv_brr((uint32_t)(rate_mhz / MHZ_PER_HZ), cfg->baudrate, &brr, &over8);
  if (ret != 0) {
    return ret;
  }

  if (over8) {
    cr1 |= STM32F4_USART_CR1_OVER8;
  }

  prv_flush(uart);

  pbl_modifyl(STM32F4_USART_CR1_UE, 0U, prv_reg(usart, STM32F4_USART_CR1_OFFSET));
  pbl_modifyl(CR1_FRAME | STM32F4_USART_CR1_OVER8, cr1, prv_reg(usart, STM32F4_USART_CR1_OFFSET));
  pbl_modifyl(CR3_FLOW, cr3, prv_reg(usart, STM32F4_USART_CR3_OFFSET));
  prv_putreg(usart, STM32F4_USART_BRR_OFFSET, brr);
  pbl_modifyl(0U, STM32F4_USART_CR1_UE, prv_reg(usart, STM32F4_USART_CR1_OFFSET));

  return 0;
}

static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);

  while ((prv_getreg(usart, STM32F4_USART_SR_OFFSET) & STM32F4_USART_SR_TXE) == 0U) {
  }
  prv_putreg(usart, STM32F4_USART_DR_OFFSET, c);
  usart->state->tx_pending = true;
}

static int prv_rx_enable(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);

  pbl_modifyl(0U, STM32F4_USART_CR1_RXNEIE, prv_reg(usart, STM32F4_USART_CR1_OFFSET));

  return 0;
}

static void prv_rx_disable(const struct pbl_uart *uart) {
  const struct pbl_uart_stm32 *usart = prv_stm32(uart);

  pbl_modifyl(STM32F4_USART_CR1_RXNEIE, 0U, prv_reg(usart, STM32F4_USART_CR1_OFFSET));
}

void pbl_uart_stm32_irq_handler(const struct pbl_uart_stm32 *usart) {
  uint32_t sr = prv_getreg(usart, STM32F4_USART_SR_OFFSET);
  uint32_t cr1 = prv_getreg(usart, STM32F4_USART_CR1_OFFSET);
  uint8_t c;

  if ((cr1 & STM32F4_USART_CR1_RXNEIE) == 0U ||
      (sr & (STM32F4_USART_SR_RXNE | STM32F4_USART_SR_ORE)) == 0U) {
    return;
  }

  // Clears the error flags
  c = (uint8_t)prv_getreg(usart, STM32F4_USART_DR_OFFSET);

  if ((sr & STM32F4_USART_SR_ORE) != 0U) {
    usart->state->overruns++;
  }
  if ((sr & STM32F4_USART_SR_FE) != 0U) {
    usart->state->framing_errors++;
  }

  if ((sr & STM32F4_USART_SR_RXNE) != 0U && !pbl_uart_receive(&usart->uart, c)) {
    prv_rx_disable(&usart->uart);
  }
}

const struct pbl_uart_ops pbl_uart_stm32_ops = {
  .init = prv_init,
  .early_init = prv_early_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .flush = prv_flush,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};

#include PBL_DT_INSTANCES(st_stm32_uart)
