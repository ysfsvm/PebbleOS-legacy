/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

PBL_PINCTRL_STM32_DT_STATE(dt_spi1_default,
                           0x506 | PBL_PINCTRL_STM32_BIAS_DISABLE | PBL_PINCTRL_STM32_PUSH_PULL |
                               PBL_PINCTRL_STM32_SLEW(2),
                           0x606 | PBL_PINCTRL_STM32_BIAS_DISABLE | PBL_PINCTRL_STM32_PUSH_PULL |
                               PBL_PINCTRL_STM32_SLEW(2),
                           0x706 | PBL_PINCTRL_STM32_BIAS_DISABLE | PBL_PINCTRL_STM32_PUSH_PULL |
                               PBL_PINCTRL_STM32_SLEW(2));
