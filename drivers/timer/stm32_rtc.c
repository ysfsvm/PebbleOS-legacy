/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! System timer on the STM32F4 RTC, the only clock left running in STOP mode.
//!
//! The RTC counts the 32.768 kHz LSE with a 256 Hz sub-second counter and a calendar. The clock
//! source is the calendar plus the sub-second counter, as 256 Hz cycles since 2000, plus an
//! offset that keeps it continuous when the calendar is set (see stm32_rtc_calendar_set()). The
//! clock event is alarm A with its sub-second match, in steps of the clock source, or the wakeup
//! timer (TIMER_STM32_RTC_WUT), which counts the LSE itself.
//!
//! The RTC configuration lives in the backup domain and outlives this firmware: the stock
//! bootloader, recovery firmware and firmware read the same calendar and assume the stock set-up,
//! a 127/255 prescaler pair (256 sub-seconds) and the shadow registers. The recovery firmware
//! computes milliseconds from the sub-second counter and asserts when time appears to go
//! backwards across STOP mode, so any other set-up makes it crash-loop. Keep the stock set-up and
//! restore it when another configuration is found. The calendar keeps UTC with a two-digit year.

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <pbl/debug/perf_probe.h>
#include <pbl/drivers/timer.h>
#include <pbl/drivers/timer/stm32_rtc.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/cmsis.h>
#include <pbl/util/math.h>

#define STM32_RTC ((RTC_TypeDef *)RTC_BASE)

#define LSE_HZ 32768U
//! Asynchronous and synchronous prescalers, as the stock firmware: LSE / 128 / 256 = 1 Hz,
//! sub-seconds at 256 Hz.
#define PREDIV_A  127U
#define PREDIV_S  255U
#define SUBSEC_HZ (PREDIV_S + 1U)
static_assert(SUBSEC_HZ == STM32_RTC_SUBSEC_HZ, "sub-second rate out of sync with the header");

//! The wakeup timer is a 16-bit down-counter with a choice of clocks. The clock event counts
//! in the finest one, LSE / 2; longer delays use a coarser clock, rounded down so that an event
//! comes early rather than late (the kernel programs the rest).
#define WUT_HZ        (LSE_HZ / 2U)
#define WUT_MAX       65536U
#define WUCKSEL_DIV2  (RTC_CR_WUCKSEL_1 | RTC_CR_WUCKSEL_0)
#define WUCKSEL_DIV16 0U
#define WUCKSEL_1HZ   RTC_CR_WUCKSEL_2
//! LSE / 16 in LSE / 2 counts.
#define WUT_DIV16_RATIO 8U

#define LSE_READY_TIMEOUT_MS 5000U
//! Initialization mode and wakeup timer write access come within two RTCCLK cycles.
#define RTC_SYNC_TIMEOUT_US 1000U

//! EXTI lines of the RTC alarms and of the wakeup timer.
#define EXTI_LINE_RTC_ALARM  (1U << 17)
#define EXTI_LINE_RTC_WAKEUP (1U << 22)

#define SECONDS_PER_DAY 86400U

// One kernel tick: the clock source only steps every four, so shorter events would just
// interrupt more often while a timeout waits for the next step.
#define MIN_DELTA CONFIG_TIMER_STM32_RTC_WUT_MIN_COUNT
#define MAX_DELTA (3600U * WUT_HZ)

#define CALENDAR_READ_RETRIES 3U

//! Clock source value minus the calendar count; changes only when the calendar is set.
static int64_t s_offset;
//! Last clock source value handed out and the registers it came from.
static uint64_t s_last_now;
static uint32_t s_last_ssr;
static uint32_t s_last_tr;
//! Registers of the last calendar count.
static uint32_t s_read_ssr;
static uint32_t s_read_tr;
//! Calendar readings that mixed two instants, and clock source readings that went back.
static uint32_t s_mixed;
static uint32_t s_mixed_regs[4];
static uint32_t s_backward;
static uint32_t s_backward_steps;
static uint32_t s_backward_regs[4];
//! The RTC was found in another configuration at init and put back to the stock one.
static bool s_restored_stock;

