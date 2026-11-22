/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/spi.h>

//! Set up the SPI device.
int bmi160_spi_init(const struct pbl_spi_dev *spi);

//! Ask the chip to accept input from the SPI bus. Required after issuing a soft reset.
void bmi160_enable_spi_mode(void);

//! Burst read of @p len bytes from @p addr.
void bmi160_burst_read(uint8_t addr, void *buf, size_t len);

uint8_t bmi160_read_reg(uint8_t reg);
uint16_t bmi160_read_16bit_reg(uint8_t reg);
void bmi160_write_reg(uint8_t reg, uint8_t value);
