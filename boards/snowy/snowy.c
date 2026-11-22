/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

#include "snowy_devices.h"

#include <assert.h>

#include <pbl/board.h>
#include <pbl/board/partitions.h>
#include <pbl/drivers/audio/stm32.h>
#include <pbl/drivers/clk/stm32f4.h>
#include <pbl/drivers/dma/stm32f4.h>
#include <pbl/drivers/pmic/max14690.h>
#include <pbl/drivers/uart/stm32.h>
#include <pbl/drivers/uart/stm32_legacy.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <devicetree/devices.h>
#include <dt-bindings/mfd/stm32f4-rcc.h>

// The stock bootloader and recovery firmware fix these partitions.
static_assert(PART_SAFE_FIRMWARE_OFFSET == 0x200000 && PART_SAFE_FIRMWARE_SIZE == PBL_MIB(1),
              "PRF moved");
static_assert(PART_STOCK_SHARED_PRF_STORAGE_OFFSET == 0x0c0000, "stock shared PRF storage moved");
static_assert(PART_MFG_INFO_OFFSET == 0x0e0000 && PART_MFG_INFO_SIZE == PBL_KIB(128),
              "MFG info moved");
static_assert(PART_FIRMWARE_SLOT_1_OFFSET == 0x100000, "firmware update slot moved");
static_assert(PART_SYSTEM_RESOURCES_BANK_0_OFFSET == 0x300000, "resources moved");
static_assert(PART_FILESYSTEM_OFFSET + PART_FILESYSTEM_SIZE == PBL_MIB(16),
              "snowy flash must be 16 MiB");

#ifdef CONFIG_QEMU_SERIAL
// The emulator's Pebble protocol and control channel, on USART2 as in the
// stock firmware's emulator build. No pins: QEMU connects it directly.
///////////////////////////////////////////////////////////////////////////////

PBL_UART_STM32_DEFINE(snowy_qemu_uart, "usart2", USART2_BASE, PBL_SOC_IRQN_USART2,
                      PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB1_CLOCK(UART2))), 230400,
                      false);
#endif

#ifdef CONFIG_BT_HCI_UART
// Bluetooth HCI
///////////////////////////////////////////////////////////////////////////////

// Hardware flow control holds the controller off while a byte is unread, so
// RX can stay interrupt driven.
static UARTDeviceState s_bt_hci_uart_state;
static UARTDevice BT_HCI_UART_DEVICE = {
  .state = &s_bt_hci_uart_state,
  .periph = USART1,
  .rcc_apb_periph = RCC_APB2Periph_USART1,
  .apb2 = true,
  .tx = PBL_GPIO(STM32_GPIOA, 9, 0),
  .rx = PBL_GPIO(STM32_GPIOA, 10, 0),
  .cts = PBL_GPIO(STM32_GPIOA, 11, 0),
  .rts = PBL_GPIO(STM32_GPIOA, 12, 0),
  .af = GPIO_AF_USART1,
  .irqn = PBL_IRQN(USART1),
};
UARTDevice *const BOARD_BT_HCI_UART_DEV = &BT_HCI_UART_DEVICE;
PBL_IRQ_CONNECT(USART1, 12, uart_irq_handler, &BT_HCI_UART_DEVICE, 0);

// The BT_HCI_UART role's port: defined, never registered.
PBL_UART_STATE_DEFINE(snowy_bt_hci_port);
const struct pbl_uart snowy_bt_hci_port =
    PBL_UART_INIT(snowy_bt_hci_port, "usart1", nullptr, 115200, nullptr);
#endif

// Display: iCE40LP FPGA on SPI6, see BOARD_CONFIG_DISPLAY
///////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_BOARD_SPALDING
// Pixels masked at each end of the top half's rows, from the stock spalding framebuffer layout,
// which is also the one of legacy round apps (g_gbitmap_legacy_3x_data_row_infos).
static const uint8_t s_round_mask[] = {
  76, 71, 66, 63, 60, 57, 55, 52, 50, 48, 46, 45, 43, 41, 40, 38, 37, 36, 34, 33, 32, 31, 29,
  28, 27, 26, 25, 24, 23, 22, 22, 21, 20, 19, 18, 18, 17, 16, 15, 15, 14, 13, 13, 12, 12, 11,
  10, 10, 9,  9,  8,  8,  7,  7,  7,  6,  6,  5,  5,  5,  4,  4,  4,  3,  3,  3,  2,  2,  2,
  2,  2,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};
