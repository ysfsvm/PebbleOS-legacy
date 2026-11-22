/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/stm32f4.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/intc.h>

extern const struct pbl_clk_controller dt_rcc;
extern const struct pbl_gpio_port dt_gpioa;
extern const struct pbl_intc dt_gpioa_intc;
extern const struct pbl_gpio_port dt_gpiog;
extern const struct pbl_intc dt_gpiog_intc;
