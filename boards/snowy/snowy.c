/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

#include "snowy_devices.h"

#include <assert.h>

#include <pbl/board.h>
#include <pbl/board/partitions.h>
#include <pbl/drivers/audio/stm32.h>
#include <pbl/drivers/backlight/pwm.h>
#include <pbl/drivers/dma/stm32.h>
#include <pbl/drivers/i2c/stm32.h>
#include <pbl/drivers/input/stm32_keys.h>
#include <pbl/drivers/pmic/max14690.h>
#include <pbl/drivers/pwm/stm32.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

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

// Debug UART
///////////////////////////////////////////////////////////////////////////////

static UARTDeviceState s_dbg_uart_state;
static UARTDevice DBG_UART_DEVICE = {
  .state = &s_dbg_uart_state,
  .periph = USART3,
  .rcc_apb_periph = RCC_APB1Periph_USART3,
  .tx = PBL_GPIO(STM32_GPIOC, 10, 0),
  .rx = PBL_GPIO(STM32_GPIOC, 11, 0),
  .af = GPIO_AF_USART3,
  .irqn = PBL_IRQN(USART3),
};
PBL_IRQ_CONNECT(USART3, 13, uart_irq_handler, &DBG_UART_DEVICE, 0);
PBL_UART_STM32_DEFINE(snowy_console_uart, "usart3", 115200, &DBG_UART_DEVICE);

#ifdef CONFIG_QEMU_SERIAL
// The emulator's Pebble protocol and control channel, on USART2 as in the
// stock firmware's emulator build. No pins: QEMU connects it directly.
///////////////////////////////////////////////////////////////////////////////

static UARTDeviceState s_qemu_uart_state;
static UARTDevice QEMU_UART_DEVICE = {
  .state = &s_qemu_uart_state,
  .periph = USART2,
  .rcc_apb_periph = RCC_APB1Periph_USART2,
  .irqn = PBL_IRQN(USART2),
};
PBL_IRQ_CONNECT(USART2, 13, uart_irq_handler, &QEMU_UART_DEVICE, 0);
PBL_UART_STM32_DEFINE(snowy_qemu_uart, "usart2", 230400, &QEMU_UART_DEVICE);
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
const struct pbl_uart_stm32 snowy_bt_hci_port = {
  .uart = PBL_UART_INIT(snowy_bt_hci_port, "usart1", &pbl_uart_stm32_ops, 115200, nullptr),
  .dev = &BT_HCI_UART_DEVICE,
};
#endif

// Display: SPI6_TX on DMA2 stream 5, channel 1
///////////////////////////////////////////////////////////////////////////////

static struct pbl_dma_stm32_state s_ice40lp_tx_dma_state;
const struct pbl_dma_stm32_stream ICE40LP_TX_DMA = {
  .state = &s_ice40lp_tx_dma_state,
  .stream = DMA2_Stream5,
  .channel = DMA_Channel_1,
  .rcc = RCC_AHB1Periph_DMA2,
  .it_ht = DMA_IT_HTIF5,
  .it_tc = DMA_IT_TCIF5,
  .irqn = PBL_IRQN(DMA2_Stream5),
};

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
// Same priority as the EXTI handlers, so the DMA handler cannot preempt the
// BUSY one.
PBL_IRQ_CONNECT(DMA2_Stream5, 14, pbl_dma_stm32_irq_handler, &ICE40LP_TX_DMA, 0);

// I2C1: PMIC and compass
///////////////////////////////////////////////////////////////////////////////

PBL_I2C_STM32_DEFINE(s_i2c1, "i2c1", I2C1, RCC_APB1Periph_I2C1, 400000, PBL_GPIO(STM32_GPIOB, 6, 0),
                     GPIO_AF_I2C1, PBL_GPIO(STM32_GPIOB, 9, 0), GPIO_AF_I2C1, nullptr);
PBL_IRQ_CONNECT(I2C1_EV, 12, pbl_i2c_stm32_ev_irq_handler, &s_i2c1, 0);
PBL_IRQ_CONNECT(I2C1_ER, 12, pbl_i2c_stm32_er_irq_handler, &s_i2c1, 0);

static const struct pbl_i2c_dev I2C_SLAVE_MAX14690 = PBL_I2C_DEV(&s_i2c1.bus, 0x28);

// Buttons: active low with pull-ups; Back has an external one
///////////////////////////////////////////////////////////////////////////////

