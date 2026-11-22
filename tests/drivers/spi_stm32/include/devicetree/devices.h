/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/stm32f4.h>
#include <pbl/drivers/reset/stm32f4.h>
#include <pbl/drivers/spi.h>

#include <dt-bindings/mfd/stm32f4-rcc.h>

extern const struct pbl_clk_controller dt_rcc;
extern const struct pbl_reset_controller dt_rcc_reset;
extern const struct pbl_spi_bus dt_spi1;
extern const struct pbl_spi_bus dt_spi2;
