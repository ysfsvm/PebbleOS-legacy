/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! Test of the hard lockup detector (CONFIG_LEGACY_HARDLOCKUP_TEST): a minute after boot, spin
//! with the kernel's interrupts locked, once per stored core dump.

#include <pbl/coredump/coredump.h>
#include <pbl/device.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>

#define DELAY_MS 60000U

static void prv_lock_up(struct pbl_work *work) {
  struct pbl_coredump_stored stored;

  if (pbl_coredump_stored_find(true, &stored) == 0) {
    PBL_LOG_WRN("Hard lockup test: an unread core dump is stored, not locking up");
    return;
  }

  PBL_LOG_SYNC_WRN("Hard lockup test: spinning with interrupts locked");
  pbl_irq_lock();
  for (;;) {
  }
}

static PBL_DELAYED_WORK_DEFINE(s_lock_up, prv_lock_up);

static int prv_init(const struct pbl_device *dev) {
  pbl_delayed_work_reschedule(&pbl_sys_workq, &s_lock_up, PBL_MSEC(DELAY_MS));
  return 0;
}

PBL_DEVICE_STATE_DEFINE(hardlockup_test);
static const struct pbl_device s_dev =
    PBL_DEVICE_INIT(hardlockup_test, "hardlockup_test", prv_init, nullptr, nullptr);
PBL_DEVICE_REGISTER(hardlockup_test, &s_dev);
