/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <clar.h>

#define M_RTC_BASE  0x40002800U
#define M_PWR_BASE  0x40007000U
#define M_EXTI_BASE 0x40013c00U
#define M_RCC_BASE  0x40023800U

#define M_TR       0x00U
#define M_DR       0x04U
#define M_CR       0x08U
#define M_ISR      0x0cU
#define M_PRER     0x10U
#define M_WUTR     0x14U
#define M_ALRMAR   0x1cU
#define M_WPR      0x24U
#define M_SSR      0x28U
#define M_ALRMASSR 0x44U

#define M_CR_WUCKSEL 0x7U
#define M_CR_BYPSHAD (1U << 5)
#define M_CR_FMT     (1U << 6)
#define M_CR_ALRAE   (1U << 8)
#define M_CR_WUTE    (1U << 10)
#define M_CR_ALRAIE  (1U << 12)
#define M_CR_WUTIE   (1U << 14)
#define M_CR_DRIVER \
  (M_CR_WUCKSEL | M_CR_BYPSHAD | M_CR_FMT | M_CR_ALRAE | M_CR_WUTE | M_CR_ALRAIE | M_CR_WUTIE)

#define M_ISR_ALRAWF (1U << 0)
#define M_ISR_WUTWF  (1U << 2)
#define M_ISR_INITS  (1U << 4)
#define M_ISR_RSF    (1U << 5)
#define M_ISR_INITF  (1U << 6)
#define M_ISR_INIT   (1U << 7)
#define M_ISR_ALRAF  (1U << 8)
#define M_ISR_WUTF   (1U << 10)

#define M_ISR_RC_W0     ((0x7fU << 8) | M_ISR_RSF)
#define M_ISR_UNPROTECT (0x3fU << 8)

#define M_PWR_CR_DBP (1U << 8)

#define M_EXTI_IMR    0x00U
#define M_EXTI_RTSR   0x08U
#define M_EXTI_FTSR   0x0cU
#define M_EXTI_PR     0x14U
#define M_LINE_ALARM  (1U << 17)
#define M_LINE_WAKEUP (1U << 22)

#define M_RCC_APB1ENR 0x40U
#define M_RCC_BDCR    0x70U
#define M_BDCR_LSEON  (1U << 0)
#define M_BDCR_LSERDY (1U << 1)
#define M_BDCR_LSEBYP (1U << 2)
#define M_BDCR_RTCSEL (3U << 8)
#define M_BDCR_RTCEN  (1U << 15)

#define M_IRQ_WKUP  3
#define M_IRQ_ALARM 41

#define M_PRER_STOCK ((127U << 16) | 255U)
#define M_EPOCH_2000 946684800

#define m_assert_eq64(a, b) cl_assert_((uint64_t)(a) == (uint64_t)(b), #a " == " #b)

struct m_access {
  bool write;
  uintptr_t addr;
  uint32_t val;
};

#define M_LOG_SIZE 4096
static struct m_access s_log[M_LOG_SIZE];
static int s_log_len;

static void m_log(bool write, uintptr_t addr, uint32_t val) {
  if (s_log_len < M_LOG_SIZE) {
    s_log[s_log_len++] = (struct m_access){write, addr, val};
  }
}

static int m_writes(uintptr_t addr) {
  int n = 0;
  for (int i = 0; i < s_log_len; i++) {
    n += (s_log[i].write && s_log[i].addr == addr) ? 1 : 0;
  }
  return n;
}

static struct {
  int64_t secs;
  uint32_t ssr;
  uint32_t sub;
  uint64_t counts;
  uint32_t cr;
  uint32_t isr;
  uint32_t prer;
  uint32_t wutr;
  uint32_t wut;
  uint32_t alrmar;
  uint32_t alrmassr;
  int wp;
  bool init_tr_written;
  bool init_dr_written;
  uint32_t init_tr;
  uint32_t init_dr;
  int lag_reads;
  int64_t stale_secs;
  int stale_reads;
  bool frozen;
  int64_t frozen_secs;
  uint32_t frozen_ssr;
  uint32_t pwr_cr;
  uint32_t bdcr;
  uint32_t apb1enr;
  uint32_t exti[6];
  bool alrawf_seen;
  bool wutwf_seen;
} s_m;

