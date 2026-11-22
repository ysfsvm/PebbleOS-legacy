/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// The devices of snowy.c that roles.yml gives the board's roles, shared with
// spalding.

#include <pbl/drivers/als/stm32_adc.h>
#include <pbl/drivers/audio.h>
#include <pbl/drivers/display/ice40lp.h>
#include <pbl/drivers/imu/mag3110/mag3110.h>
#include <pbl/drivers/uart.h>
#include <pbl/drivers/vibe/pwm.h>

#ifdef CONFIG_QEMU_SERIAL
extern const struct pbl_uart snowy_qemu_uart;
#define SNOWY_QEMU_UART (&snowy_qemu_uart)
#else
// The emulator's port exists in emulator builds only.
#define SNOWY_QEMU_UART nullptr
#endif

#ifdef CONFIG_BT_HCI_UART
// A port over the HCI USART that is never registered, so never initialised:
// the legacy transport drives the USART itself (BOARD_BT_HCI_UART_DEV).
extern const struct pbl_uart snowy_bt_hci_port;
#endif

extern const struct pbl_display_ice40lp snowy_display;
extern const struct pbl_mag3110 snowy_mag;
extern const struct pbl_vibe_pwm snowy_vibe;
extern const struct pbl_als_stm32_adc snowy_als;
extern const struct pbl_audio snowy_mic;
