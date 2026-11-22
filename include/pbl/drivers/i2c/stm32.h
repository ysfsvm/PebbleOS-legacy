/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/reset.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_i2c_stm32 STM32F4
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c bus driver for the STM32F4 I2C block (v1), in master mode.
 *
 * Interrupt driven, with bus recovery:
 *
 * @code{.c}
 * PBL_I2C_STM32_DEFINE(s_i2c1, "i2c1", 0x40005400, 31, 32,
 *                      PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB1_CLOCK(I2C1))),
 *                      PBL_RESET(&dt_rcc_reset, STM32F4_APB1_RESET(I2C1)), 400000,
 *                      &s_i2c1_default, PBL_GPIO(&dt_gpiob, 6, 0), PBL_GPIO(&dt_gpiob, 9, 0));
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
enum pbl_i2c_stm32_xfer_state {
  PBL_I2C_STM32_START,
  PBL_I2C_STM32_ADDRESS_TX,
  PBL_I2C_STM32_ADDRESS_RX,
  PBL_I2C_STM32_WRITE_DATA,
  PBL_I2C_STM32_END_WRITE,
  PBL_I2C_STM32_READ_DATA,
  PBL_I2C_STM32_COMPLETE,
};

struct pbl_i2c_stm32_state {
  enum pbl_i2c_stm32_xfer_state xfer_state;
  struct pbl_i2c_msg *msgs;
  uint8_t num_msgs;
  uint8_t msg;
  uint16_t addr;
  uint32_t idx;
  bool stop_blocked;
};
/** @endcond */

/** @brief STM32F4 I2C bus. */
struct pbl_i2c_stm32 {
  /** I2C bus. */
  struct pbl_i2c_bus bus;
  /** Driver runtime state. */
  struct pbl_i2c_stm32_state *state;
  /** Registers. */
  uintptr_t base;
  /** Event interrupt line. */
  pbl_irq_t ev_irq;
  /** Error interrupt line. */
  pbl_irq_t er_irq;
  /** Clock. */
  struct pbl_clk clk;
  /** Reset line. */
  struct pbl_reset reset;
  /** Bus clock frequency, in Hz. */
  uint32_t clock_hz;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
  /** SCL, as a GPIO for bus recovery. */
  struct pbl_gpio scl;
  /** SDA, as a GPIO for bus recovery. */
  struct pbl_gpio sda;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_i2c_bus_ops pbl_i2c_stm32_ops;
/** @endcond */

/**
 * @brief Define an STM32F4 I2C bus.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _base Registers.
 * @param _ev_irq Event interrupt line.
 * @param _er_irq Error interrupt line.
 * @param _clk Clock, from PBL_CLK().
 * @param _reset Reset line, from PBL_RESET().
 * @param _clock_hz Bus clock frequency, in Hz.
 * @param _pinctrl Default pin control state.
 * @param _scl SCL pin, from PBL_GPIO().
 * @param _sda SDA pin, from PBL_GPIO().
 */
#define PBL_I2C_STM32_DEFINE(sym, _name, _base, _ev_irq, _er_irq, _clk, _reset, _clock_hz, \
                             _pinctrl, _scl, _sda)                                         \
  PBL_I2C_BUS_STATE_DEFINE(sym);                                                           \
  static struct pbl_i2c_stm32_state sym##_stm32_state;                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_i2c_stm32, struct pbl_i2c_bus, bus){                 \
    .bus = PBL_I2C_BUS_INIT(sym, _name, &pbl_i2c_stm32_ops, nullptr),                      \
    .state = &sym##_stm32_state,                                                           \
    .base = (_base),                                                                       \
    .ev_irq = (_ev_irq),                                                                   \
    .er_irq = (_er_irq),                                                                   \
    .clk = _clk,                                                                           \
    .reset = _reset,                                                                       \
    .clock_hz = (_clock_hz),                                                               \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                          \
    .scl = _scl,                                                                           \
    .sda = _sda,                                                                           \
  };                                                                                       \
  PBL_IRQ_CONNECT_NUM(_ev_irq, CONFIG_I2C_STM32_IRQ_PRIO, pbl_i2c_stm32_ev_irq_handler,    \
                      &sym##_inst, 0);                                                     \
  PBL_IRQ_CONNECT_NUM(_er_irq, CONFIG_I2C_STM32_IRQ_PRIO, pbl_i2c_stm32_er_irq_handler,    \
                      &sym##_inst, 0)

/**
 * @brief Event interrupt handler, connected by PBL_I2C_STM32_DEFINE().
 *
 * @param i2c Bus.
 */
void pbl_i2c_stm32_ev_irq_handler(const struct pbl_i2c_stm32 *i2c);

/**
 * @brief Error interrupt handler, connected by PBL_I2C_STM32_DEFINE().
 *
 * @param i2c Bus.
 */
void pbl_i2c_stm32_er_irq_handler(const struct pbl_i2c_stm32 *i2c);

/** @} */