static uint32_t s_counts_per_access;
static bool s_irq_pending[64];
static int s_locks;
static bool s_masked_unlocks_only;

static uint32_t m_bcd(uint32_t v) {
  return ((v / 10U) << 4) | (v % 10U);
}

static uint32_t m_bin(uint32_t v) {
  return ((v >> 4) * 10U) + (v & 0xfU);
}

static uint32_t m_tr_of(int64_t secs) {
  time_t t = (time_t)secs;
  struct tm tm;
  gmtime_r(&t, &tm);
  return (m_bcd((uint32_t)tm.tm_hour) << 16) | (m_bcd((uint32_t)tm.tm_min) << 8) |
         m_bcd((uint32_t)tm.tm_sec);
}

static uint32_t m_dr_of(int64_t secs) {
  time_t t = (time_t)secs;
  struct tm tm;
  gmtime_r(&t, &tm);
  const uint32_t wdu = (tm.tm_wday == 0) ? 7U : (uint32_t)tm.tm_wday;
  return (m_bcd((uint32_t)(tm.tm_year - 100)) << 16) | (wdu << 13) |
         (m_bcd((uint32_t)(tm.tm_mon + 1)) << 8) | m_bcd((uint32_t)tm.tm_mday);
}

static int64_t m_secs_of(uint32_t tr, uint32_t dr) {
  struct tm tm = {
    .tm_sec = (int)m_bin(tr & 0x7fU),
    .tm_min = (int)m_bin((tr >> 8) & 0x7fU),
    .tm_hour = (int)m_bin((tr >> 16) & 0x3fU),
    .tm_mday = (int)m_bin(dr & 0x3fU),
    .tm_mon = (int)m_bin((dr >> 8) & 0x1fU) - 1,
    .tm_year = (int)m_bin((dr >> 16) & 0xffU) + 100,
  };
  return (int64_t)timegm(&tm);
}

static void m_set_calendar(int year, int mon, int mday, int hour, int min, int sec, uint32_t ssr) {
  struct tm tm = {
    .tm_sec = sec,
    .tm_min = min,
    .tm_hour = hour,
    .tm_mday = mday,
    .tm_mon = mon - 1,
    .tm_year = year - 1900,
  };
  s_m.secs = (int64_t)timegm(&tm);
  s_m.ssr = ssr;
  s_m.sub = 0U;
}

[[maybe_unused]] static uint64_t m_count(void) {
  return ((uint64_t)(s_m.secs - M_EPOCH_2000) * 256U) + (255U - s_m.ssr);
}

static uint32_t m_exti(uint32_t offset) {
  return s_m.exti[offset / 4U];
}

static void m_event(uint32_t flag, uint32_t ie, uint32_t line, int irq) {
  const bool was = (s_m.isr & flag) != 0U;

  s_m.isr |= flag;
  if (!was && (s_m.cr & ie) != 0U && (m_exti(M_EXTI_RTSR) & line) != 0U &&
      (m_exti(M_EXTI_IMR) & line) != 0U) {
    s_m.exti[M_EXTI_PR / 4U] |= line;
    s_irq_pending[irq] = true;
  }
}

static void m_alarm_check(void) {
  if ((s_m.cr & M_CR_ALRAE) == 0U) {
    return;
  }
  const uint32_t tr = m_tr_of(s_m.secs);
  const uint32_t maskss = (s_m.alrmassr >> 24) & 0xfU;
  const uint32_t ss_mask = (maskss == 0U) ? 0U : ((1U << maskss) - 1U);
  cl_assert((s_m.alrmar & (1U << 31)) != 0U);
  cl_assert((s_m.alrmar & ((1U << 23) | (1U << 15) | (1U << 7))) == 0U);
  if ((s_m.alrmar & 0x7f7f7fU) == tr && (s_m.alrmassr & ss_mask) == (s_m.ssr & ss_mask)) {
    m_event(M_ISR_ALRAF, M_CR_ALRAIE, M_LINE_ALARM, M_IRQ_ALARM);
  }
}

