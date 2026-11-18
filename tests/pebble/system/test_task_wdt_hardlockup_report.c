/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <setjmp.h>
#include <string.h>

#include <pbl/task_wdt/hardlockup.h>

#include <clar.h>
#include <system/bootbits.h>
#include <system/die.h>
#include <system/reboot_reason.h>

// The firmware's hard lockup report, which runs from the detector's interrupt and ends in a core
// dump: what it leaves in the retained registers for the next boot and the bootloader.

static RebootReason s_reason;
static uint32_t s_boot_bits;
static uint32_t s_boot_bits_at_dump;
static int s_dumps;
static jmp_buf s_dump_jmp;

void reboot_reason_get(RebootReason *reason) {
  *reason = s_reason;
}

void reboot_reason_clear(void) {
  memset(&s_reason, 0, sizeof(s_reason));
}

// First writer wins, as the firmware's
void reboot_reason_set(RebootReason *reason) {
  if (s_reason.code == RebootReasonCode_Unknown) {
    s_reason = *reason;
  }
}

void boot_bit_set(BootBitValue bit) {
  s_boot_bits |= bit;
}

[[noreturn]] void reset_with_core_dump(bool is_forced) {
  s_dumps++;
  s_boot_bits_at_dump = s_boot_bits;
  longjmp(s_dump_jmp, 1);
}

[[noreturn]] void reset_due_to_software_failure(void) {
  cl_fail("reset_due_to_software_failure");
  for (;;) {
  }
}

static void prv_report(void) {
  if (setjmp(s_dump_jmp) == 0) {
    pbl_task_wdt_hardlockup(&(struct pbl_task_wdt_hardlockup){
      .pc = 0x08001234U,
      .lr = 0x08005678U,
      .exception = 0U,
      .awake_ms = 4524U,
    });
    cl_fail("the report returned");
  }
}

void test_task_wdt_hardlockup_report__initialize(void) {
  memset(&s_reason, 0, sizeof(s_reason));
  s_boot_bits = 0U;
  s_boot_bits_at_dump = 0U;
  s_dumps = 0;
}

void test_task_wdt_hardlockup_report__stores_the_lockup(void) {
  prv_report();

  cl_assert_equal_i(s_dumps, 1);
  cl_assert_equal_i(s_reason.code, RebootReasonCode_HardLockup);
  cl_assert_equal_i(s_reason.hard_lockup.pc, 0x08001234U);
  cl_assert_equal_i(s_reason.hard_lockup.lr, 0x08005678U);
}

//! As the assert and task watchdog paths: the bootloader counts it as a failed boot.
void test_task_wdt_hardlockup_report__counts_as_a_software_failure(void) {
  prv_report();

  cl_assert(s_boot_bits_at_dump & BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);
}

void test_task_wdt_hardlockup_report__replaces_a_task_watchdog_stall(void) {
  s_reason.code = RebootReasonCode_Watchdog;
  s_reason.watchdog.stuck_task_pc = 0x08009999U;

  prv_report();

  cl_assert_equal_i(s_reason.code, RebootReasonCode_HardLockup);
  cl_assert_equal_i(s_reason.hard_lockup.pc, 0x08001234U);
}