bool stm32_rtc_restored_stock_config(void) {
  return s_restored_stock;
}
[[maybe_unused]] static bool s_armed;
[[maybe_unused]] static uint32_t s_stale_irqs;
[[maybe_unused]] static uint32_t s_rearmed;
[[maybe_unused]] static uint32_t s_repended;

static void prv_wp_unlock(void) {
  STM32_RTC->WPR = 0xCA;
  STM32_RTC->WPR = 0x53;
}

static void prv_wp_lock(void) {
  STM32_RTC->WPR = 0xFF;
}

static uint32_t prv_bcd2bin(uint32_t bcd) {
  return ((bcd >> 4) * 10U) + (bcd & 0xFU);
}

//! Days from 2000-01-01 to the date, for years 2000-2099.
static uint32_t prv_days_since_2000(uint32_t year, uint32_t month, uint32_t day) {
  // Days from civil, on years starting in March.
  const uint32_t y = (2000U + year) - ((month <= 2U) ? 1U : 0U);
  const uint32_t era = y / 400U;
  const uint32_t yoe = y - (era * 400U);
  const uint32_t doy = ((153U * ((month > 2U) ? (month - 3U) : (month + 9U))) + 2U) / 5U + day - 1U;
  const uint32_t doe = (yoe * 365U) + (yoe / 4U) - (yoe / 100U) + doy;
  // 730425 is 2000-01-01 counted the same way.
  return (era * 146097U) + doe - 730425U;
}

static void prv_log_mixed(void *data) {
  PBL_LOG_WRN("Calendar read mixed two instants (%" PRIu32 " times): ssr %" PRIx32 " tr %" PRIx32
              " then ssr %" PRIx32 " tr %" PRIx32,
              s_mixed, s_mixed_regs[0], s_mixed_regs[1], s_mixed_regs[2], s_mixed_regs[3]);
}

//! Read the calendar's shadow registers. Reading SSR (or TR) should freeze the set until DR is
//! read, so that the three values belong to one instant; a reading that mixes two is off by
//! up to a second. Read until two in a row are of the same second. Interrupts masked.
static void prv_calendar_read(uint32_t *ssr, uint32_t *tr, uint32_t *dr) {
  uint32_t prev_ssr = STM32_RTC->SSR;
  uint32_t prev_tr = STM32_RTC->TR;
  uint32_t prev_dr = STM32_RTC->DR;

  for (unsigned int i = 0U; i < CALENDAR_READ_RETRIES; i++) {
    *ssr = STM32_RTC->SSR;
    *tr = STM32_RTC->TR;
    *dr = STM32_RTC->DR;

    // The sub-second counter counts down within a second and starts again with the next.
    const bool same_second = (*tr == prev_tr) && (*dr == prev_dr);
    const bool counted_down = *ssr <= prev_ssr;
    if (same_second && counted_down) {
      return;
    }
    if (same_second || counted_down) {
      // Not a second gone by between the two: one of them is mixed. Report the first and
      // then every 64th.
      s_mixed++;
      if ((s_mixed % 64U) == 1U) {
        s_mixed_regs[0] = prev_ssr;
        s_mixed_regs[1] = prev_tr;
        s_mixed_regs[2] = *ssr;
        s_mixed_regs[3] = *tr;
        (void)pbl_sys_work_call_from_isr(prv_log_mixed, nullptr);
      }
    }
    prev_ssr = *ssr;
    prev_tr = *tr;
    prev_dr = *dr;
  }
}

