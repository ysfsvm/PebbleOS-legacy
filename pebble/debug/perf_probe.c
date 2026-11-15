/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/debug/perf_probe.h>

#ifdef CONFIG_LEGACY_PERF_PROBE

#include <inttypes.h>
#include <stdbool.h>

#include <pbl/input/input.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <applib/ui/app_window_stack.h>
#include <applib/ui/window.h>
#include <kernel/ui/modals/modal_manager.h>
#include <process_management/app_manager.h>
#include <process_management/pebble_process_md.h>

#define REPORT_PERIOD_MS 5000U
//! A scripted key's frames end at the first gap this long between two of them.
#define KEY_GAP_MS 200U

uint32_t perf_probe_counters[PERF_NUM_COUNTERS];

// Since the last report. Written from interrupts, read and cleared with interrupts locked.
static struct {
  uint32_t frames;
  uint32_t frame_ms_sum;
  uint32_t frame_ms_max;
  //! Longest time between the ends of two frames.
  uint32_t gap_ms_max;
  uint32_t keys;
  //! From a key press to the end of the next frame.
  uint32_t key_frame_ms_sum;
  uint32_t key_frame_ms_max;
  uint32_t key_frames;
  //! Time in each part of a frame, and from the end of the wait to the end of the frame.
  uint32_t part_us[PERF_NUM_PARTS];
  uint32_t flush_us;
} s_acc;

static uint32_t s_frame_begin_ms;
static uint32_t s_frame_last_end_ms;
static uint32_t s_key_ms;
static bool s_key_waiting;

static uint32_t s_part_begin_us[PERF_NUM_PARTS];
static bool s_part_open[PERF_NUM_PARTS];
static uint32_t s_flush_begin_us;
static bool s_flush_open;

//! The scripted key last pressed, and the frames after it.
static struct {
  bool active;
  //! A gap of KEY_GAP_MS ended its frames.
  bool closed;
  unsigned int step;
  unsigned int repeat;
  uint32_t press_ms;
  uint32_t first_ms;
  uint32_t last_ms;
  uint32_t frames;
} s_key;

static uint32_t s_last_counters[PERF_NUM_COUNTERS];
static uint64_t s_last_idle_us[2];
static uint32_t s_last_cycles;
static uint32_t s_last_report_ms;
static bool s_reporting;
static uint32_t s_seq;

static void prv_report(struct pbl_work *work);
static PBL_DELAYED_WORK_DEFINE(s_report_work, prv_report);

static uint32_t prv_now_ms(void) {
  return (uint32_t)pbl_uptime_ms();
}

// Microsecond clock for the parts of a frame. The cycle counter stops while the CPU waits in
// WFI, as it does during a frame's transfer: TIM5, free running at 1 MHz, runs on in sleep
// mode. STOP mode, which halts it, is blocked during a transfer and never entered for the
// 1 ms sleeps of the wait. The emulator has the uptime only.
#ifdef CONFIG_SOC_STM32F4_EMULATED
static uint32_t prv_now_us(void) {
  return (uint32_t)((pbl_uptime_ticks() * 1000000U) / PBL_TICK_HZ);
}
#else
#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/cmsis.h>

static bool s_us_running;

static uint32_t prv_now_us(void) {
  if (!s_us_running) {
    RCC_ClocksTypeDef clocks;

    pbl_irq_lock();
    if (!s_us_running) {
      soc_stm32_clock_set(SOC_STM32_APB1, RCC_APB1ENR_TIM5EN, true);
      RCC_GetClocksFreq(&clocks);
      // APB1 timers run at twice PCLK1 when APB1 is divided.
      const uint32_t timer_hz =
          clocks.PCLK1_Frequency * ((clocks.HCLK_Frequency == clocks.PCLK1_Frequency) ? 1U : 2U);
      TIM5->CR1 = 0U;
      TIM5->PSC = (timer_hz / 1000000U) - 1U;
      TIM5->ARR = UINT32_MAX;
      TIM5->EGR = TIM_EGR_UG;
      TIM5->CR1 = TIM_CR1_CEN;
      s_us_running = true;
    }
    pbl_irq_unlock();
  }
  return TIM5->CNT;
}
#endif

void perf_probe_part_begin(enum perf_probe_part part) {
  if (pbl_is_user_context()) {
    return;
  }
  const uint32_t now = prv_now_us();

  pbl_irq_lock();
  s_part_begin_us[part] = now;
  s_part_open[part] = true;
  pbl_irq_unlock();
}