static void m_wut_clock(void) {
  if ((s_m.cr & M_CR_WUTE) == 0U) {
    return;
  }
  if (--s_m.wut == 0U) {
    s_m.wut = s_m.wutr + 1U;
    m_event(M_ISR_WUTF, M_CR_WUTIE, M_LINE_WAKEUP, M_IRQ_WKUP);
  }
}

static void m_advance(uint32_t counts) {
  const uint32_t prediv_s = s_m.prer & 0x7fffU;

  if (counts > 0U) {
    s_m.frozen = false;
  }
  for (uint32_t i = 0U; i < counts; i++) {
    s_m.counts++;
    const uint32_t wucksel = s_m.cr & M_CR_WUCKSEL;
    if (wucksel == 3U || (wucksel == 0U && (s_m.counts % 8U) == 0U)) {
      m_wut_clock();
    }
    if (++s_m.sub < 64U) {
      continue;
    }
    s_m.sub = 0U;
    if (s_m.ssr == 0U || s_m.ssr > prediv_s) {
      s_m.ssr = prediv_s;
      if (s_m.lag_reads > 0) {
        s_m.stale_secs = s_m.secs;
        s_m.stale_reads = s_m.lag_reads;
      }
      s_m.secs++;
      if (wucksel >= 4U) {
        m_wut_clock();
      }
    } else {
      s_m.ssr--;
    }
    m_alarm_check();
  }
}

[[maybe_unused]] static void m_stop(uint32_t counts) {
  const int64_t secs = s_m.secs;
  const uint32_t ssr = s_m.ssr;

  m_advance(counts);
  s_m.frozen = true;
  s_m.frozen_secs = secs;
  s_m.frozen_ssr = ssr;
}

[[maybe_unused]] static void m_advance_steps(uint32_t steps) {
  m_advance(steps * 64U);
}

static bool m_unlocked(void) {
  return s_m.wp == 2;
}

static uint32_t m_rtc_read(uint32_t offset) {
  m_advance(s_counts_per_access);
  switch (offset) {
    case M_TR:
      if (s_m.frozen) {
        return m_tr_of(s_m.frozen_secs);
      }
      return m_tr_of((s_m.stale_reads > 0) ? s_m.stale_secs : s_m.secs);
    case M_DR:
      if (s_m.frozen) {
        return m_dr_of(s_m.frozen_secs);
      }
      if (s_m.stale_reads > 0) {
        s_m.stale_reads--;
        return m_dr_of(s_m.stale_secs);
      }
      return m_dr_of(s_m.secs);
    case M_SSR:
      return s_m.frozen ? s_m.frozen_ssr : s_m.ssr;
    case M_CR:
      return s_m.cr;
    case M_PRER:
      return s_m.prer;
    case M_WUTR:
      return s_m.wutr;
    case M_ALRMAR:
      return s_m.alrmar;
    case M_ALRMASSR:
      return s_m.alrmassr;
    case M_ISR: {
      s_m.alrawf_seen = s_m.alrawf_seen || (s_m.cr & M_CR_ALRAE) == 0U;
      s_m.wutwf_seen = s_m.wutwf_seen || (s_m.cr & M_CR_WUTE) == 0U;
      uint32_t isr = s_m.isr | M_ISR_INITS;
      isr |= ((s_m.cr & M_CR_ALRAE) == 0U) ? M_ISR_ALRAWF : 0U;
      isr |= ((s_m.cr & M_CR_WUTE) == 0U) ? M_ISR_WUTWF : 0U;
      isr |= ((s_m.isr & M_ISR_INIT) != 0U) ? M_ISR_INITF : 0U;
      if ((s_m.isr & M_ISR_RSF) == 0U) {
        s_m.stale_reads = 0;
        s_m.frozen = false;
      }
      s_m.isr |= M_ISR_RSF;
      return isr;
    }
    default:
      cl_fail("read of an RTC register the driver does not use");
      return 0U;
  }
}

