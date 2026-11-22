/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#include "../../../drivers/otp/otp_stm32.c"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/otp.h>

#include <clar.h>
#include <mfg/mfg_serials.h>
#include <sys/mman.h>

#define OTP_PAGE  0x1fff7000U
#define OTP_SLOTS 0x1fff7800U
#define OTP_LOCKS 0x1fff7a00U
#define SLOT_SIZE 32U
#define NUM_SLOTS 16U

static uint8_t *const s_page = (uint8_t *)(uintptr_t)OTP_PAGE;
static uint8_t s_before[4096];

static void prv_map(void) {
  void *p = mmap(s_page, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  cl_assert(p == s_page);
  memset(s_page, 0xff, 4096);
}

static void prv_slot(uint8_t index, const char *value, bool locked) {
  char *slot = (char *)(uintptr_t)(OTP_SLOTS + SLOT_SIZE * index);

  memset(slot, 0xff, SLOT_SIZE);
  if (value != NULL) {
    strcpy(slot, value);
  }
  s_page[OTP_LOCKS - OTP_PAGE + index] = locked ? 0x00U : 0xffU;
}

static void prv_seal(void) {
  memcpy(s_before, s_page, sizeof(s_before));
  cl_assert_equal_i(mprotect(s_page, 4096, PROT_READ), 0);
}

static void prv_assert_unchanged(void) {
  cl_assert(memcmp(s_before, s_page, sizeof(s_before)) == 0);
}

void test_otp_stm32__initialize(void) {
  prv_map();
}

void test_otp_stm32__cleanup(void) {
  cl_assert_equal_i(munmap(s_page, 4096), 0);
}

void test_otp_stm32__slots_and_locks_where_the_manual_puts_them(void) {
  prv_seal();
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    cl_assert(otp_get_slot(i) == (char *)(uintptr_t)(OTP_SLOTS + SLOT_SIZE * i));
    cl_assert(otp_get_lock(i) == (uint8_t *)(uintptr_t)(OTP_LOCKS + i));
  }
  prv_assert_unchanged();
}

void test_otp_stm32__a_slot_is_locked_only_with_its_lock_byte_at_zero(void) {
  prv_slot(0, "TESTSERIAL01", true);
  prv_slot(1, "TESTSERIAL02", false);
  s_page[OTP_LOCKS - OTP_PAGE + 2] = 0x5aU;
  prv_seal();
  cl_assert(otp_is_locked(0));
  cl_assert(!otp_is_locked(1));
  cl_assert(!otp_is_locked(2));
  cl_assert(!otp_is_locked(15));
  prv_assert_unchanged();
}

void test_otp_stm32__writing_a_slot_is_refused_without_a_write(void) {
  prv_slot(OTP_SERIAL1, "TESTSERIAL01", true);
  prv_seal();
  for (uint8_t i = 0; i < NUM_SLOTS; i++) {
    const OtpWriteResult expected =
        (i == OTP_SERIAL1) ? OtpWriteFailAlreadyWritten : OtpWriteFailCorrupt;
    cl_assert_equal_i(otp_write_slot(i, "TESTSERIAL99"), expected);
  }
  prv_assert_unchanged();
}

void test_otp_stm32__a_slot_out_of_range_panics(void) {
  prv_seal();
  cl_assert_panic(otp_get_slot(NUM_SLOTS));
  cl_assert_panic(otp_get_lock(NUM_SLOTS));
  cl_assert_panic(otp_write_slot(NUM_SLOTS, "TESTSERIAL99"));
  prv_assert_unchanged();
}

void test_otp_stm32__serials_from_the_latest_locked_slot(void) {
  prv_slot(OTP_SERIAL1, "TESTSERIAL01", true);
  prv_slot(OTP_SERIAL2, "TESTSERIAL02", true);
  prv_slot(OTP_SERIAL3, "TESTSERIAL03", false);
  prv_slot(OTP_HWVER1, "TESTHW001", true);
  prv_slot(OTP_PCBA_SERIAL1, "TESTPCBA0001", true);
  prv_slot(OTP_PCBA_SERIAL3, "TESTPCBA0003", true);
  prv_seal();
  cl_assert_equal_s(mfg_get_serial_number(), "TESTSERIAL02");
  cl_assert_equal_s(mfg_get_hw_version(), "TESTHW001");
  cl_assert_equal_s(mfg_get_pcba_serial_number(), "TESTPCBA0003");
  prv_assert_unchanged();
}

void test_otp_stm32__placeholders_without_a_locked_slot(void) {
  prv_seal();
  cl_assert_equal_s(mfg_get_serial_number(), "XXXXXXXXXXXX");
  cl_assert_equal_s(mfg_get_hw_version(), "XXXXXXXX");
  cl_assert_equal_s(mfg_get_pcba_serial_number(), "XXXXXXXXXXXX");
  prv_assert_unchanged();
}

void test_otp_stm32__the_factory_writers_find_no_room_and_write_nothing(void) {
  uint8_t index = 0xaaU;

  prv_slot(OTP_SERIAL1, "TESTSERIAL01", true);
  prv_seal();
  cl_assert_equal_i(mfg_write_serial_number("TESTSERIAL99", 12, &index),
                    MfgSerialsResultFailNoMoreSpace);
  cl_assert_equal_i(mfg_write_hw_version("TESTHW099", 9, &index), MfgSerialsResultFailNoMoreSpace);
  cl_assert_equal_i(mfg_write_pcba_serial_number("TESTPCBA0099", 12, &index),
                    MfgSerialsResultFailNoMoreSpace);
  cl_assert_equal_i(index, 0xaaU);
  prv_assert_unchanged();
}