void perf_probe_part_end(enum perf_probe_part part) {
  if (pbl_is_user_context()) {
    return;
  }
  const uint32_t now = prv_now_us();

  pbl_irq_lock();
  if (s_part_open[part]) {
    s_part_open[part] = false;
    s_acc.part_us[part] += now - s_part_begin_us[part];
  }
  if (part == PERF_PART_WAIT) {
    // The transfer starts.
    s_flush_begin_us = now;
    s_flush_open = true;
  }
  pbl_irq_unlock();
}

//! With interrupts locked: a frame ended at now_ms.
static void prv_key_frame(uint32_t now_ms) {
  if (!s_key.active || s_key.closed) {
    return;
  }
  const uint32_t ms = now_ms - s_key.press_ms;
  if (s_key.frames == 0U) {
    s_key.first_ms = ms;
  } else if (ms - s_key.last_ms >= KEY_GAP_MS) {
    s_key.closed = true;
    return;
  }
  s_key.last_ms = ms;
  s_key.frames++;
}

static void prv_start_reporting(void) {
  if (!s_reporting) {
    s_reporting = true;
    (void)pbl_delayed_work_reschedule(&pbl_sys_workq, &s_report_work, PBL_MSEC(REPORT_PERIOD_MS));
  }
}

void perf_probe_frame_begin(void) {
  s_frame_begin_ms = prv_now_ms();
}

void perf_probe_frame_end(void) {
  const uint32_t now = prv_now_ms();
  const uint32_t frame_ms = now - s_frame_begin_ms;
  const uint32_t now_us = prv_now_us();

  pbl_irq_lock();
  if (s_flush_open) {
    s_flush_open = false;
    s_acc.flush_us += now_us - s_flush_begin_us;
  }
  prv_key_frame(now);
  s_acc.frames++;
  s_acc.frame_ms_sum += frame_ms;
  s_acc.frame_ms_max = MAX(s_acc.frame_ms_max, frame_ms);
  if (s_acc.frames > 1U) {
    s_acc.gap_ms_max = MAX(s_acc.gap_ms_max, now - s_frame_last_end_ms);
  }
  s_frame_last_end_ms = now;
  if (s_key_waiting) {
    const uint32_t key_ms = now - s_key_ms;
    s_key_waiting = false;
    s_acc.key_frames++;
    s_acc.key_frame_ms_sum += key_ms;
    s_acc.key_frame_ms_max = MAX(s_acc.key_frame_ms_max, key_ms);
  }
  pbl_irq_unlock();

  prv_start_reporting();
}

static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  if (evt->type != PBL_INPUT_EV_KEY || evt->value == 0) {
    return;
  }

  pbl_irq_lock();
  s_acc.keys++;
  // A key that comes before the frame of the previous one keeps the earlier time.
  if (!s_key_waiting) {
    s_key_waiting = true;
    s_key_ms = prv_now_ms();
  }
  pbl_irq_unlock();
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, nullptr);

