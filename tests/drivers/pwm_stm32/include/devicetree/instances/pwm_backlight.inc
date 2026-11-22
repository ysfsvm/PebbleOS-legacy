/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/pwm.h>

#include <devicetree/devices.h>

PBL_BACKLIGHT_PWM_DEFINE(dt_backlight, "backlight", PBL_GPIO(nullptr, 0, 0),
                         PBL_PWM(&dt_pwm12, 0, 3906250, 0));
PBL_DEVICE_DEPENDS(dt_backlight, &dt_backlight.dev, &dt_pwm12.dev);
