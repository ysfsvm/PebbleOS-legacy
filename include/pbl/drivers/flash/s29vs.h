/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/flash.h>

//! @file s29vs.h
//! Spansion S29VS parallel NOR on the STM32F439 FMC (Pebble Time, Time Round).
//!
//! The NOR is memory mapped, but only readable while the FMC is clocked and its pins are
//! configured: memory-mapped readers hold a use for as long as they keep pointers into it.

extern const struct pbl_flash pbl_flash_s29vs;

//! The S29VS, for a board's FLASH.
#define S29VS_FLASH (&pbl_flash_s29vs)

//! Base of the window the NOR is mapped at.
#define S29VS_MAPPED_ADDRESS 0x60000000U

//! Keep the NOR readable through its window.
void s29vs_use(void);

//! Drop @p num uses taken with s29vs_use().
void s29vs_release_many(uint32_t num);