//! Calendar count in 256 Hz cycles since 2000. Interrupts masked.
static uint64_t prv_calendar_count(void) {
  uint32_t ssr;
  uint32_t tr;
  uint32_t dr;

  prv_calendar_read(&ssr, &tr, &dr);
  if (ssr > PREDIV_S) {
    // A shift operation can take it past the prescaler; clamp.
    ssr = PREDIV_S;
  }

  const uint32_t sec = prv_bcd2bin(tr & (RTC_TR_ST | RTC_TR_SU));
  const uint32_t min = prv_bcd2bin((tr & (RTC_TR_MNT | RTC_TR_MNU)) >> 8);
  const uint32_t hour = prv_bcd2bin((tr & (RTC_TR_HT | RTC_TR_HU)) >> 16);
  const uint32_t day = prv_bcd2bin(dr & (RTC_DR_DT | RTC_DR_DU));
  const uint32_t month = prv_bcd2bin((dr & (RTC_DR_MT | RTC_DR_MU)) >> 8);
  const uint32_t year = prv_bcd2bin((dr & (RTC_DR_YT | RTC_DR_YU)) >> 16);

  const uint64_t seconds = ((uint64_t)prv_days_since_2000(year, month, day) * SECONDS_PER_DAY) +
                           (hour * 3600U) + (min * 60U) + sec;
  s_read_ssr = ssr;
  s_read_tr = tr;
  return (seconds * SUBSEC_HZ) + (PREDIV_S - ssr);
}

static void prv_log_backward(void *data) {
  PBL_LOG_WRN("Clock source read %" PRIu32 " steps back (%" PRIu32 " times): ssr %" PRIx32
              " tr %" PRIx32 " after ssr %" PRIx32 " tr %" PRIx32,
              s_backward_steps, s_backward, s_backward_regs[0], s_backward_regs[1],
              s_backward_regs[2], s_backward_regs[3]);
}

//! The clock source never goes back: the kernel takes a smaller value for a counter that
//! wrapped, and its time jumps ahead by weeks. Interrupts masked.
static uint64_t prv_now(void) {
  const uint64_t now = (uint64_t)((int64_t)prv_calendar_count() + s_offset);

  if (now < s_last_now) {
    s_backward++;
    // Report the first and then every 64th.
    if ((s_backward % 64U) == 1U) {
      s_backward_steps = (uint32_t)(s_last_now - now);
      s_backward_regs[0] = s_read_ssr;
      s_backward_regs[1] = s_read_tr;
      s_backward_regs[2] = s_last_ssr;
      s_backward_regs[3] = s_last_tr;
      (void)pbl_sys_work_call_from_isr(prv_log_backward, nullptr);
    }
    return s_last_now;
  }

  s_last_now = now;
  s_last_ssr = s_read_ssr;
  s_last_tr = s_read_tr;
  return now;
}

static int prv_wait(uint32_t isr_flag) {
#ifdef CONFIG_SOC_STM32F4_EMULATED
  // The emulator does not model the RTC's synchronization flags.
  return 0;
#endif
  for (uint32_t us = 0U; us < RTC_SYNC_TIMEOUT_US; us++) {
    if ((STM32_RTC->ISR & isr_flag) != 0U) {
      return 0;
    }
    pbl_busy_wait_us(1);
  }
  return -ETIMEDOUT;
}

static uint32_t prv_read(const struct pbl_clocksource *cs) {
  return (uint32_t)prv_now();
}

static void prv_alarm_disable(void) {
  STM32_RTC->CR &= ~(RTC_CR_ALRAIE | RTC_CR_ALRAE);
  STM32_RTC->ISR = ~(RTC_ISR_ALRAF | RTC_ISR_INIT) & 0x0001FFFFU;
  EXTI->PR = EXTI_LINE_RTC_ALARM;
}

#ifdef CONFIG_TIMER_STM32_RTC_WUT
//! Stop the wakeup timer and drop a pending event. Write protection off.
static void prv_wakeup_disable(void) {
  STM32_RTC->CR &= ~(RTC_CR_WUTIE | RTC_CR_WUTE);
  STM32_RTC->ISR = ~(RTC_ISR_WUTF | RTC_ISR_INIT) & 0x0001FFFFU;
  EXTI->PR = EXTI_LINE_RTC_WAKEUP;
}