PBL_STM32_KEYS_DEFINE(
    BUTTONS, "buttons",
    PBL_GPIO_KEYS_KEY("Back", PBL_GPIO(STM32_GPIOG, 4, PBL_GPIO_ACTIVE_LOW), PBL_INPUT_KEY_BACK,
                      40),
    PBL_GPIO_KEYS_KEY("Up", PBL_GPIO(STM32_GPIOG, 3, PBL_GPIO_ACTIVE_LOW | PBL_GPIO_PULL_UP),
                      PBL_INPUT_KEY_UP, 40),
    PBL_GPIO_KEYS_KEY("Select", PBL_GPIO(STM32_GPIOG, 1, PBL_GPIO_ACTIVE_LOW | PBL_GPIO_PULL_UP),
                      PBL_INPUT_KEY_SELECT, 40),
    PBL_GPIO_KEYS_KEY("Down", PBL_GPIO(STM32_GPIOG, 2, PBL_GPIO_ACTIVE_LOW | PBL_GPIO_PULL_UP),
                      PBL_INPUT_KEY_DOWN, 40));

// Compass: MAG3110, data ready active high on PF14
PBL_MAG3110_DEFINE(snowy_mag, &s_i2c1.bus, 0x0e, .int_gpio = PBL_GPIO(STM32_GPIOF, 14, 0),
#ifdef CONFIG_BOARD_SPALDING
                   .axis_map = {0, 1, 2}, .axis_dir = {1, -1, 1});
#else
                   .axis_map = {1, 0, 2}, .axis_dir = {-1, -1, 1});
#endif

// PMIC
///////////////////////////////////////////////////////////////////////////////

// MON output on ADC channel 1 (PA1)
PBL_MAX14690_DEFINE(s_max14690, &s_i2c1.bus, &I2C_SLAVE_MAX14690,
                    .int_gpio = PBL_GPIO(STM32_GPIOG, 7, 0), .mon_adc_channel = ADC_Channel_1,
                    .mon_adc_pin = PBL_GPIO(STM32_GPIOA, 1, 0));
const struct pbl_max14690 *const MAX14690 = &s_max14690;

// Backlight: TIM12 CH1
///////////////////////////////////////////////////////////////////////////////

PBL_PWM_STM32_DEFINE(s_tim12, "tim12", TIM12, RCC_APB1Periph_TIM12, false, GPIO_AF_TIM12,
                     PBL_GPIO(STM32_GPIOB, 14, 0));

PBL_BACKLIGHT_PWM_DEFINE(snowy_backlight, "backlight", PBL_GPIO(nullptr, 0, 0),
                         PBL_PWM(&s_tim12.ctrl, 0, PBL_PWM_HZ(256), 0));

// Vibe: H-bridge driven by TIM10 CH1, enabled by PF4
///////////////////////////////////////////////////////////////////////////////

PBL_PWM_STM32_DEFINE(s_tim10, "tim10", TIM10, RCC_APB2Periph_TIM10, true, GPIO_AF_TIM10,
                     PBL_GPIO(STM32_GPIOB, 8, 0));

PBL_VIBE_PWM_DEFINE(snowy_vibe, true, .pwm = PBL_PWM(&s_tim10.ctrl, 0, PBL_PWM_HZ(22000), 0),
                    .ctl = PBL_GPIO(STM32_GPIOF, 4, 0));

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

// Accelerometer: BMI160 on SPI, see BOARD_CONFIG_ACCEL
///////////////////////////////////////////////////////////////////////////////

PBL_BMI160_DEFINE(snowy_accel);

// Microphone: I2S on SPI2
///////////////////////////////////////////////////////////////////////////////

// SPI2_RX: DMA1 stream 3, channel 0
static struct pbl_dma_stm32_state s_mic_dma_state;
static const struct pbl_dma_stm32_stream MIC_DMA = {
  .state = &s_mic_dma_state,
  .stream = DMA1_Stream3,
  .channel = DMA_Channel_0,
  .rcc = RCC_AHB1Periph_DMA1,
  .it_ht = DMA_IT_HTIF3,
  .it_tc = DMA_IT_TCIF3,
  .irqn = PBL_IRQN(DMA1_Stream3),
};
PBL_IRQ_CONNECT(DMA1_Stream3, 15, pbl_dma_stm32_irq_handler, &MIC_DMA, 0);

PBL_AUDIO_STM32_I2S_PDM_DEFINE(snowy_mic_dai, "i2s2", SPI2, RCC_APB1Periph_SPI2, &MIC_DMA,
                               PBL_GPIO(STM32_GPIOB, 10, 0), PBL_GPIO(STM32_GPIOB, 15, 0),
                               GPIO_AF_SPI2, 250);
PBL_AUDIO_CARD_DEFINE(snowy_mic, "mic", &snowy_mic_dai, nullptr, nullptr, 0, 0);

void board_early_init(void) {
}

void board_init(void) {
}
