/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

void RTC_WriteBackupRegister(uint32_t reg, uint32_t value);
uint32_t RTC_ReadBackupRegister(uint32_t reg);

#include "../../../pebble/system/reboot_reason.c"

#include <string.h>

#include <pbl/util/size.h>

#include <clar.h>
#include <kernel/pebble_tasks.h>

#define NUM_BKP          20
#define ERASE_RECORD     0x80012000U
#define CALLBACK_POINTER 0x08061235U

static uint32_t s_bkp[NUM_BKP];

void RTC_WriteBackupRegister(uint32_t reg, uint32_t value) {
  cl_assert(reg < NUM_BKP);
  s_bkp[reg] = value;
}

uint32_t RTC_ReadBackupRegister(uint32_t reg) {
  cl_assert(reg < NUM_BKP);
  return s_bkp[reg];
}

bool pbl_kernel_is_running(void) {
  return false;
}

static bool prv_prf302_names(uint32_t task) {
  return task <= 7 || task == 9;
}

static bool prv_stock4_names(uint32_t task) {
  return task <= 8 || task == 10;
}

static bool prv_prints_without_assert(bool (*names)(uint32_t task)) {
  const uint32_t code = s_bkp[5] & 0xff;
  if (code == RebootReasonCode_StackOverflow) {
    return names((s_bkp[5] >> 16) & 0xff);
  }
  if (code == RebootReasonCode_EventQueueFull) {
    return names(s_bkp[7]);
  }
  return true;
}

static void prv_preset_others(void) {
  for (int i = 0; i < NUM_BKP; i++) {
    s_bkp[i] = 0xa5a50000U | i;
  }
  for (int i = 5; i <= 10; i++) {
    s_bkp[i] = 0;
  }
  s_bkp[11] = ERASE_RECORD;
}

static void prv_assert_others_kept(void) {
  for (int i = 0; i < NUM_BKP; i++) {
    if (i >= 5 && i <= 10) {
      continue;
    }
    cl_assert_equal_i(s_bkp[i], i == 11 ? ERASE_RECORD : (0xa5a50000U | i));
  }
}

static RebootReason prv_reason(RebootReasonCode code, uint8_t data8_0, uint32_t w1, uint32_t w2,
                               uint32_t w3) {
  RebootReason r = {.code = code, .data8 = {data8_0, 0}};
  uint32_t *raw = (uint32_t *)&r;
  raw[1] = w1;
  raw[2] = w2;
  raw[3] = w3;
  return r;
}

static void prv_check_walk(uint32_t w1, uint32_t w2, uint32_t w3) {
  for (int code = 0; code <= RebootReasonCode_HardLockup; code++) {
    for (int task = 0; task <= PebbleTask_Unknown; task++) {
      prv_preset_others();
      RebootReason in = prv_reason(code, task, w1, w2, w3);
      if (code != RebootReasonCode_StackOverflow) {
        in.data8[1] = 0x83;
      }
      reboot_reason_set(&in);

      cl_assert_(prv_prints_without_assert(prv_prf302_names), "PRF 3.0.2 asserts");
      cl_assert_(prv_prints_without_assert(prv_stock4_names), "stock 4.x asserts");
      prv_assert_others_kept();

      RebootReason out;
      reboot_reason_get(&out);
      cl_assert_equal_m(&in, &out, sizeof(in));
    }
  }
}

void test_reboot_reason_stock_layout__initialize(void) {
  prv_preset_others();
}

void test_reboot_reason_stock_layout__every_code_prints_in_prf302_and_stock4_with_pointers(void) {
  prv_check_walk(CALLBACK_POINTER, 0x08051ab9U, 0x0805f001U);
}

void test_reboot_reason_stock_layout__every_code_prints_in_prf302_and_stock4_with_small_words(
    void) {
  for (uint32_t w = 0; w <= 11; w++) {
    prv_check_walk(w, w, w);
  }
}

void test_reboot_reason_stock_layout__event_queue_in_stock_order(void) {
  RebootReason in =
      prv_reason(RebootReasonCode_EventQueueFull, 0, 0x0801aaaaU, CALLBACK_POINTER, 0x0803ccccU);
  reboot_reason_set(&in);

  cl_assert_equal_i(s_bkp[5], RebootReasonCode_EventQueueFull);
  cl_assert_equal_i(s_bkp[6], 0);
  cl_assert_equal_i(s_bkp[7], PebbleTask_KernelMain);
  cl_assert_equal_i(s_bkp[8], 0x0801aaaaU);
  cl_assert_equal_i(s_bkp[9], CALLBACK_POINTER);
  cl_assert_equal_i(s_bkp[10], 0x0803ccccU);
}

void test_reboot_reason_stock_layout__watchdog_in_stock_order_without_mutex_words(void) {
  RebootReason in =
      prv_reason(RebootReasonCode_Watchdog, 0x3, 0x0804bcdeU, 0x0805cdefU, CALLBACK_POINTER);
  in.data8[1] = 0x83;
  reboot_reason_set(&in);

  cl_assert_equal_i(s_bkp[5], RebootReasonCode_Watchdog | (0x3 << 16) | (0x83U << 24));
  cl_assert_equal_i(s_bkp[6], 0);
  cl_assert_equal_i(s_bkp[7], 0x0804bcdeU);
  cl_assert_equal_i(s_bkp[8], 0x0805cdefU);
  cl_assert_equal_i(s_bkp[9], CALLBACK_POINTER);
  cl_assert_equal_i(s_bkp[10], 0);
}