PBL_DISPLAY_ICE40LP_DEFINE(snowy_display, "display", CONFIG_PLATFORM_DISPLAY_WIDTH,
                           CONFIG_PLATFORM_DISPLAY_HEIGHT, .round_mask = s_round_mask);
#else
PBL_DISPLAY_ICE40LP_DEFINE(snowy_display, "display", CONFIG_PLATFORM_DISPLAY_WIDTH,
                           CONFIG_PLATFORM_DISPLAY_HEIGHT);
#endif

// I2C1: PMIC and compass
///////////////////////////////////////////////////////////////////////////////

static const struct pbl_i2c_dev I2C_SLAVE_MAX14690 = PBL_I2C_DEV(&dt_i2c1, 0x28);

// Compass: MAG3110, data ready active high on PF14
PBL_MAG3110_DEFINE(snowy_mag, &dt_i2c1, 0x0e, .int_gpio = PBL_GPIO(STM32_GPIOF, 14, 0),
#ifdef CONFIG_BOARD_SPALDING
                   .axis_map = {0, 1, 2}, .axis_dir = {1, -1, 1});
#else
                   .axis_map = {1, 0, 2}, .axis_dir = {-1, -1, 1});
#endif

// PMIC
///////////////////////////////////////////////////////////////////////////////

// MON output on ADC channel 1 (PA1)
PBL_MAX14690_DEFINE(s_max14690, &dt_i2c1, &I2C_SLAVE_MAX14690,
                    .int_gpio = PBL_GPIO(STM32_GPIOG, 7, 0), .mon_adc_channel = ADC_Channel_1,
                    .mon_adc_pin = PBL_GPIO(STM32_GPIOA, 1, 0));
const struct pbl_max14690 *const MAX14690 = &s_max14690;

// Ambient light: phototransistor on ADC1 channel 2, powered by PA3
///////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_BOARD_SPALDING
// Light sensor readings (3 s after the backlight went off) on a Pebble Time
// Round in seven lighting conditions, against the middle of each condition's
// usual lux range: the phototransistor jumps from the dark floor in the
// faintest light, then rises slowly up to sunlight.
static const struct pbl_als_lux_point s_als_lux_table[] = {
  {236, 0},      // closet, near pitch dark
  {2861, 2},     // dark room, light from a doorway
  {2995, 10},    // dim room, one small lamp
  {3589, 175},   // normal indoor light
  {3636, 550},   // bright room, desk lamp
  {3822, 3000},  // outdoors in shade
  {4006, 10000}, // direct sunlight
};
#else
// Light sensor readings (3 s after the backlight went off) on a Pebble Time
// in eight lighting conditions, against the middle of each condition's usual
// lux range: the phototransistor rises steeply up to room light, then
// flattens out. Not measured on a Steel.
static const struct pbl_als_lux_point s_als_lux_table[] = {
  {201, 0},      // closet, near pitch dark
  {216, 2},      // dark room, light from a doorway
  {403, 10},     // dim room, one small lamp
  {1600, 45},    // evening living room
  {2603, 175},   // normal indoor light
  {2854, 550},   // bright room, desk lamp
  {2957, 3000},  // outdoors in shade
  {3030, 10000}, // direct sunlight
};
#endif

static const struct pbl_als_scale s_als_scale = {
  .table = s_als_lux_table,
  .table_len = ARRAY_LENGTH(s_als_lux_table),
};

PBL_ALS_STM32_ADC_DEFINE(snowy_als, &s_als_scale, .enable = PBL_GPIO(STM32_GPIOA, 3, 0),
                         .channel = ADC_Channel_2, .pin = PBL_GPIO(STM32_GPIOA, 2, 0));

// Microphone: I2S on SPI2
///////////////////////////////////////////////////////////////////////////////

// SPI2_RX: DMA1 stream 3, channel 0
static const struct pbl_dma_chan s_mic_dma =
    PBL_DMA_CHAN(&dt_dma1, 3, 0, PBL_DMA_STM32F4_PRIORITY(PBL_DMA_STM32F4_PRIORITY_HIGH));

PBL_AUDIO_STM32_I2S_PDM_DEFINE(snowy_mic_dai, "i2s2", SPI2, RCC_APB1Periph_SPI2, &s_mic_dma,
                               PBL_GPIO(STM32_GPIOB, 10, 0), PBL_GPIO(STM32_GPIOB, 15, 0),
                               GPIO_AF_SPI2, 250);
PBL_AUDIO_CARD_DEFINE(snowy_mic, "mic", &snowy_mic_dai, nullptr, nullptr, 0, 0);
