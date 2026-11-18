/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_uart_stm32 STM32F4
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver for the STM32F4 USART.
 *
 * Polled TX, interrupt-driven RX:
 *
 * @code{.c}
 * PBL_UART_STM32_DEFINE(s_usart3, "usart3", 0x40004800, 39,
 *                       PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB1_CLOCK(UART3))), 115200,
 *                       false, &s_usart3_default);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_uart_stm32_state {
  bool tx_pending;
  uint32_t overruns;
  uint32_t framing_errors;
};
/** @endcond */

/** @brief STM32F4 serial port. */
struct pbl_uart_stm32 {
  /** Serial port. */
  struct pbl_uart uart;
  /** Driver runtime state. */
  struct pbl_uart_stm32_state *state;
  /** Registers. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Clock. */
  struct pbl_clk clk;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
  /** RTS and CTS are wired. */
  bool rtscts;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_stm32_ops;

#define PBL_UART_STM32_FIRST_(_first, ...) _first
/** @endcond */

/**
 * @brief Define a serial port on an STM32F4 USART.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line.
 * @param _clk Clock, from PBL_CLK().
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _rtscts true if RTS and CTS are wired.
 * @param ... Default pin control state, if the port has pins.
 */
#define PBL_UART_STM32_DEFINE(sym, _name, _base, _irq, _clk, _baudrate, _rtscts, ...)              \
  PBL_UART_STATE_DEFINE(sym);                                                                      \
  static struct pbl_uart_stm32_state sym##_stm32_state;                                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_uart_stm32, struct pbl_uart, uart){                          \
    .uart = PBL_UART_INIT(sym, _name, &pbl_uart_stm32_ops, _baudrate, nullptr),                    \
    .state = &sym##_stm32_state,                                                                   \
    .base = (_base),                                                                               \
    .irq = (_irq),                                                                                 \
    .clk = _clk,                                                                                   \
    .pinctrl = PBL_PINCTRL(PBL_UART_STM32_FIRST_(__VA_ARGS__ __VA_OPT__(, ) nullptr), nullptr, 0), \
    .rtscts = (_rtscts),                                                                           \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UART_STM32_IRQ_PRIO, pbl_uart_stm32_irq_handler, &sym##_inst, 0)

/**
 * @brief USART interrupt handler, connected by PBL_UART_STM32_DEFINE().
 *
 * @param uart Port.
 */
void pbl_uart_stm32_irq_handler(const struct pbl_uart_stm32 *uart);

/** @} */
