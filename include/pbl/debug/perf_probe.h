/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @file perf_probe.h
//! Measurement build (CONFIG_LEGACY_PERF_PROBE): counters of what the user sees (frames, time
//! from a key to the next frame) and of what may cost it (CPU, clock events, flash), logged
//! every few seconds while there is activity, and a fixed key sequence to compare builds with.
//! All of it compiles to nothing without the option.

enum perf_probe_counter {
  //! Clock event interrupts taken.
  PERF_CE_IRQ,
  //! Clock event interrupts with nothing expired.
  PERF_CE_STALE,
  //! Clock event timer stopped and written again.
  PERF_CE_REPROG,
  //! Clock event raised by the idle check.
  PERF_CE_REPEND,
  //! Flash erase operations started.
  PERF_FLASH_ERASE,
  //! Flash write calls.
  PERF_FLASH_WRITE,
  PERF_NUM_COUNTERS,
};

//! Timed parts of a frame, in microseconds.
enum perf_probe_part {
  //! A privileged app drawing its window.
  PERF_PART_APP,
  //! The compositor drawing: app frame copy, modals, transitions.
  PERF_PART_COMPOSITOR,
  //! The display not ready yet for a new frame.
  PERF_PART_WAIT,
  //! The display not ready yet for the next line of a frame.
  PERF_PART_BUSY,
  PERF_NUM_PARTS,
};

#ifdef CONFIG_LEGACY_PERF_PROBE
extern uint32_t perf_probe_counters[PERF_NUM_COUNTERS];

//! Count an event. Usable anywhere, interrupts included.
static inline void perf_probe_count(enum perf_probe_counter counter) {
  perf_probe_counters[counter]++;
}

//! A display update was handed to the panel / has reached it. From any context.
void perf_probe_frame_begin(void);
void perf_probe_frame_end(void);

//! Start / end of a part of a frame. Parts do not nest. From any context; ignored in an
//! unprivileged one.
void perf_probe_part_begin(enum perf_probe_part part);
void perf_probe_part_end(enum perf_probe_part part);

//! Play the fixed key sequence.
void perf_probe_script_start(void);

//! Play the next group of the key sequence only, for a screenshot between groups.
void perf_probe_script_step(void);

//! Measure by the protocol: stop Bluetooth, play the sequence three times, start it again.
void perf_probe_bench_start(void);

//! Note in the log something that disturbs a measurement, e.g. a notification popping up.
void perf_probe_mark(const char *what);
#else
static inline void perf_probe_count(enum perf_probe_counter counter) {
  (void)counter;
}
static inline void perf_probe_frame_begin(void) {
}
static inline void perf_probe_frame_end(void) {
}
static inline void perf_probe_part_begin(enum perf_probe_part part) {
  (void)part;
}
static inline void perf_probe_part_end(enum perf_probe_part part) {
  (void)part;
}
static inline void perf_probe_script_start(void) {
}
static inline void perf_probe_script_step(void) {
}
static inline void perf_probe_bench_start(void) {
}
static inline void perf_probe_mark(const char *what) {
  (void)what;
}
#endif
