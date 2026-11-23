/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <setjmp.h>

#include <pbl/logging/logging.h>
#include <pbl/util/size.h>

#include <clar.h>

#undef PBL_LOG_WRN
#define PBL_LOG_WRN(...)
#undef PBL_LOG_SYNC_WRN
#define PBL_LOG_SYNC_WRN(...)

#include "../../../soc/stm32/stm32f4/hardlockup_test.c"

static RebootReasonCode s_last_reason;
static bool s_iwdg_reset;
static bool s_unread_dump;
static bool s_updating;
static int s_armed;
static uint32_t s_armed_ticks;
static jmp_buf s_spin_jmp;

struct pbl_workq pbl_sys_workq;

void reboot_reason_get(RebootReason *reason) {
  *reason = (RebootReason){.code = s_last_reason};
}

bool watchdog_check_reset_flag(void) {
  return s_iwdg_reset;
}

int pbl_coredump_stored_find(bool unread_only, struct pbl_coredump_stored *stored) {
  cl_assert(unread_only);
  return s_unread_dump ? 0 : -ENOENT;
}

bool firmware_update_is_in_progress(void) {
  return s_updating;
}

bool pbl_delayed_work_reschedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                                 pbl_timeout_t delay) {
  s_armed++;
  s_armed_ticks = delay.ticks;
  return false;
}

void pbl_irq_lock(void) {
  longjmp(s_spin_jmp, 1);
}

static bool prv_locks_up(void) {
  if (setjmp(s_spin_jmp) == 0) {
    prv_lock_up(&s_lock_up.work);
    return false;
  }
  return true;
}

void test_hardlockup_test__initialize(void) {
  s_last_reason = RebootReasonCode_Unknown;
  s_iwdg_reset = false;
  s_unread_dump = false;
  s_updating = false;
  s_armed = 0;
  s_armed_ticks = 0U;
}

void test_hardlockup_test__arms_a_minute_after_a_clean_boot(void) {
  cl_assert_equal_i(prv_init(&s_dev), 0);
  cl_assert_equal_i(s_armed, 1);
  cl_assert_equal_i(s_armed_ticks, PBL_MSEC(DELAY_MS).ticks);
}

void test_hardlockup_test__not_after_a_hardware_watchdog_reset(void) {
  s_iwdg_reset = true;
  cl_assert_equal_i(prv_init(&s_dev), 0);
  cl_assert_equal_i(s_armed, 0);
}

void test_hardlockup_test__not_after_a_lockup_or_a_crash(void) {
  static const RebootReasonCode s_codes[] = {
    RebootReasonCode_HardLockup,
    RebootReasonCode_CoreDumpEntryFailed,
    RebootReasonCode_Watchdog,
    RebootReasonCode_Assert,
  };

  for (unsigned int i = 0; i < ARRAY_LENGTH(s_codes); i++) {
    s_last_reason = s_codes[i];
    cl_assert_equal_i(prv_init(&s_dev), 0);
  }
  cl_assert_equal_i(s_armed, 0);
}

void test_hardlockup_test__locks_up(void) {
  cl_assert(prv_locks_up());
}

void test_hardlockup_test__not_with_an_unread_dump(void) {
  s_unread_dump = true;
  cl_assert(!prv_locks_up());
}

void test_hardlockup_test__not_during_a_firmware_update(void) {
  s_updating = true;
  cl_assert(!prv_locks_up());
}