static void prv_report(struct pbl_work *work) {
  typeof(s_acc) acc;
  uint32_t counters[PERF_NUM_COUNTERS];
  uint32_t idle_pct[2] = {0U, 0U};

  pbl_irq_lock();
  acc = s_acc;
  s_acc = (typeof(s_acc)){};
  for (unsigned int i = 0U; i < PERF_NUM_COUNTERS; i++) {
    counters[i] = perf_probe_counters[i] - s_last_counters[i];
    s_last_counters[i] = perf_probe_counters[i];
  }
  pbl_irq_unlock();

  const uint32_t now = prv_now_ms();
  const uint32_t span_ms = MAX(now - s_last_report_ms, 1U);
  s_last_report_ms = now;

  for (size_t i = 0U; i < MIN(pbl_soc_num_idle_states, ARRAY_LENGTH(s_last_idle_us)); i++) {
    const uint64_t idle_us = pbl_idle_residency_us(i);
    idle_pct[i] = (uint32_t)((idle_us - s_last_idle_us[i]) / (span_ms * 10U));
    s_last_idle_us[i] = idle_us;
  }

  // The cycle counter stands still while the CPU sleeps: the share of the interval it ran.
  // It wraps after 30 to 45 s, so only a span of a few seconds gives a meaningful number.
  const uint32_t cycles = pbl_cycle_get_32();
  const uint32_t run_pct = (uint32_t)(((uint64_t)(cycles - s_last_cycles) * 100U) /
                                      (((uint64_t)pbl_cycles_per_sec() * span_ms) / 1000U + 1U));
  s_last_cycles = cycles;

  if (acc.frames == 0U && acc.keys == 0U) {
    // Nothing on screen: stop until the next frame.
    s_reporting = false;
    PBL_LOG_INFO("PERF %" PRIu32 " idle %" PRIu32 " ms: run %" PRIu32 " wfi %" PRIu32
                 " stop %" PRIu32 " pct, clock irq %" PRIu32 " reprog %" PRIu32,
                 s_seq++, span_ms, run_pct, idle_pct[0], idle_pct[1], counters[PERF_CE_IRQ],
                 counters[PERF_CE_REPROG]);
    return;
  }

  PBL_LOG_INFO("PERF %" PRIu32 " frames %" PRIu32 " in %" PRIu32 " ms: avg %" PRIu32 " max %" PRIu32
               " gap %" PRIu32 " ms",
               s_seq, acc.frames, span_ms, acc.frames ? acc.frame_ms_sum / acc.frames : 0U,
               acc.frame_ms_max, acc.gap_ms_max);
  PBL_LOG_INFO("PERF %" PRIu32 " keys %" PRIu32 ", key to frame avg %" PRIu32 " max %" PRIu32 " ms",
               s_seq, acc.keys, acc.key_frames ? acc.key_frame_ms_sum / acc.key_frames : 0U,
               acc.key_frame_ms_max);
  PBL_LOG_INFO("PERF %" PRIu32 " cpu run %" PRIu32 " wfi %" PRIu32 " stop %" PRIu32
               " pct, flash erase %" PRIu32 " write %" PRIu32,
               s_seq, run_pct, idle_pct[0], idle_pct[1], counters[PERF_FLASH_ERASE],
               counters[PERF_FLASH_WRITE]);
  PBL_LOG_INFO("PERF %" PRIu32 " clock irq %" PRIu32 " reprog %" PRIu32 " stale %" PRIu32
               " repend %" PRIu32,
               s_seq, counters[PERF_CE_IRQ], counters[PERF_CE_REPROG], counters[PERF_CE_STALE],
               counters[PERF_CE_REPEND]);
  const uint32_t frames = MAX(acc.frames, 1U);
  PBL_LOG_INFO("PERF %" PRIu32 " frame parts: app %" PRIu32 " comp %" PRIu32 " flush %" PRIu32
               " wait %" PRIu32 " busy %" PRIu32 " us",
               s_seq, acc.part_us[PERF_PART_APP] / frames,
               acc.part_us[PERF_PART_COMPOSITOR] / frames, acc.flush_us / frames,
               acc.part_us[PERF_PART_WAIT] / frames, acc.part_us[PERF_PART_BUSY] / frames);
  s_seq++;

  (void)pbl_delayed_work_reschedule(&pbl_sys_workq, &s_report_work, PBL_MSEC(REPORT_PERIOD_MS));
}

// Sampling profiler
///////////////////////////////////////////////////////////////////////////////
// While the key sequence plays, TIM7 interrupts about 1000 times a second, above every
// interrupt lock, and notes where the thread it interrupted was executing: a histogram of the
// firmware in 1 KiB steps. The largest entries go to the log afterwards; the PC side turns
// them into function names with the build's ELF.

#ifdef CONFIG_SOC_STM32F4_EMULATED
static void prv_profile_start(void) {
}
static void prv_profile_stop(void) {
}
#else
#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/cmsis.h>

#define PROFILE_BASE      0x08000000U
#define PROFILE_SHIFT     10U
#define PROFILE_BUCKETS   1024U
#define PROFILE_TOP       36U
// A prime number of microseconds, so the samples do not walk in step with the kernel tick.
#define PROFILE_PERIOD_US 991U

static uint16_t s_profile[PROFILE_BUCKETS];
//! Samples in all, in a thread outside the firmware image (RAM: an app), and in an interrupt.
static uint32_t s_profile_total;
static uint32_t s_profile_other;
static uint32_t s_profile_isr;

