/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pwm.h>

/**
 * @defgroup drivers_pwm_stm32 STM32 timer PWM
 * @ingroup drivers_pwm
 * @brief @ref drivers_pwm controller driver for the output channels of an STM32F4 timer.
 *
 * Channels 0 to 3 are the timer's CH1 to CH4. They share one counter, so channels running at the
 * same time have the same period; applying another period fails with @c -EBUSY. The counter is
 * 16 bits, with the smallest prescaler that fits the period. STOP is held off while a channel
 * runs; idle outputs take the sleep pin state:
 *
 * @code{.c}
 * PBL_PWM_STM32_DEFINE(s_pwm12, "pwm12", 0x40001800,
 *                      PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB1_CLOCK(TIM12))),
 *                      &s_pwm12_default, &s_pwm12_sleep);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_pwm_stm32_state {
  uint16_t prescaler;
  uint16_t reload;
  uint8_t running;
};
/** @endcond */

/** @brief STM32 timer PWM controller. */
struct pbl_pwm_stm32 {
  /** PWM controller. */
  struct pbl_pwm_controller ctrl;
  /** Driver runtime state. */
  struct pbl_pwm_stm32_state *state;
  /** Timer registers. */
  uintptr_t base;
  /** Timer kernel clock. */
  struct pbl_clk clk;
  /** Output pins: default while running, sleep while idle. */
  struct pbl_pinctrl pinctrl;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pwm_controller_ops pbl_pwm_stm32_ops;
int pbl_pwm_stm32_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an STM32 timer PWM controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Timer registers.
 * @param _clk Timer clock, from PBL_CLK().
 * @param _default Pin state while a channel runs.
 * @param _sleep Pin state while none runs.
 */
#define PBL_PWM_STM32_DEFINE(sym, _name, _base, _clk, _default, _sleep)                           \
  PBL_PWM_CONTROLLER_STATE_DEFINE(sym, 4);                                                        \
  static struct pbl_pwm_stm32_state sym##_stm32_state;                                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_pwm_stm32, struct pbl_pwm_controller, ctrl) {               \
    .ctrl = PBL_PWM_CONTROLLER_INIT(sym, _name, pbl_pwm_stm32_init, nullptr, &pbl_pwm_stm32_ops), \
    .state = &sym##_stm32_state, .base = (_base), .clk = _clk,                                    \
    .pinctrl = PBL_PINCTRL(_default, _sleep, 0),                                                  \
  }

/** @} */
