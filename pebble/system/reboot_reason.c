/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "reboot_reason.h"

#include <pbl/kernel/irq.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/interrupts.h>
#ifdef CONFIG_SOC_STM32F4
#include <pbl/soc/stm32/cmsis.h>
#elif defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX)
extern void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value);
extern uint32_t RTC_ReadBackupRegister(uint32_t reg_id);
#endif
static_assert(sizeof(RebootReason) == sizeof(uint32_t[4]), "RebootReason is a funny size");

#ifdef CONFIG_LEGACY_REBOOT_REASON_STOCK_LAYOUT
#include <string.h>

#include <kernel/pebble_tasks.h>

#define STOCK_WORDS 6

static const uint32_t s_stock_regs[STOCK_WORDS] = {
  RTC_BKP_DR5, RTC_BKP_DR6, RTC_BKP_DR7, RTC_BKP_DR8, RTC_BKP_DR9, RTC_BKP_DR10,
};

// A task number the stock firmwares accept
#define STOCK_TASK_PLACEHOLDER PebbleTask_App

static void prv_to_stock(const RebootReason *reason, uint32_t stock[STOCK_WORDS]) {
  RebootReason r = *reason;
  uint32_t raw[4];

  memset(stock, 0, STOCK_WORDS * sizeof(uint32_t));
  if (r.code == RebootReasonCode_StackOverflow && r.data8[0] >= NumPebbleTask) {
    r.data8[1] = r.data8[0];
    r.data8[0] = STOCK_TASK_PLACEHOLDER;
  }
  memcpy(raw, &r, sizeof(raw));

  stock[0] = raw[0];
  switch (r.code) {
    case RebootReasonCode_Watchdog:
    case RebootReasonCode_HardLockup:
      stock[2] = raw[1];
      stock[3] = raw[2];
      stock[4] = raw[3];
      break;
    case RebootReasonCode_EventQueueFull:
      stock[2] = PebbleTask_KernelMain;
      stock[3] = raw[1];
      stock[4] = raw[2];
      stock[5] = raw[3];
      break;
    default:
      stock[1] = raw[1];
      stock[2] = raw[2];
      stock[3] = raw[3];
      break;
  }
}

static void prv_from_stock(const uint32_t stock[STOCK_WORDS], RebootReason *reason) {
  uint32_t raw[4] = {stock[0], stock[1], stock[2], stock[3]};
  RebootReason r;

  memcpy(&r, raw, sizeof(r));
  switch (r.code) {
    case RebootReasonCode_Watchdog:
    case RebootReasonCode_HardLockup:
      raw[1] = stock[2];
      raw[2] = stock[3];
      raw[3] = stock[4];
      break;
    case RebootReasonCode_EventQueueFull:
      raw[1] = stock[3];
      raw[2] = stock[4];
      raw[3] = stock[5];
      break;
    default:
      break;
  }
  memcpy(reason, raw, sizeof(raw));
  if (reason->code == RebootReasonCode_StackOverflow && reason->data8[1] != 0) {
    reason->data8[0] = reason->data8[1];
    reason->data8[1] = 0;
  }
}
#endif

