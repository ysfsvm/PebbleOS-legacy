/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <setjmp.h>
#include <stdint.h>

/*!
  Initialize the fake SPI flash region.

  @param offset the offset at which this fake region of flash begins.
  @param length the length of this fake region of flash.
 */
void fake_spi_flash_init(uint32_t offset, uint32_t length);

//! Add data to the fake spi flash from a file on your local filesystem
void fake_spi_flash_populate_from_file(char *path, uint32_t offset);

//! Cleanup the fake SPI flash region, freeing all resources.
void fake_spi_flash_erase(void);

//! Cleanup the fake SPI flash region, freeing all resources.
void fake_spi_flash_cleanup(void);

//! Force the SPI flash to fail at some point in the future, jumping back to
//! the given jmp_buf when it does so. This is intended for use in verifying
//! the atomicity of algorithms which are purported to be so.
void fake_spi_flash_force_future_failure(int after_n_bytes, jmp_buf *retire_to);

typedef enum {
  FakeFlashCutBefore = 0,
  FakeFlashCutHalf,
  FakeFlashCutBits,
} FakeFlashCut;

//! Cut power at program or erase operation after_n_ops
void fake_spi_flash_force_op_failure(uint32_t after_n_ops, FakeFlashCut cut, jmp_buf *retire_to);

uint32_t fake_flash_op_count(void);

void fake_spi_flash_save(uint8_t *buf, uint32_t addr, uint32_t len);
void fake_spi_flash_restore(const uint8_t *buf, uint32_t addr, uint32_t len);

void fake_flash_assert_untouched_outside(uint32_t begin, uint32_t end);

uint32_t fake_flash_read_count(void);
uint32_t fake_flash_write_count(void);
uint32_t fake_flash_erase_count(void);

//! Erases that touched the sector holding addr
uint32_t fake_flash_sector_erase_count(uint32_t addr);
