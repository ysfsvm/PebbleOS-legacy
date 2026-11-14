/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource_mapped.h"

#include <stdint.h>

#include <pbl/drivers/flash.h>
#include <pbl/drivers/flash/s29vs.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>

// Per task, so a process that exits holding mapped bytes releases them.
static uint16_t s_refs[NumPebbleTask];

void resource_mapped_use(PebbleTask task) {
  PBL_ASSERTN(task < NumPebbleTask);
  s29vs_use();
  pbl_irq_lock();
  s_refs[task]++;
  pbl_irq_unlock();
}

void resource_mapped_release(PebbleTask task) {
  PBL_ASSERTN(task < NumPebbleTask);
  pbl_irq_lock();
  PBL_ASSERTN(s_refs[task] > 0);
  s_refs[task]--;
  pbl_irq_unlock();
  s29vs_release_many(1);
}

void resource_mapped_release_all(PebbleTask task) {
  PBL_ASSERTN(task < NumPebbleTask);
  pbl_irq_lock();
  const uint32_t refs = s_refs[task];
  s_refs[task] = 0;
  pbl_irq_unlock();
  if (refs > 0) {
    s29vs_release_many(refs);
  }
}