void test_reboot_reason_stock_layout__hard_lockup_in_the_stuck_task_words(void) {
  RebootReason in = prv_reason(RebootReasonCode_HardLockup, 0, 0x08001234U, 0x08005678U, 15);
  reboot_reason_set(&in);

  cl_assert_equal_i(s_bkp[6], 0);
  cl_assert_equal_i(s_bkp[7], 0x08001234U);
  cl_assert_equal_i(s_bkp[8], 0x08005678U);
  cl_assert_equal_i(s_bkp[9], 15);
  cl_assert_equal_i(s_bkp[10], 0);
}

void test_reboot_reason_stock_layout__extra_word_in_dr6(void) {
  const RebootReasonCode codes[] = {
    RebootReasonCode_Assert, RebootReasonCode_HardFault, RebootReasonCode_LauncherPanic
  };
  for (size_t i = 0; i < ARRAY_LENGTH(codes); i++) {
    reboot_reason_clear();
    RebootReason in = prv_reason(codes[i], 0, 0x0804a001U, 0, 0);
    reboot_reason_set(&in);

    cl_assert_equal_i(s_bkp[5], codes[i]);
    cl_assert_equal_i(s_bkp[6], 0x0804a001U);
    cl_assert_equal_i(s_bkp[7], 0);
    cl_assert_equal_i(s_bkp[8], 0);
  }
}

void test_reboot_reason_stock_layout__unknown_task_overflow_keeps_its_number(void) {
  RebootReason in =
      prv_reason(RebootReasonCode_StackOverflow, PebbleTask_Unknown, 0x0804a003U, 0, 0);
  reboot_reason_set(&in);

  cl_assert_equal_i((s_bkp[5] >> 16) & 0xff, PebbleTask_App);
  cl_assert_equal_i((s_bkp[5] >> 24) & 0xff, PebbleTask_Unknown);
  cl_assert_equal_i(s_bkp[6], 0x0804a003U);

  RebootReason out;
  reboot_reason_get(&out);
  cl_assert_equal_i(out.data8[0], PebbleTask_Unknown);
  cl_assert_equal_i(out.data8[1], 0);
}

void test_reboot_reason_stock_layout__known_task_overflow_unchanged(void) {
  RebootReason in = prv_reason(RebootReasonCode_StackOverflow, PebbleTask_BTHCI, 0, 0, 0);
  reboot_reason_set(&in);

  cl_assert_equal_i(s_bkp[5], RebootReasonCode_StackOverflow | (PebbleTask_BTHCI << 16));
}

void test_reboot_reason_stock_layout__restart_keeps_the_task_byte(void) {
  RebootReason in = prv_reason(RebootReasonCode_StackOverflow, PebbleTask_Unknown, 0, 0, 0);
  reboot_reason_set(&in);
  prv_sys_off_prepare(PBL_SYS_OFF_RESTART_PREPARE, nullptr);

  cl_assert(prv_prints_without_assert(prv_prf302_names));
  cl_assert(prv_prints_without_assert(prv_stock4_names));

  RebootReason out;
  reboot_reason_get(&out);
  cl_assert(out.restarted_safely);
  cl_assert_equal_i(out.code, RebootReasonCode_StackOverflow);
  cl_assert_equal_i(out.data8[0], PebbleTask_Unknown);
}

void test_reboot_reason_stock_layout__reads_a_stock_event_queue_record(void) {
  s_bkp[5] = RebootReasonCode_EventQueueFull;
  s_bkp[7] = PebbleTask_KernelMain;
  s_bkp[8] = 0x0801aaaaU;
  s_bkp[9] = 0x0802bbbbU;
  s_bkp[10] = 0x0803ccccU;

  RebootReason out;
  reboot_reason_get(&out);
  cl_assert_equal_i(out.event_queue.push_lr, 0x0801aaaaU);
  cl_assert_equal_i(out.event_queue.current_event, 0x0802bbbbU);
  cl_assert_equal_i(out.event_queue.dropped_event, 0x0803ccccU);
}

void test_reboot_reason_stock_layout__reads_a_stock_watchdog_record(void) {
  s_bkp[5] = RebootReasonCode_Watchdog;
  s_bkp[7] = 0x0801aaaaU;
  s_bkp[8] = 0x0802bbbbU;
  s_bkp[9] = 0x0803ccccU;

  RebootReason out;
  reboot_reason_get(&out);
  cl_assert_equal_i(out.watchdog.stuck_task_pc, 0x0801aaaaU);
  cl_assert_equal_i(out.watchdog.stuck_task_lr, 0x0802bbbbU);
  cl_assert_equal_i(out.watchdog.stuck_task_callback, 0x0803ccccU);
}

void test_reboot_reason_stock_layout__set_refused_while_one_is_stored(void) {
  RebootReason first = prv_reason(RebootReasonCode_Assert, 0, 0x0804a001U, 0, 0);
  reboot_reason_set(&first);
  RebootReason second =
      prv_reason(RebootReasonCode_EventQueueFull, 0, 0x0801aaaaU, CALLBACK_POINTER, 1);
  reboot_reason_set(&second);

  cl_assert_equal_i(s_bkp[5], RebootReasonCode_Assert);
  cl_assert_equal_i(s_bkp[6], 0x0804a001U);
  cl_assert_equal_i(s_bkp[9], 0);
  cl_assert_equal_i(s_bkp[10], 0);
}

void test_reboot_reason_stock_layout__clear_empties_dr5_to_dr10_only(void) {
  for (int i = 5; i <= 10; i++) {
    s_bkp[i] = 0xffffffffU;
  }
  reboot_reason_clear();

  for (int i = 5; i <= 10; i++) {
    cl_assert_equal_i(s_bkp[i], 0);
  }
  prv_assert_others_kept();
}