PBL_IRQ_DIRECT(TIM7, 5, PBL_IRQ_ZERO_LATENCY) {
  // The value the handler was entered with tells which stack holds the interrupted context.
  const uint32_t exc_return = (uint32_t)__builtin_return_address(0);

  TIM7->SR = 0U;
  s_profile_total++;

  if ((exc_return & 0x4U) == 0U) {
    // The main stack: another interrupt handler was running.
    s_profile_isr++;
    return;
  }

  const uint32_t pc = ((const uint32_t *)__get_PSP())[6];
  const uint32_t bucket = (pc - PROFILE_BASE) >> PROFILE_SHIFT;
  if (bucket < PROFILE_BUCKETS) {
    if (s_profile[bucket] != UINT16_MAX) {
      s_profile[bucket]++;
    }
  } else {
    s_profile_other++;
  }
}

static void prv_profile_start(void) {
  RCC_ClocksTypeDef clocks;

  for (unsigned int i = 0U; i < PROFILE_BUCKETS; i++) {
    s_profile[i] = 0U;
  }
  s_profile_total = 0U;
  s_profile_other = 0U;
  s_profile_isr = 0U;

  soc_stm32_clock_set(SOC_STM32_APB1, RCC_APB1ENR_TIM7EN, true);
  RCC_GetClocksFreq(&clocks);
  // APB1 timers run at twice PCLK1 when APB1 is divided.
  const uint32_t timer_hz =
      clocks.PCLK1_Frequency * ((clocks.HCLK_Frequency == clocks.PCLK1_Frequency) ? 1U : 2U);
  TIM7->CR1 = 0U;
  TIM7->PSC = (timer_hz / 1000000U) - 1U;
  TIM7->ARR = PROFILE_PERIOD_US - 1U;
  TIM7->EGR = TIM_EGR_UG;
  TIM7->SR = 0U;
  TIM7->DIER = TIM_DIER_UIE;
  pbl_irq_enable(PBL_IRQN(TIM7));
  TIM7->CR1 = TIM_CR1_CEN;
}

static void prv_profile_stop(void) {
  TIM7->CR1 = 0U;
  TIM7->DIER = 0U;
  pbl_irq_disable(PBL_IRQN(TIM7));
  soc_stm32_clock_set(SOC_STM32_APB1, RCC_APB1ENR_TIM7EN, false);

  PBL_LOG_INFO("PERF profile samples %" PRIu32 " in interrupts %" PRIu32 " outside %" PRIu32,
               s_profile_total, s_profile_isr, s_profile_other);

  // The largest buckets, three to a line.
  uint32_t addr[3];
  uint32_t count[3];
  for (unsigned int n = 0U; n < PROFILE_TOP; n++) {
    unsigned int best = 0U;
    for (unsigned int i = 1U; i < PROFILE_BUCKETS; i++) {
      if (s_profile[i] > s_profile[best]) {
        best = i;
      }
    }
    addr[n % 3U] = PROFILE_BASE + (best << PROFILE_SHIFT);
    count[n % 3U] = s_profile[best];
    s_profile[best] = 0U;
    if ((n % 3U) == 2U) {
      PBL_LOG_INFO("PERF pc %" PRIx32 " %" PRIu32 " %" PRIx32 " %" PRIu32 " %" PRIx32 " %" PRIu32,
                   addr[0], count[0], addr[1], count[1], addr[2], count[2]);
    }
  }
}
#endif

// Key sequence
///////////////////////////////////////////////////////////////////////////////

struct script_step {
  uint16_t code;
  //! Time to the next step, the key being held for the first HOLD_MS of it.
  uint16_t wait_ms;
  uint8_t repeat;
};

#define HOLD_MS 60U

static const struct script_step s_script[] = {
  // To the watchface, whatever is open.
  {PBL_INPUT_KEY_BACK, 700, 3},
  // Launcher, then through the list at a walking pace and back.
  {PBL_INPUT_KEY_SELECT, 1500, 1},
  {PBL_INPUT_KEY_DOWN, 400, 6},
  {PBL_INPUT_KEY_UP, 400, 6},
  // The same as fast as a finger goes.
  {PBL_INPUT_KEY_DOWN, 120, 10},
  {PBL_INPUT_KEY_UP, 120, 10},
  // Open the first entry, look at it, leave.
  {PBL_INPUT_KEY_SELECT, 4000, 1},
  {PBL_INPUT_KEY_BACK, 1500, 1},
  {PBL_INPUT_KEY_BACK, 1500, 1},
  // Timeline and back.
  {PBL_INPUT_KEY_DOWN, 2500, 1},
  {PBL_INPUT_KEY_BACK, 1500, 1},
};

