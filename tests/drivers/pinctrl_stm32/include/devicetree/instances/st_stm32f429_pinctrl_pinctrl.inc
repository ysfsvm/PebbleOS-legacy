/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

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
PBL_PINCTRL_STM32_DT_STATE(dt_out_low_pins, 0x100 | PBL_PINCTRL_STM32_OUTPUT_LOW |
                                                PBL_PINCTRL_STM32_PULL_DOWN |
                                                PBL_PINCTRL_STM32_SLEW(1));
PBL_PINCTRL_STM32_DT_STATE(dt_out_high_pins,
                           0x6d00 | PBL_PINCTRL_STM32_OUTPUT_HIGH | PBL_PINCTRL_STM32_OPEN_DRAIN);
PBL_PINCTRL_STM32_DT_STATE(dt_out_both_pins,
                           0x100 | PBL_PINCTRL_STM32_OUTPUT_LOW | PBL_PINCTRL_STM32_OUTPUT_HIGH);
PBL_PINCTRL_STM32_DT_STATE(dt_af_level_pins, 0x908 | PBL_PINCTRL_STM32_OUTPUT_LOW);
