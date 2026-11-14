/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! STM32F4 RTC wall clock. The calendar holds UTC with a two-digit year (2000-2099), as the stock
//! bootloader and recovery firmware expect. The hardware set-up and the calendar access are the
//! system timer's (drivers/timer/stm32_rtc.c), which keeps its clock continuous when the
//! calendar is set.

#include <errno.h>

#include <pbl/drivers/rtc.h>
#include <pbl/drivers/rtc/stm32.h>
#include <pbl/drivers/timer/stm32_rtc.h>
#include <pbl/services/time.h>
#include <pbl/soc/stm32/cmsis.h>

static uint32_t prv_bcd2bin(uint32_t bcd) {
  return ((bcd >> 4) * 10U) + (bcd & 0xFU);
}

static uint32_t prv_bin2bcd(uint32_t bin) {
  return ((bin / 10U) << 4) | (bin % 10U);
}

static int prv_get_time(const struct pbl_rtc *rtc, time_t *t) {
  uint32_t tr;
  uint32_t dr;
  uint32_t subsec;

  stm32_rtc_calendar_get_subsec(&tr, &dr, &subsec);

  struct pbl_tm tm = {
    .tm_sec = (int)prv_bcd2bin(tr & (RTC_TR_ST | RTC_TR_SU)),
    .tm_min = (int)prv_bcd2bin((tr & (RTC_TR_MNT | RTC_TR_MNU)) >> 8),
    .tm_hour = (int)prv_bcd2bin((tr & (RTC_TR_HT | RTC_TR_HU)) >> 16),
    .tm_mday = (int)prv_bcd2bin(dr & (RTC_DR_DT | RTC_DR_DU)),
    .tm_mon = (int)prv_bcd2bin((dr & (RTC_DR_MT | RTC_DR_MU)) >> 8) - 1,
    .tm_year = (int)prv_bcd2bin((dr & (RTC_DR_YT | RTC_DR_YU)) >> 16) + 100,
  };

  // Another firmware may have left an invalid date behind.
  if (tm.tm_mday < 1 || tm.tm_mon < 0 || tm.tm_mon > 11) {
    return -EIO;
  }

  // To the nearest second: the time saved before a restart was rounded so too, and truncating
  // here lost half a second on average across every restart.
  *t = pbl_mktime(&tm) + ((subsec >= (STM32_RTC_SUBSEC_HZ / 2U)) ? 1 : 0);

  return 0;
}

static int prv_set_time(const struct pbl_rtc *rtc, time_t t) {
  struct pbl_tm tm;

  pbl_gmtime_r(&t, &tm);
  // The calendar holds years 2000 to 2099.
  if (tm.tm_year < 100 || tm.tm_year > 199) {
    return -EINVAL;
  }

  const uint32_t tr = (prv_bin2bcd((uint32_t)tm.tm_hour) << 16) |
                      (prv_bin2bcd((uint32_t)tm.tm_min) << 8) | prv_bin2bcd((uint32_t)tm.tm_sec);
  const uint32_t dr = (prv_bin2bcd((uint32_t)(tm.tm_year % 100)) << 16) |
                      ((uint32_t)((tm.tm_wday == 0) ? 7 : tm.tm_wday) << 13) |
                      (prv_bin2bcd((uint32_t)(tm.tm_mon + 1)) << 8) |
                      prv_bin2bcd((uint32_t)tm.tm_mday);

  return stm32_rtc_calendar_set(tr, dr);
}

static int prv_init(const struct pbl_device *dev) {
  // The system timer set the RTC up before the kernel started.
  return 0;
}

static const struct pbl_rtc_ops s_ops = {
  .get_time = prv_get_time,
  .set_time = prv_set_time,
};

PBL_DEVICE_STATE_DEFINE(pbl_rtc_stm32);
const struct pbl_rtc pbl_rtc_stm32 = PBL_RTC_INIT(pbl_rtc_stm32, "rtc", prv_init, &s_ops);
PBL_DEVICE_REGISTER(pbl_rtc_stm32, &pbl_rtc_stm32.dev);