void reboot_reason_set(RebootReason *reason) {
#ifdef CONFIG_SOC_NRF52
  uint32_t *raw = (uint32_t *)reason;

  if (retained_read(REBOOT_REASON_REGISTER_1)) {
    // It's not safe to log if we're called from an ISR or from a FreeRTOS critical section (basepri
    // != 0)
    if (!mcu_state_is_isr() && !pbl_irq_is_locked() && pbl_kernel_is_running()) {
      PBL_LOG_WRN("Reboot reason is already set");
    }
    return;
  }

  retained_write(REBOOT_REASON_REGISTER_1, raw[0]);
  retained_write(REBOOT_REASON_STUCK_TASK_PC, raw[1]);
  retained_write(REBOOT_REASON_STUCK_TASK_LR, raw[2]);
  retained_write(REBOOT_REASON_STUCK_TASK_CALLBACK, raw[3]);
#elifdef CONFIG_SOC_SF32LB52
  uint32_t *raw = (uint32_t *)reason;

  if (retained_read(REBOOT_REASON_REGISTER_1)) {
    // It's not safe to log if we're called from an ISR or from a FreeRTOS critical section (basepri
    // != 0)
    if (!mcu_state_is_isr() && !pbl_irq_is_locked() && pbl_kernel_is_running()) {
      PBL_LOG_WRN("Reboot reason is already set");
    }
    return;
  }

  retained_write(REBOOT_REASON_REGISTER_1, raw[0]);
  retained_write(REBOOT_REASON_STUCK_TASK_PC, raw[1]);
  retained_write(REBOOT_REASON_STUCK_TASK_LR, raw[2]);
  retained_write(REBOOT_REASON_STUCK_TASK_CALLBACK, raw[3]);
#elif defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX) || defined(CONFIG_SOC_STM32F4)
  if (RTC_ReadBackupRegister(REBOOT_REASON_REGISTER_1)) {
    // It's not safe to log if we're called from an ISR or from a FreeRTOS critical section (basepri
    // != 0)
    if (!mcu_state_is_isr() && !pbl_irq_is_locked() && pbl_kernel_is_running()) {
      PBL_LOG_WRN("Reboot reason is already set");
    }
    return;
  }

#ifdef CONFIG_LEGACY_REBOOT_REASON_STOCK_LAYOUT
  uint32_t stock[STOCK_WORDS];
  prv_to_stock(reason, stock);
  for (int i = 0; i < STOCK_WORDS; i++) {
    RTC_WriteBackupRegister(s_stock_regs[i], stock[i]);
  }
#else
  uint32_t *raw = (uint32_t *)reason;
  RTC_WriteBackupRegister(REBOOT_REASON_REGISTER_1, raw[0]);
  RTC_WriteBackupRegister(REBOOT_REASON_STUCK_TASK_PC, raw[1]);
  RTC_WriteBackupRegister(REBOOT_REASON_STUCK_TASK_LR, raw[2]);
  RTC_WriteBackupRegister(REBOOT_REASON_STUCK_TASK_CALLBACK, raw[3]);
#endif
#endif
}

static void prv_sys_off_prepare(enum pbl_sys_off_mode mode, void *user_data) {
  RebootReason reason;
#ifdef CONFIG_LEGACY_REBOOT_REASON_STOCK_LAYOUT
  // The first word as stored, task byte undecoded
  memset(&reason, 0, sizeof(reason));
  *(uint32_t *)&reason = RTC_ReadBackupRegister(REBOOT_REASON_REGISTER_1);
#else
  reboot_reason_get(&reason);
#endif
  reason.restarted_safely = true;

#ifdef CONFIG_SOC_NRF52
  uint32_t *raw = (uint32_t *)&reason;
  retained_write(REBOOT_REASON_REGISTER_1, *raw);
#elifdef CONFIG_SOC_SF32LB52
  uint32_t *raw = (uint32_t *)&reason;
  retained_write(REBOOT_REASON_REGISTER_1, *raw);
#elif defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX) || defined(CONFIG_SOC_STM32F4)
  uint32_t *raw = (uint32_t *)&reason;
  RTC_WriteBackupRegister(REBOOT_REASON_REGISTER_1, *raw);
#endif
}

PBL_SYS_OFF_HANDLER_DEFINE(reboot_reason_restart, PBL_SYS_OFF_RESTART_PREPARE, PBL_SYS_OFF_PRIO_LOW,
                           prv_sys_off_prepare, nullptr);
PBL_SYS_OFF_HANDLER_DEFINE(reboot_reason_power_off, PBL_SYS_OFF_POWER_OFF_PREPARE,
                           PBL_SYS_OFF_PRIO_LOW, prv_sys_off_prepare, nullptr);