static void m_exit_init(void) {
  s_m.frozen = true;
  s_m.frozen_secs = s_m.secs;
  s_m.frozen_ssr = s_m.ssr;
  if (s_m.init_tr_written || s_m.init_dr_written) {
    cl_assert(s_m.init_tr_written && s_m.init_dr_written);
    s_m.secs = m_secs_of(s_m.init_tr, s_m.init_dr);
    s_m.ssr = s_m.prer & 0x7fffU;
    s_m.sub = 0U;
    s_m.stale_reads = 0;
  }
  s_m.init_tr_written = false;
  s_m.init_dr_written = false;
  s_m.isr &= ~M_ISR_RSF;
}

static void m_rtc_write(uint32_t offset, uint32_t val) {
  m_advance(s_counts_per_access);
  cl_assert_((s_m.pwr_cr & M_PWR_CR_DBP) != 0U, "RTC write before backup domain access");
  if (offset == M_WPR) {
    cl_assert_(val != 0xcaU || !s_masked_unlocks_only || s_locks > 0,
               "write protection turned off with interrupts unmasked");
    s_m.wp = (val == 0xcaU) ? 1 : ((val == 0x53U && s_m.wp == 1) ? 2 : 0);
    return;
  }
  if (offset == M_ISR) {
    const uint32_t writable = m_unlocked() ? (M_ISR_RC_W0 | M_ISR_INIT) : M_ISR_UNPROTECT;
    const bool was_init = (s_m.isr & M_ISR_INIT) != 0U;
    s_m.isr &= ~(M_ISR_RC_W0 & writable & ~val);
    if ((writable & M_ISR_INIT) != 0U) {
      s_m.isr = (s_m.isr & ~M_ISR_INIT) | (val & M_ISR_INIT);
      if (was_init && (val & M_ISR_INIT) == 0U) {
        m_exit_init();
      }
    }
    return;
  }

  cl_assert_(m_unlocked(), "write to a protected RTC register while locked");
  const bool init = (s_m.isr & M_ISR_INIT) != 0U;
  switch (offset) {
    case M_TR:
      cl_assert_(init, "TR written outside initialization mode");
      s_m.init_tr = val;
      s_m.init_tr_written = true;
      break;
    case M_DR:
      cl_assert_(init, "DR written outside initialization mode");
      s_m.init_dr = val;
      s_m.init_dr_written = true;
      break;
    case M_PRER:
      cl_assert_(init, "PRER written outside initialization mode");
      s_m.prer = val;
      break;
    case M_CR:
      cl_assert_(((s_m.cr ^ val) & ~M_CR_DRIVER) == 0U, "RTC_CR bit the driver never writes");
      cl_assert_(((s_m.cr ^ val) & M_CR_FMT) == 0U || init, "FMT changed outside init mode");
      cl_assert_(((s_m.cr ^ val) & M_CR_WUCKSEL) == 0U || (s_m.cr & M_CR_WUTE) == 0U,
                 "WUCKSEL changed while the wakeup timer runs");
      cl_assert_(((s_m.cr ^ val) & M_CR_WUCKSEL) == 0U || s_m.wutwf_seen,
                 "WUCKSEL changed before WUTWF was read");
      if ((val & M_CR_WUTE) != 0U && (s_m.cr & M_CR_WUTE) == 0U) {
        s_m.wut = s_m.wutr + 1U;
      }
      if ((val & M_CR_WUTE) == 0U && (s_m.cr & M_CR_WUTE) != 0U) {
        s_m.wutwf_seen = false;
      }
      if ((val & M_CR_ALRAE) == 0U && (s_m.cr & M_CR_ALRAE) != 0U) {
        s_m.alrawf_seen = false;
      }
      s_m.cr = val;
      break;
    case M_WUTR:
      cl_assert_((s_m.cr & M_CR_WUTE) == 0U, "WUTR written while the wakeup timer runs");
      cl_assert_(s_m.wutwf_seen, "WUTR written before WUTWF was read");
      s_m.wutr = val & 0xffffU;
      break;
    case M_ALRMAR:
      cl_assert_((s_m.cr & M_CR_ALRAE) == 0U, "ALRMAR written while alarm A is on");
      cl_assert_(s_m.alrawf_seen, "ALRMAR written before ALRAWF was read");
      s_m.alrmar = val;
      break;
    case M_ALRMASSR:
      cl_assert_((s_m.cr & M_CR_ALRAE) == 0U, "ALRMASSR written while alarm A is on");
      cl_assert_(s_m.alrawf_seen, "ALRMASSR written before ALRAWF was read");
      s_m.alrmassr = val;
      break;
    default:
      cl_fail("write to an RTC register the driver must not write");
  }
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  uint32_t val;

  if (addr >= M_RTC_BASE && addr < M_RTC_BASE + 0x400U) {
    val = m_rtc_read((uint32_t)(addr - M_RTC_BASE));
  } else if (addr == M_PWR_BASE) {
    val = s_m.pwr_cr;
  } else if (addr == M_RCC_BASE + M_RCC_BDCR) {
    if ((s_m.bdcr & M_BDCR_LSEON) != 0U) {
      s_m.bdcr |= M_BDCR_LSERDY;
    }
    val = s_m.bdcr;
  } else if (addr >= M_EXTI_BASE && addr < M_EXTI_BASE + 0x18U) {
    val = s_m.exti[(addr - M_EXTI_BASE) / 4U];
  } else {
    cl_fail("read of an unexpected register");
    val = 0U;
  }
  m_log(false, addr, val);
  return val;
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  m_log(true, addr, val);
  if (addr >= M_RTC_BASE && addr < M_RTC_BASE + 0x400U) {
    m_rtc_write((uint32_t)(addr - M_RTC_BASE), val);
  } else if (addr == M_PWR_BASE) {
    s_m.pwr_cr = val;
  } else if (addr == M_RCC_BASE + M_RCC_BDCR) {
    cl_assert_((s_m.pwr_cr & M_PWR_CR_DBP) != 0U, "BDCR write before backup domain access");
    cl_assert_((val & (1U << 16)) == 0U, "backup domain reset");
    cl_assert_((s_m.bdcr & M_BDCR_RTCEN) == 0U || ((s_m.bdcr ^ val) & M_BDCR_RTCSEL) == 0U,
               "RTCSEL changed while the RTC runs");
    s_m.bdcr = (val & ~M_BDCR_LSERDY) | (s_m.bdcr & M_BDCR_LSERDY);
  } else if (addr == M_EXTI_BASE + M_EXTI_PR) {
    s_m.exti[M_EXTI_PR / 4U] &= ~val;
  } else if (addr >= M_EXTI_BASE && addr < M_EXTI_BASE + 0x18U) {
    s_m.exti[(addr - M_EXTI_BASE) / 4U] = val;
  } else {
    cl_fail("write to an unexpected register");
  }
}

static void m_reset_stock(void) {
  memset(&s_m, 0, sizeof(s_m));
  memset(s_irq_pending, 0, sizeof(s_irq_pending));
  s_m.prer = M_PRER_STOCK;
  s_m.bdcr = M_BDCR_LSEON | M_BDCR_LSERDY | M_BDCR_LSEBYP | (1U << 8) | M_BDCR_RTCEN;
  s_m.isr = M_ISR_RSF;
  s_m.wutr = 0xffffU;
  m_set_calendar(2026, 11, 20, 12, 34, 56, 200U);
  s_counts_per_access = 0U;
  s_masked_unlocks_only = false;
  s_log_len = 0;
}
