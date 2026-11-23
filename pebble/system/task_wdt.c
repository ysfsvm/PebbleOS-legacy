/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/task_wdt/hardlockup.h>
#include <pbl/task_wdt/task_wdt.h>

#include <system/bootbits.h>
#include <system/die.h>
#include <system/reboot_reason.h>

static bool prv_reboot_reason_is_ours(void) {
  RebootReason current;
  reboot_reason_get(&current);
  return current.code == RebootReasonCode_Unknown || current.code == RebootReasonCode_Watchdog;
}

void pbl_task_wdt_stalled(const struct pbl_task_wdt_stall *stall) {
  RebootReason reason = {
    .code = RebootReasonCode_Watchdog,
    .data8 = {stall->fed_mask, stall->active_mask},
  };
  reason.watchdog.stuck_task_pc = stall->pc;
  reason.watchdog.stuck_task_lr = stall->lr;
  reason.watchdog.stuck_task_callback = (uint32_t)(uintptr_t)stall->work;

  if (prv_reboot_reason_is_ours()) {
    reboot_reason_clear();
    reboot_reason_set(&reason);
  }
}

void pbl_task_wdt_recovered(void) {
  if (prv_reboot_reason_is_ours()) {
    reboot_reason_clear();
  }
}

[[noreturn]] void pbl_task_wdt_expired(void) {
  reset_due_to_software_failure();
}

#ifdef CONFIG_TASK_WDT_HARDLOCKUP
// Runs from the detector's interrupt
void pbl_task_wdt_hardlockup(const struct pbl_task_wdt_hardlockup *lockup) {
  RebootReason reason = {.code = RebootReasonCode_HardLockup};

  reason.hard_lockup.pc = lockup->pc;
  reason.hard_lockup.lr = lockup->lr;
  reason.hard_lockup.exception = lockup->exception;
  if (prv_reboot_reason_is_ours()) {
    reboot_reason_clear();
    reboot_reason_set(&reason);
  }
#ifndef CONFIG_MFG
  boot_bit_set(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);
#endif
  reset_with_core_dump(false);
}
#endif