static unsigned int s_step;
static unsigned int s_repeat;
static bool s_pressed;
static bool s_script_running;
//! Stop after the group in progress, for a screenshot.
static bool s_stepping;

//! The frames of the key pressed last, if any, before the next one or the end.
static void prv_log_key(void) {
  pbl_irq_lock();
  const typeof(s_key) key = s_key;
  s_key.active = false;
  pbl_irq_unlock();

  if (key.active) {
    PBL_LOG_INFO("PERF key %u/%u: first %" PRIu32 " last %" PRIu32 " ms, %" PRIu32 " frames",
                 key.step, key.repeat, key.frames ? key.first_ms : 0U,
                 key.frames ? key.last_ms : 0U, key.frames);
  }
}

static void prv_press_key(void) {
  pbl_irq_lock();
  s_key = (typeof(s_key)){
    .active = true,
    .step = s_step,
    .repeat = s_repeat,
    .press_ms = prv_now_ms(),
  };
  pbl_irq_unlock();
}

void perf_probe_mark(const char *what) {
  PBL_LOG_INFO("PERF mark: %s", what);
}

//! Which screen the next key goes to.
static void prv_log_screen(void) {
  const PebbleProcessMd *md = app_manager_get_current_app_md();
  Window *modal = modal_manager_get_top_window();
  Window *window = app_window_stack_get_top_window();
  const char *app_name = (md != nullptr) ? process_metadata_get_name(md) : nullptr;

  // A log line takes two strings at most.
  PBL_LOG_INFO("PERF screen %u/%u: app %s window %s", s_step, s_repeat,
               (app_name != nullptr) ? app_name : "-",
               (window != nullptr) ? window_get_debug_name(window) : "-");
  if (modal != nullptr) {
    PBL_LOG_INFO("PERF screen %u/%u: modal %s", s_step, s_repeat, window_get_debug_name(modal));
  }
}

// The keys are pressed from the high priority queue: the normal one starves while an animation
// keeps the CPU busy, and the keys would come late. The end of the sequence (key line, profile)
// goes back to the normal queue, where the flash log is written.
static void prv_script(struct pbl_work *work);
static PBL_DELAYED_WORK_DEFINE(s_script_work, prv_script);

static void prv_script_done(struct pbl_work *work);
static PBL_WORK_DEFINE(s_script_done_work, prv_script_done);

static void prv_bench(struct pbl_work *work);
static PBL_DELAYED_WORK_DEFINE(s_bench_work, prv_bench);
static bool s_bench_running;

static void prv_script_done(struct pbl_work *work) {
  prv_log_key();
  PBL_LOG_INFO("PERF script done");
  prv_profile_stop();
  s_script_running = false;
  if (s_bench_running) {
    (void)pbl_delayed_work_reschedule(&pbl_sys_workq, &s_bench_work, PBL_MSEC(100));
  }
}

static void prv_script(struct pbl_work *work) {
  if (s_step >= ARRAY_LENGTH(s_script)) {
    (void)pbl_work_submit(&pbl_sys_workq, &s_script_done_work);
    return;
  }

  const struct script_step *step = &s_script[s_step];
  uint32_t next_ms;

  if (!s_pressed) {
    prv_log_key();
    prv_log_screen();
    prv_press_key();
    pbl_input_report_key(step->code, true, true);
    s_pressed = true;
    next_ms = HOLD_MS;
  } else {
    pbl_input_report_key(step->code, false, true);
    s_pressed = false;
    next_ms = step->wait_ms - HOLD_MS;
    if (++s_repeat >= step->repeat) {
      s_repeat = 0U;
      s_step++;
      if (s_stepping) {
        // The group is through: wait for the next request.
        s_script_running = false;
        if (s_step >= ARRAY_LENGTH(s_script)) {
          s_stepping = false;
          prv_log_key();
          PBL_LOG_INFO("PERF steps done");
        }
        return;
      }
    }
  }

  (void)pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_script_work, PBL_MSEC(next_ms));
}

void perf_probe_script_step(void) {
  if (s_script_running) {
    return;
  }

  if (!s_stepping) {
    s_stepping = true;
    s_step = 0U;
    s_repeat = 0U;
    s_pressed = false;
  }
  s_script_running = true;
  (void)pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_script_work, PBL_MSEC(100));
}

