/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/stm32f4.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pinctrl/stm32.h>
#include <pbl/drivers/pwm.h>

extern const struct pbl_clk_controller dt_rcc;
extern const struct pbl_gpio_port dt_gpiob;
extern const struct pbl_intc dt_gpiob_intc;
extern const struct pbl_gpio_port dt_gpiof;
extern const struct pbl_intc dt_gpiof_intc;
extern const struct pbl_pinctrl_state dt_pwm10_default;
extern const struct pbl_pinctrl_state dt_pwm10_sleep;
extern const struct pbl_pwm_controller dt_pwm10;
extern const struct pbl_pinctrl_state dt_pwm12_default;
extern const struct pbl_pinctrl_state dt_pwm12_sleep;
extern const struct pbl_pwm_controller dt_pwm12;
