/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

PBL_PINCTRL_STM32_DT_STATE(dt_i2c1_default,
                           0x1605 | PBL_PINCTRL_STM32_BIAS_DISABLE | PBL_PINCTRL_STM32_OPEN_DRAIN |
                               PBL_PINCTRL_STM32_SLEW(2),
                           0x1905 | PBL_PINCTRL_STM32_BIAS_DISABLE | PBL_PINCTRL_STM32_OPEN_DRAIN |
                               PBL_PINCTRL_STM32_SLEW(2));