void perf_probe_script_start(void) {
  if (s_script_running) {
    return;
  }

  s_stepping = false;

  s_script_running = true;
  s_step = 0U;
  s_repeat = 0U;
  s_pressed = false;
  s_key.active = false;
  PBL_LOG_INFO("PERF script start");
  prv_profile_start();
  (void)pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_script_work, PBL_MSEC(500));
}

// Measurement by the protocol
///////////////////////////////////////////////////////////////////////////////
// The watch measures on its own, so that every run has the same conditions: Bluetooth is
// stopped (which no phone, notification or connection interval can disturb), the sequence
// plays BENCH_RUNS times with a pause in between, and Bluetooth comes back. The results are
// in the flash log. The stop is an override, not the airplane mode setting: a reset in the
// middle brings Bluetooth back.

#define BENCH_RUNS      3U
#define BENCH_LEAD_MS   5000U
#define BENCH_SETTLE_MS 20000U
#define BENCH_PAUSE_MS  15000U

enum bench_state {
  BENCH_STOP_BLUETOOTH,
  BENCH_RUN,
  BENCH_RUN_DONE,
  BENCH_END,
  BENCH_CHECK_BLUETOOTH,
};

#define BENCH_BT_CHECK_MS 15000U
#define BENCH_BT_RETRIES  5U

static enum bench_state s_bench_state;
static unsigned int s_bench_run;
static unsigned int s_bench_bt_retries;

static void prv_bench(struct pbl_work *work) {
  uint32_t next_ms = 0U;

  switch (s_bench_state) {
    case BENCH_STOP_BLUETOOTH:
      bt_ctl_set_override_mode(BtCtlModeOverrideStop);
      PBL_LOG_INFO("PERF bench: bluetooth stopped");
      s_bench_state = BENCH_RUN;
      next_ms = BENCH_SETTLE_MS;
      break;
    case BENCH_RUN:
      s_bench_run++;
      PBL_LOG_INFO("PERF run %u start", s_bench_run);
      s_bench_state = BENCH_RUN_DONE;
      // The end of the sequence schedules this work again.
      perf_probe_script_start();
      return;
    case BENCH_RUN_DONE:
      PBL_LOG_INFO("PERF run %u done", s_bench_run);
      if (s_bench_run < BENCH_RUNS) {
        s_bench_state = BENCH_RUN;
        next_ms = BENCH_PAUSE_MS;
      } else {
        // Leave the last report time to come before the radio is back.
        s_bench_state = BENCH_END;
        next_ms = 2U * REPORT_PERIOD_MS;
      }
      break;
    case BENCH_END:
      bt_ctl_set_override_mode(BtCtlModeOverrideNone);
      PBL_LOG_INFO("PERF bench done");
      s_bench_state = BENCH_CHECK_BLUETOOTH;
      s_bench_bt_retries = 0U;
      next_ms = BENCH_BT_CHECK_MS;
      break;
    case BENCH_CHECK_BLUETOOTH:
      // The controller does not always start again (seen on the watch); the results could
      // then not be fetched. Stop and start until it runs, and say how often it took.
      if (bt_ctl_is_bluetooth_running() || !bt_ctl_is_bluetooth_active() ||
          s_bench_bt_retries >= BENCH_BT_RETRIES) {
        PBL_LOG_INFO("PERF bench: bluetooth running %d after %u retries",
                     (int)bt_ctl_is_bluetooth_running(), s_bench_bt_retries);
        s_bench_running = false;
        return;
      }
      s_bench_bt_retries++;
      bt_ctl_set_override_mode(BtCtlModeOverrideStop);
      bt_ctl_set_override_mode(BtCtlModeOverrideNone);
      next_ms = BENCH_BT_CHECK_MS;
      break;
  }

  (void)pbl_delayed_work_reschedule(&pbl_sys_workq, &s_bench_work, PBL_MSEC(next_ms));
}

void perf_probe_bench_start(void) {
  if (s_bench_running || s_script_running) {
    return;
  }

  s_bench_running = true;
  s_bench_state = BENCH_STOP_BLUETOOTH;
  s_bench_run = 0U;
  PBL_LOG_INFO("PERF bench start");
  (void)pbl_delayed_work_reschedule(&pbl_sys_workq, &s_bench_work, PBL_MSEC(BENCH_LEAD_MS));
}

#endif // CONFIG_LEGACY_PERF_PROBE
