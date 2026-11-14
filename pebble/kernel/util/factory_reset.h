/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Factory resets the device by wiping the flash
//! @param should_shutdown If true, shutdown after factory resetting, otherwise reboot.
void factory_reset(bool should_shutdown);

#ifdef CONFIG_LEGACY_FACTORY_RESET_KEEPS_FIRMWARE
//! At boot: finish a filesystem wipe a reset cut short.
void factory_reset_finish_pending_wipe(void);
#endif

//! Factory resets the device by deleting all files
void factory_reset_fast(void *unused);

//! Returns true if a factory reset is in progress.
bool factory_reset_ongoing(void);
