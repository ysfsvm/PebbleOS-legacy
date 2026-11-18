/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

/* Pin states written by hand in the generator's form: USART1 TX on PA9 and RX on PA10 (AF7),
 * I2C1 SDA on PG13 (AF4, open drain), PA3 (AF5, pull-down, speed 1) and PA2 (AF1, speed 3) in the
 * low alternate function register, an analog PA4, a pin of port K, which the tree lacks, a GPIO
 * PA0, a reserved PA5 before PA6 (AF2), and cells with bits outside the pin, mode and options */
PBL_PINCTRL_STM32_DT_STATE(dt_usart1_pins, 0x908 | PBL_PINCTRL_STM32_SLEW(2),
                           0xa08 | PBL_PINCTRL_STM32_PULL_UP | PBL_PINCTRL_STM32_SLEW(2));
PBL_PINCTRL_STM32_DT_STATE(dt_i2c_pins,
                           0x6d05 | PBL_PINCTRL_STM32_OPEN_DRAIN | PBL_PINCTRL_STM32_BIAS_DISABLE);
PBL_PINCTRL_STM32_DT_STATE(dt_low_pins,
                           0x306 | PBL_PINCTRL_STM32_PULL_DOWN | PBL_PINCTRL_STM32_SLEW(1),
                           0x202 | PBL_PINCTRL_STM32_SLEW(3));
PBL_PINCTRL_STM32_DT_STATE(dt_adc_pins, 0x411);
PBL_PINCTRL_STM32_DT_STATE(dt_port_k_pins, 0xa008);
PBL_PINCTRL_STM32_DT_STATE(dt_gpio_pins, 0x0);
PBL_PINCTRL_STM32_DT_STATE(dt_rsvd_pins, 0x512, 0x603);
PBL_PINCTRL_STM32_DT_STATE(dt_bit19_pins, 0x908 | (1U << 19));
PBL_PINCTRL_STM32_DT_STATE(dt_bit24_pins, 0x908 | (1U << 24));
PBL_PINCTRL_STM32_DT_STATE(dt_mode19_pins, 0x913);
