/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

//! Clears the stock bootloader's stuck button count of a released key.

#include <pbl/input/input.h>
#include <pbl/kernel/irq.h>
#include <pbl/soc/stm32/cmsis.h>

#include <system/rtc_registers.h>

//! Byte of a key's count, in the bootloader's button order.
static int prv_button_index(uint16_t code) {
  switch (code) {
    case PBL_INPUT_KEY_BACK:
      return 0;
    case PBL_INPUT_KEY_UP:
      return 1;
    case PBL_INPUT_KEY_SELECT:
      return 2;
    case PBL_INPUT_KEY_DOWN:
      return 3;
    default:
      return -1;
  }
}

static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  int index;
  uint32_t reg, updated;

  if (evt->type != PBL_INPUT_EV_KEY || evt->value != 0) {
    return;
  }

  index = prv_button_index(evt->code);
  if (index < 0) {
    return;
  }

  pbl_irq_lock();
  reg = RTC_ReadBackupRegister(STUCK_BUTTON_REGISTER);
  updated = reg & ~(0xffU << (index * 8));
  if (reg != updated) {
    RTC_WriteBackupRegister(STUCK_BUTTON_REGISTER, updated);
  }
  pbl_irq_unlock();
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, nullptr);