static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  uint32_t clock;
  uint32_t count;

  if (delta <= WUT_MAX) {
    clock = WUCKSEL_DIV2;
    count = delta;
  } else if (delta <= WUT_MAX * WUT_DIV16_RATIO) {
    clock = WUCKSEL_DIV16;
    count = delta / WUT_DIV16_RATIO;
  } else {
    clock = WUCKSEL_1HZ;
    count = MIN(delta / WUT_HZ, WUT_MAX);
  }

  // The timer reloads itself. The kernel asks for an event every tick while a thread runs:
  // a timer that already runs with the period asked for is left running, so a tick costs
  // nothing here. Stopping it to write the counter takes about two RTCCLK cycles (60 us)
  // with interrupts locked, 1024 times a second. Its next event comes no later than asked.
#ifdef CONFIG_TIMER_STM32_RTC_WUT_PERIODIC
  if ((STM32_RTC->CR & (RTC_CR_WUTE | RTC_CR_WUTIE | RTC_CR_WUCKSEL)) ==
          (RTC_CR_WUTE | RTC_CR_WUTIE | clock) &&
      STM32_RTC->WUTR == count - 1U) {
    s_armed = true;
    return;
  }
#endif

  perf_probe_count(PERF_CE_REPROG);
  prv_wp_unlock();
  prv_wakeup_disable();
  // An event of the timer just stopped may already be pending as an interrupt: it must not
  // be taken for one of the timer started below.
  pbl_irq_clear_pending(PBL_IRQN(RTC_WKUP));
  // The counter and its clock can only be written while the timer is off.
  (void)prv_wait(RTC_ISR_WUTWF);
  STM32_RTC->CR = (STM32_RTC->CR & ~RTC_CR_WUCKSEL) | clock;
  // The event comes count clock cycles after the timer is enabled.
  STM32_RTC->WUTR = count - 1U;
  s_armed = true;
  STM32_RTC->CR |= RTC_CR_WUTIE | RTC_CR_WUTE;
  prv_wp_lock();
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  s_armed = false;
  prv_wp_unlock();
  prv_wakeup_disable();
  prv_wp_lock();
}
#endif

#ifdef CONFIG_TIMER_STM32_RTC_ALARM
// The other clock event: alarm A with a sub-second match, in steps of the clock source.

//! Sub-second bits the alarm compares: all of them.
#define ALARM_SS_BITS 8U
//! Alarm written, then the calendar must still reach it: three steps (11.7 ms).
#define ALARM_MIN_DELTA 3U
#define ALARM_MAX_DELTA (3600U * SUBSEC_HZ)

static uint64_t s_deadline;

static uint32_t prv_bin2bcd(uint32_t bin) {
  return ((bin / 10U) << 4) | (bin % 10U);
}

static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  const uint64_t target = prv_calendar_count() + delta;
  const uint64_t seconds = target / SUBSEC_HZ;
  const uint32_t sod = (uint32_t)(seconds % SECONDS_PER_DAY);
  const uint32_t ss = PREDIV_S - (uint32_t)(target % SUBSEC_HZ);

  perf_probe_count(PERF_CE_REPROG);
  prv_wp_unlock();
  prv_alarm_disable();
  (void)prv_wait(RTC_ISR_ALRAWF);
  // Any date, this time of day and all the sub-second bits.
  STM32_RTC->ALRMAR = RTC_ALRMAR_MSK4 | (prv_bin2bcd(sod / 3600U) << 16) |
                      (prv_bin2bcd((sod / 60U) % 60U) << 8) | prv_bin2bcd(sod % 60U);
  STM32_RTC->ALRMASSR = (ALARM_SS_BITS << 24) | ss;
  STM32_RTC->CR |= RTC_CR_ALRAIE | RTC_CR_ALRAE;
  prv_wp_lock();

  s_deadline = target;
  s_armed = true;

  // The match may have gone by while the alarm was written.
  if (prv_calendar_count() >= target) {
    perf_probe_count(PERF_CE_REPEND);
    pbl_irq_set_pending(PBL_IRQN(RTC_Alarm));
  }
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  s_armed = false;
  prv_wp_unlock();
  prv_alarm_disable();
  prv_wp_lock();
}
#endif

