/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <devicetree/devices.h>

PBL_I2C_STM32_DEFINE(dt_i2c1, "i2c1", 0x40005400, 31, 32,
                     PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, STM32F4_APB1_CLOCK(I2C1))),
                     PBL_RESET(&dt_rcc_reset, STM32F4_APB1_RESET(I2C1)), 400000, &dt_i2c1_default,
                     PBL_GPIO(&dt_gpiob, 6, PBL_GPIO_DT_FLAGS(0)),
                     PBL_GPIO(&dt_gpiob, 9, PBL_GPIO_DT_FLAGS(0)));
PBL_DEVICE_DEPENDS(dt_i2c1, &dt_i2c1.dev, &dt_rcc.dev, &dt_rcc_reset.dev, &dt_gpiob.dev);
