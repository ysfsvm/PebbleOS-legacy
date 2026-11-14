/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash/flash_guard.h>

typedef struct {
  uint32_t begin;
  uint32_t end;
} FlashGuardRange;

static const FlashGuardRange s_protected[] = {
  {PART_SAFE_FIRMWARE_OFFSET, PART_SAFE_FIRMWARE_OFFSET + PART_SAFE_FIRMWARE_SIZE},
  {PART_STOCK_SHARED_PRF_STORAGE_OFFSET,
   PART_STOCK_SHARED_PRF_STORAGE_OFFSET + PART_STOCK_SHARED_PRF_STORAGE_SIZE},
  {PART_MFG_INFO_OFFSET, PART_MFG_INFO_OFFSET + PART_MFG_INFO_SIZE},
};

bool flash_guard_is_protected(uint32_t addr, uint32_t len) {
  if (len == 0) {
    return false;
  }
  // 64-bit end so a bogus length cannot wrap past a protected range.
  const uint64_t end = (uint64_t)addr + len;
  for (unsigned i = 0; i < sizeof(s_protected) / sizeof(s_protected[0]); i++) {
    if (addr < s_protected[i].end && end > s_protected[i].begin) {
      return true;
    }
  }
  return false;
}
