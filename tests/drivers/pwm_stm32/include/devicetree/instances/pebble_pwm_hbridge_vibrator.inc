/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pwm.h>

#include <devicetree/devices.h>

PBL_VIBE_PWM_HBRIDGE_DEFINE(dt_vibrator, PBL_PWM(&dt_pwm10, 0, 45454, 0),
                            PBL_GPIO(&dt_gpiof, 4, PBL_GPIO_DT_FLAGS(0)));
PBL_DEVICE_DEPENDS(dt_vibrator, &dt_vibrator.dev, &dt_gpiof.dev);
