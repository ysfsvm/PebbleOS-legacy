/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! STM32F4 OTP area. Read-only: the slots were written at the factory and
//! writing OTP cannot be undone, so writes are always refused.

#include <pbl/drivers/otp.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mmio.h>

#include <hardware/stm32f4_otp.h>

static_assert(NUM_OTP_SLOTS <= STM32F4_OTP_NUM_BLOCKS, "more slots than OTP blocks");

char *otp_get_slot(const uint8_t index) {
  PBL_ASSERTN(index < NUM_OTP_SLOTS);
  return (char *)(uintptr_t)(STM32F4_OTP_BASE + (STM32F4_OTP_BLOCK_SIZE * index));
}

uint8_t *otp_get_lock(const uint8_t index) {
  PBL_ASSERTN(index < NUM_OTP_SLOTS);
  return (uint8_t *)(uintptr_t)(STM32F4_OTP_LOCK_BASE + index);
}

bool otp_is_locked(const uint8_t index) {
  return pbl_readb((uintptr_t)otp_get_lock(index)) == STM32F4_OTP_LOCKED;
}

OtpWriteResult otp_write_slot(const uint8_t index, const char *value) {
  return otp_is_locked(index) ? OtpWriteFailAlreadyWritten : OtpWriteFailCorrupt;
}