static int prv_init(const struct pbl_device *dev);

static const struct pbl_clocksource_ops s_cs_ops = {
  .read = prv_read,
};

#if defined(CONFIG_TIMER_STM32_RTC_WUT) || defined(CONFIG_TIMER_STM32_RTC_ALARM)
static const struct pbl_clockevent_ops s_ce_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};
#endif

#ifdef CONFIG_TIMER_STM32_RTC_ALARM
PBL_DEVICE_STATE_DEFINE(s_rtc_ce);
static const struct pbl_clockevent s_rtc_ce =
    PBL_CLOCKEVENT_INIT(s_rtc_ce, "rtc", prv_init, &s_ce_ops, ALARM_MIN_DELTA, ALARM_MAX_DELTA);

const struct pbl_clockevent *const pbl_system_clockevent = &s_rtc_ce;

static void prv_alarm_isr(void) {
  const bool fired = (STM32_RTC->ISR & RTC_ISR_ALRAF) != 0U;

  perf_probe_count(PERF_CE_IRQ);
  STM32_RTC->ISR = ~(RTC_ISR_ALRAF | RTC_ISR_INIT) & 0x0001FFFFU;
  EXTI->PR = EXTI_LINE_RTC_ALARM;

  // Also runs when set_next pended the interrupt itself.
  if (s_armed && (fired || (prv_calendar_count() >= s_deadline))) {
    s_armed = false;
    pbl_clockevent_handler(&s_rtc_ce);
  }
}

// Lowest priority, as the kernel's own interrupts.
PBL_IRQ_CONNECT(RTC_Alarm, (1U << CONFIG_NUM_IRQ_PRIO_BITS) - 1U, prv_alarm_isr, , 0);
#endif

PBL_DEVICE_STATE_DEFINE(s_rtc_cs);
static const struct pbl_clocksource s_rtc_cs =
    PBL_CLOCKSOURCE_INIT(s_rtc_cs, "rtc", prv_init, &s_cs_ops, 32);

const struct pbl_clocksource *const pbl_system_clocksource = &s_rtc_cs;

#ifdef CONFIG_TIMER_STM32_RTC_WUT
PBL_DEVICE_STATE_DEFINE(s_rtc_ce);
static const struct pbl_clockevent s_rtc_ce =
    PBL_CLOCKEVENT_INIT(s_rtc_ce, "rtc", prv_init, &s_ce_ops, MIN_DELTA, MAX_DELTA);

const struct pbl_clockevent *const pbl_system_clockevent = &s_rtc_ce;

static void prv_isr(void) {
  // Acknowledge the line first: a timer that expires after the check below raises it again.
  EXTI->PR = EXTI_LINE_RTC_WAKEUP;

  // Not expired: the interrupt is left over from a timer that has been stopped or started
  // again since. The running timer must be left alone, or its event never comes.
  perf_probe_count(PERF_CE_IRQ);
  if ((STM32_RTC->ISR & RTC_ISR_WUTF) == 0U) {
    s_stale_irqs++;
    perf_probe_count(PERF_CE_STALE);
    return;
  }

#ifdef CONFIG_TIMER_STM32_RTC_WUT_PERIODIC
  // The timer keeps running: the handler either wants the same period again, or sets another
  // one, or nothing, in which case the timer is stopped below.
  STM32_RTC->ISR = ~(RTC_ISR_WUTF | RTC_ISR_INIT) & 0x0001FFFFU;
#else
  // One shot: the timer would reload and run again.
  prv_wp_unlock();
  prv_wakeup_disable();
  prv_wp_lock();
#endif

  if (s_armed) {
    s_armed = false;
    pbl_clockevent_handler(&s_rtc_ce);
  }

  if (!s_armed) {
    prv_wp_unlock();
    prv_wakeup_disable();
    prv_wp_lock();
  }
}

