/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

PBL_PINCTRL_STM32_DT_STATE(dt_pwm10_default, 0x1804 | PBL_PINCTRL_STM32_PULL_DOWN |
                                                 PBL_PINCTRL_STM32_PUSH_PULL |
                                                 PBL_PINCTRL_STM32_SLEW(3));
PBL_PINCTRL_STM32_DT_STATE(dt_pwm10_sleep,
                           0x1800 | PBL_PINCTRL_STM32_BIAS_DISABLE | PBL_PINCTRL_STM32_PUSH_PULL |
                               PBL_PINCTRL_STM32_OUTPUT_LOW | PBL_PINCTRL_STM32_SLEW(0));
PBL_PINCTRL_STM32_DT_STATE(dt_pwm12_default, 0x1e0a | PBL_PINCTRL_STM32_PULL_DOWN |
                                                 PBL_PINCTRL_STM32_PUSH_PULL |
                                                 PBL_PINCTRL_STM32_SLEW(3));
PBL_PINCTRL_STM32_DT_STATE(dt_pwm12_sleep,
                           0x1e00 | PBL_PINCTRL_STM32_BIAS_DISABLE | PBL_PINCTRL_STM32_PUSH_PULL |
                               PBL_PINCTRL_STM32_OUTPUT_LOW | PBL_PINCTRL_STM32_SLEW(0));
