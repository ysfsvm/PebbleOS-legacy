/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bmi160_private.h"
#include "bmi160_regs.h"

#include <pbl/drivers/spi.h>
#include <pbl/kernel/delay.h>

static const struct pbl_spi_dev *s_spi;

int bmi160_spi_init(const struct pbl_spi_dev *spi) {
  s_spi = spi;

  return pbl_spi_setup(spi);
}

void bmi160_burst_read(uint8_t addr, void *buf, size_t len) {
  if (len == 0U) {
    (void)pbl_spi_write(s_spi, &addr, 1U);
    return;
  }

  (void)pbl_spi_write_read(s_spi, &addr, 1U, buf, len);
}

uint8_t bmi160_read_reg(uint8_t reg) {
  uint8_t value;
  // Registers are read when the address MSB=1
  bmi160_burst_read(reg | BMI160_READ_FLAG, &value, 1);
  return value;
}

uint16_t bmi160_read_16bit_reg(uint8_t reg) {
  // 16-bit registers are little endian
  uint8_t value[2];
  bmi160_burst_read(reg | BMI160_READ_FLAG, value, sizeof(value));
  return value[0] | (value[1] << 8);
}

void bmi160_write_reg(uint8_t reg, uint8_t value) {
  const uint8_t buf[] = {reg & BMI160_REG_MASK, value};

  (void)pbl_spi_write(s_spi, buf, sizeof(buf));
}

void bmi160_enable_spi_mode(void) {
  // The BMI160 needs a rising edge on SCS to switch into SPI mode; the
  // datasheet recommends reading the reserved register 0x7F.
  bmi160_read_reg(0x7f);
  pbl_sleep_ms(2); // Necessary on cold boots
}