static void prv_log_rearmed(void *data) {
  PBL_LOG_WRN("Clock event timer was off while armed; restarted it (%" PRIu32 " times, %" PRIu32
              " stale interrupts)",
              s_rearmed, s_stale_irqs);
}

static void prv_log_repended(void *data) {
  PBL_LOG_WRN("Clock event expired without an interrupt; raised it (%" PRIu32 " times)",
              s_repended);
}

static void prv_restart(void) {
  // Armed but not running: no event would ever come and the system would stall until the
  // watchdog resets it. Start the shortest timer instead and say so.
  s_rearmed++;
  prv_set_next(&s_rtc_ce, MIN_DELTA);
  (void)pbl_sys_work_call_from_isr(prv_log_rearmed, nullptr);
}

void stm32_rtc_event_check(void) {
  if (!s_armed) {
    return;
  }

  const uint32_t on = RTC_CR_WUTIE | RTC_CR_WUTE;
  if ((STM32_RTC->CR & on) != on) {
    prv_restart();
    return;
  }

  // The line is edge triggered and the flag has to be clear 1.5 RTCCLK cycles before the timer
  // expires again. A handler that ran late (interrupts locked, a long handler of the same
  // priority) can clear it too late: the flag then stays set, no edge follows and no clock
  // event would ever come. Interrupts are masked here, so a set flag is an event not yet
  // delivered either way: make sure its interrupt is pending.
  if ((STM32_RTC->ISR & RTC_ISR_WUTF) != 0U) {
    pbl_irq_set_pending(PBL_IRQN(RTC_WKUP));
    s_repended++;
    perf_probe_count(PERF_CE_REPEND);
    // Normal just before an event; report only every 1024th.
    if ((s_repended % 1024U) == 1U) {
      (void)pbl_sys_work_call_from_isr(prv_log_repended, nullptr);
    }
  }
}

// Lowest priority, as the kernel's own interrupts.
PBL_IRQ_CONNECT(RTC_WKUP, (1U << CONFIG_NUM_IRQ_PRIO_BITS) - 1U, prv_isr, , 0);
#endif

static void prv_lse_start(void) {
  // The LSE keeps running from the backup domain across resets.
  if ((RCC->BDCR & RCC_BDCR_LSERDY) != 0U) {
    return;
  }

  // The board feeds the LSE from an external 32.768 kHz clock.
  RCC->BDCR |= RCC_BDCR_LSEBYP;
  RCC->BDCR |= RCC_BDCR_LSEON;
  for (uint32_t ms = 0U; ms < LSE_READY_TIMEOUT_MS; ms++) {
    if ((RCC->BDCR & RCC_BDCR_LSERDY) != 0U) {
      return;
    }
    pbl_busy_wait_us(1000);
  }
}

static int prv_enter_init(void) {
  STM32_RTC->ISR |= RTC_ISR_INIT;
  return prv_wait(RTC_ISR_INITF);
}

static void prv_exit_init(void) {
  STM32_RTC->ISR &= ~RTC_ISR_INIT;
}

void stm32_rtc_resync(void) {
  // The shadow registers hold the last synchronized calendar: after STOP mode or an
  // initialization they are stale until the next synchronization, which RSF reports.
  STM32_RTC->ISR &= ~RTC_ISR_RSF;
  (void)prv_wait(RTC_ISR_RSF);
}

int stm32_rtc_calendar_set(uint32_t tr, uint32_t dr) {
  int ret;

  pbl_irq_lock();
  // Keep the clock source continuous across the change.
  const uint64_t before = prv_now();

  prv_wp_unlock();
  ret = prv_enter_init();
  if (ret == 0) {
    STM32_RTC->TR = tr;
    STM32_RTC->DR = dr;
    prv_exit_init();
  }
  prv_wp_lock();
  stm32_rtc_resync();

  s_offset = (int64_t)before - (int64_t)prv_calendar_count();
  pbl_irq_unlock();

  return ret;
}

