/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/battery.h>
#include <pbl/drivers/display.h>
#include <pbl/kernel/reboot.h>
#ifdef CONFIG_LEGACY_WATCH
#include <pbl/kernel/thread.h>
#endif
#include <pbl/logging/logging.h>

#include <kernel/util/standby.h>

int enter_standby(RebootReasonCode reason) {
  // Check before tearing anything down, so the caller can keep running
  if (battery_is_usb_connected_impl()) {
    PBL_LOG_WRN("Not entering standby, USB is connected");
    return -EBUSY;
  }

  PBL_LOG_ALWAYS("Preparing to enter standby mode (reason %u).", (unsigned)reason);

  RebootReason reboot_reason = {reason, 0};
  reboot_reason_set(&reboot_reason);

#ifdef CONFIG_LEGACY_WATCH
  // The clear is refused while a frame is going out, which left the last screen showing on the
  // memory LCD: wait for it, a frame times out after 200 ms.
  for (int i = 0; i < 20 && pbl_display_clear(DISPLAY) == -EBUSY; i++) {
    pbl_thread_sleep(PBL_MSEC(10));
  }
#else
  (void)pbl_display_clear(DISPLAY);
#endif
  (void)pbl_display_disable(DISPLAY);

  pbl_power_off();
}
