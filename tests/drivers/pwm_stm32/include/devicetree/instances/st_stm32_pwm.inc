/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

/* TIM10 and TIM12 of the snowy tree, as the generator writes them */

#include <devicetree/devices.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/stm32f4.h>

PBL_PWM_STM32_DEFINE(dt_pwm10, "pwm10", 0x40014400, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 177)),
                     &dt_pwm10_default, &dt_pwm10_sleep);
PBL_DEVICE_DEPENDS(dt_pwm10, &dt_pwm10.dev, &dt_rcc.dev);
PBL_PWM_STM32_DEFINE(dt_pwm12, "pwm12", 0x40001800, PBL_CLK(&dt_rcc, STM32F4_CLK_ID(0, 134)),
                     &dt_pwm12_default, &dt_pwm12_sleep);
PBL_DEVICE_DEPENDS(dt_pwm12, &dt_pwm12.dev, &dt_rcc.dev);