void stm32_rtc_calendar_get_subsec(uint32_t *tr, uint32_t *dr, uint32_t *subsec) {
  uint32_t ssr;

  pbl_irq_lock();
  prv_calendar_read(&ssr, tr, dr);
  pbl_irq_unlock();
  *subsec = PREDIV_S - MIN(ssr, PREDIV_S);
}

void stm32_rtc_calendar_get(uint32_t *tr, uint32_t *dr) {
  uint32_t ssr;

  pbl_irq_lock();
  prv_calendar_read(&ssr, tr, dr);
  pbl_irq_unlock();
}

// Both devices share the RTC.
static int prv_init(const struct pbl_device *dev) {
  static bool s_started;

  if (!s_started) {
    // Backup domain write access.
    soc_stm32_clock_set(SOC_STM32_APB1, RCC_APB1ENR_PWREN, true);
    PWR->CR |= PWR_CR_DBP;

    prv_lse_start();
    if ((RCC->BDCR & RCC_BDCR_RTCEN) == 0U) {
      RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) | RCC_BDCR_RTCSEL_0;
      RCC->BDCR |= RCC_BDCR_RTCEN;
    }

    prv_wp_unlock();
    // The stock set-up: 127/255, 24-hour format, shadow registers. Earlier builds of this port
    // ran 31/1023 with the shadow registers bypassed; put that back, keeping the calendar.
    if ((STM32_RTC->PRER != ((PREDIV_A << 16) | PREDIV_S)) ||
        ((STM32_RTC->CR & (RTC_CR_BYPSHAD | RTC_CR_FMT)) != 0U)) {
      if (prv_enter_init() == 0) {
        STM32_RTC->PRER = PREDIV_S;
        STM32_RTC->PRER = (PREDIV_A << 16) | PREDIV_S;
        STM32_RTC->CR &= ~(RTC_CR_FMT | RTC_CR_BYPSHAD);
        prv_exit_init();
        s_restored_stock = true;
      }
    }
    // Both event sources keep their state in the backup domain, and a build with the other
    // one may have run before: start with both off.
    prv_alarm_disable();
    STM32_RTC->CR &= ~(RTC_CR_WUTIE | RTC_CR_WUTE);
    STM32_RTC->ISR = ~(RTC_ISR_WUTF | RTC_ISR_INIT) & 0x0001FFFFU;
    EXTI->PR = EXTI_LINE_RTC_WAKEUP;
    prv_wp_lock();
    stm32_rtc_resync();

#ifdef CONFIG_TIMER_STM32_RTC_WUT
    EXTI->RTSR |= EXTI_LINE_RTC_WAKEUP;
    EXTI->FTSR &= ~EXTI_LINE_RTC_WAKEUP;
    EXTI->IMR |= EXTI_LINE_RTC_WAKEUP;
    pbl_irq_enable(PBL_IRQN(RTC_WKUP));
#endif
#ifdef CONFIG_TIMER_STM32_RTC_ALARM
    EXTI->RTSR |= EXTI_LINE_RTC_ALARM;
    EXTI->FTSR &= ~EXTI_LINE_RTC_ALARM;
    EXTI->IMR |= EXTI_LINE_RTC_ALARM;
    pbl_irq_enable(PBL_IRQN(RTC_Alarm));
#endif

    s_started = true;
  }

  if (dev == &s_rtc_cs.dev) {
    pbl_clocksource_set_hz(&s_rtc_cs, SUBSEC_HZ);
  }
#ifdef CONFIG_TIMER_STM32_RTC_WUT
  else {
    pbl_clockevent_set_hz(&s_rtc_ce, WUT_HZ);
  }
#endif
#ifdef CONFIG_TIMER_STM32_RTC_ALARM
  else {
    pbl_clockevent_set_hz(&s_rtc_ce, SUBSEC_HZ);
  }
#endif

  return 0;
}
