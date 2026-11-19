/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

//! Hard lockup detector test: spin with interrupts locked a minute after boot.

#include <pbl/coredump/coredump.h>
#include <pbl/device.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/firmware_update.h>

#include <system/reboot_reason.h>

#define DELAY_MS 60000U

static void prv_lock_up(struct pbl_work *work) {
  struct pbl_coredump_stored stored;

  if (pbl_coredump_stored_find(true, &stored) == 0) {
    PBL_LOG_WRN("Hard lockup test: an unread core dump is stored, not locking up");
    return;
  }
  if (firmware_update_is_in_progress()) {
    PBL_LOG_WRN("Hard lockup test: a firmware update runs, not locking up");
    return;
  }

  PBL_LOG_SYNC_WRN("Hard lockup test: spinning with interrupts locked");
  pbl_irq_lock();
  for (;;) {
  }
}

static PBL_DELAYED_WORK_DEFINE(s_lock_up, prv_lock_up);

static int prv_init(const struct pbl_device *dev) {
  RebootReason reason;

  reboot_reason_get(&reason);
  if (reason.code >= RebootReasonCode_Watchdog || watchdog_check_reset_flag()) {
    PBL_LOG_WRN("Hard lockup test: the last boot ended in a crash, not locking up");
    return 0;
  }

  pbl_delayed_work_reschedule(&pbl_sys_workq, &s_lock_up, PBL_MSEC(DELAY_MS));
  return 0;
}

PBL_DEVICE_STATE_DEFINE(hardlockup_test);
static const struct pbl_device s_dev =
    PBL_DEVICE_INIT(hardlockup_test, "hardlockup_test", prv_init, nullptr, nullptr);
PBL_DEVICE_REGISTER(hardlockup_test, &s_dev);
