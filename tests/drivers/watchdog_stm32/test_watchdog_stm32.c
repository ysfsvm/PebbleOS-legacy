/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/watchdog/stm32.c"
#include "iwdg_model.h"

static int s_hardlockup_feeds;

void pbl_task_wdt_hardlockup_feed(void) {
  s_hardlockup_feeds++;
}

static void prv_assert_access(int i, bool write, uintptr_t addr, uint32_t val) {
  cl_assert(i < s_log_len);
  cl_assert_equal_b(s_log[i].write, write);
  cl_assert_equal_i(s_log[i].addr, addr);
  cl_assert_equal_i(s_log[i].val, val);
}

void test_watchdog_stm32__initialize(void) {
  m_reset();
  s_hardlockup_feeds = 0;
}

void test_watchdog_stm32__init(void) {
  s_m.apb1fz = 1U << 21;

  watchdog_init();

  cl_assert_equal_i(s_log_len, 6);
  prv_assert_access(0, true, M_IWDG_KR, 0x5555U);
  prv_assert_access(1, true, M_IWDG_PR, 4U);
  prv_assert_access(2, true, M_IWDG_RLR, 0xfffU);
  prv_assert_access(3, true, M_IWDG_KR, 0x0000U);
  prv_assert_access(4, false, M_APB1FZ, 1U << 21);
  prv_assert_access(5, true, M_APB1FZ, (1U << 21) | (1U << 12));
  cl_assert(!s_m.unlocked);
  cl_assert(!s_m.running);
}

void test_watchdog_stm32__start_and_feed(void) {
  watchdog_start();
  cl_assert_equal_i(s_log_len, 2);
  prv_assert_access(0, true, M_IWDG_KR, 0xccccU);
  prv_assert_access(1, true, M_IWDG_KR, 0xaaaaU);
  cl_assert(s_m.running);
  cl_assert_equal_i(s_hardlockup_feeds, 1);

  s_log_len = 0;
  watchdog_feed();
  cl_assert_equal_i(s_log_len, 1);
  prv_assert_access(0, true, M_IWDG_KR, 0xaaaaU);
  cl_assert_equal_i(s_m.reloads, 2);
  cl_assert_equal_i(s_hardlockup_feeds, 2);

  s_log_len = 0;
  watchdog_stop();
  cl_assert_equal_i(s_log_len, 0);
}

void test_watchdog_stm32__each_reset_flag_maps_to_its_mask_bit(void) {
  for (uint32_t i = 0U; i < 7U; i++) {
    m_reset();
    s_m.csr |= 1U << (25U + i);

    const McuRebootReason reason = watchdog_get_reset_flag();
    cl_assert_equal_i(reason.reset_mask, 1U << i);
    cl_assert_equal_b(watchdog_check_reset_flag(), i == 4U);
    for (int k = 0; k < s_log_len; k++) {
      cl_assert(!s_log[k].write);
    }
  }
  cl_assert_equal_i(((McuRebootReason){.independent_watchdog_reset = true}).reset_mask, 1U << 4);
}

void test_watchdog_stm32__clear_returns_the_flags_and_sets_rmvf(void) {
  s_m.csr |= (1U << 29) | (1U << 26);

  const McuRebootReason reason = watchdog_clear_reset_flag();

  cl_assert(reason.independent_watchdog_reset);
  cl_assert(reason.pin_reset);
  cl_assert(!reason.power_on_reset);
  cl_assert_equal_i(s_log_len, 3);
  prv_assert_access(1, false, M_RCC_CSR, M_CSR_LSION | (1U << 1) | (1U << 29) | (1U << 26));
  prv_assert_access(2, true, M_RCC_CSR,
                    M_CSR_LSION | (1U << 1) | (1U << 29) | (1U << 26) | M_CSR_RMVF);
  cl_assert_equal_i(s_m.csr & M_CSR_FLAGS, 0);
  cl_assert(!watchdog_check_reset_flag());
}
