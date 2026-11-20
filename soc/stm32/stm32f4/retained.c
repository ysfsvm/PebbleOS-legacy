/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/mmio.h>
#include <pbl/services/time.h>
#include <pbl/system/rtc_registers.h>

#include <devicetree/regs.h>
#include <hardware/stm32f4_rtc.h>

// Timezone in backup registers 12 to 16, in the stock layout
static_assert(sizeof(TimezoneInfo) == 20, "TimezoneInfo differs from the stock firmware's");
static_assert(offsetof(TimezoneInfo, tm_zone) == 0 && offsetof(TimezoneInfo, dst_id) == 5 &&
                  offsetof(TimezoneInfo, timezone_id) == 6 &&
                  offsetof(TimezoneInfo, tm_gmtoff) == 8 &&
                  offsetof(TimezoneInfo, dst_start) == 12 && offsetof(TimezoneInfo, dst_end) == 16,
              "TimezoneInfo differs from the stock firmware's");

void retained_write(uint8_t id, uint32_t value) {
  pbl_writel(value, PBL_DT_REG_RTC + STM32F4_RTC_BKPR_OFFSET(id));
}

uint32_t retained_read(uint8_t id) {
  return pbl_readl(PBL_DT_REG_RTC + STM32F4_RTC_BKPR_OFFSET(id));
}