void reboot_reason_get(RebootReason *reason) {
#ifdef CONFIG_SOC_NRF52
  uint32_t *raw = (uint32_t *)reason;
  raw[0] = retained_read(REBOOT_REASON_REGISTER_1);
  raw[1] = retained_read(REBOOT_REASON_STUCK_TASK_PC);
  raw[2] = retained_read(REBOOT_REASON_STUCK_TASK_LR);
  raw[3] = retained_read(REBOOT_REASON_STUCK_TASK_CALLBACK);
#elifdef CONFIG_SOC_SF32LB52
  uint32_t *raw = (uint32_t *)reason;
  raw[0] = retained_read(REBOOT_REASON_REGISTER_1);
  raw[1] = retained_read(REBOOT_REASON_STUCK_TASK_PC);
  raw[2] = retained_read(REBOOT_REASON_STUCK_TASK_LR);
  raw[3] = retained_read(REBOOT_REASON_STUCK_TASK_CALLBACK);
#elif defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX) || defined(CONFIG_SOC_STM32F4)
#ifdef CONFIG_LEGACY_REBOOT_REASON_STOCK_LAYOUT
  uint32_t stock[STOCK_WORDS];
  for (int i = 0; i < STOCK_WORDS; i++) {
    stock[i] = RTC_ReadBackupRegister(s_stock_regs[i]);
  }
  prv_from_stock(stock, reason);
#else
  uint32_t *raw = (uint32_t *)reason;
  raw[0] = RTC_ReadBackupRegister(REBOOT_REASON_REGISTER_1);
  raw[1] = RTC_ReadBackupRegister(REBOOT_REASON_STUCK_TASK_PC);
  raw[2] = RTC_ReadBackupRegister(REBOOT_REASON_STUCK_TASK_LR);
  raw[3] = RTC_ReadBackupRegister(REBOOT_REASON_STUCK_TASK_CALLBACK);
#endif
#endif
}

void reboot_reason_clear(void) {
#ifdef CONFIG_SOC_NRF52
  retained_write(REBOOT_REASON_REGISTER_1, 0);
  retained_write(REBOOT_REASON_STUCK_TASK_PC, 0);
  retained_write(REBOOT_REASON_STUCK_TASK_LR, 0);
  retained_write(REBOOT_REASON_STUCK_TASK_CALLBACK, 0);
#elifdef CONFIG_SOC_SF32LB52
  retained_write(REBOOT_REASON_REGISTER_1, 0);
  retained_write(REBOOT_REASON_STUCK_TASK_PC, 0);
  retained_write(REBOOT_REASON_STUCK_TASK_LR, 0);
  retained_write(REBOOT_REASON_STUCK_TASK_CALLBACK, 0);
#elif defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX) || defined(CONFIG_SOC_STM32F4)
#ifdef CONFIG_LEGACY_REBOOT_REASON_STOCK_LAYOUT
  for (int i = 0; i < STOCK_WORDS; i++) {
    RTC_WriteBackupRegister(s_stock_regs[i], 0);
  }
#else
  RTC_WriteBackupRegister(REBOOT_REASON_REGISTER_1, 0);
  RTC_WriteBackupRegister(REBOOT_REASON_STUCK_TASK_PC, 0);
  RTC_WriteBackupRegister(REBOOT_REASON_STUCK_TASK_LR, 0);
  RTC_WriteBackupRegister(REBOOT_REASON_STUCK_TASK_CALLBACK, 0);
#endif
#endif
}

uint32_t reboot_get_slot_of_last_launched_app(void) {
#ifdef CONFIG_SOC_NRF52
  return retained_read(SLOT_OF_LAST_LAUNCHED_APP);
#elifdef CONFIG_SOC_SF32LB52
  return retained_read(SLOT_OF_LAST_LAUNCHED_APP);
#elif defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX) || defined(CONFIG_SOC_STM32F4)
  return RTC_ReadBackupRegister(SLOT_OF_LAST_LAUNCHED_APP);
#endif
}

void reboot_set_slot_of_last_launched_app(uint32_t app_slot) {
#ifdef CONFIG_SOC_NRF52
  retained_write(SLOT_OF_LAST_LAUNCHED_APP, app_slot);
#elifdef CONFIG_SOC_SF32LB52
  retained_write(SLOT_OF_LAST_LAUNCHED_APP, app_slot);
#elif defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX) || defined(CONFIG_SOC_STM32F4)
  RTC_WriteBackupRegister(SLOT_OF_LAST_LAUNCHED_APP, app_slot);
#endif
}
