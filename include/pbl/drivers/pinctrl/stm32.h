/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/pinctrl.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_pinctrl_stm32 STM32 pin controller
 * @ingroup drivers_pinctrl
 * @brief @ref drivers_pinctrl controller of the STM32F4 GPIO ports.
 *
 * A pin is a pinmux cell of the upstream binding, STM32_PINMUX(port, line, mode) from
 * dt-bindings/pinctrl/stm32-pinfunc.h, or'ed with its options. A GPIO pin is the GPIO driver's
 * and fails with @c -EINVAL, a reserved (RSVD) pin is left as it is. Every option is written when
 * the pin is configured, so a pin without any gets no pull, push-pull output and the lowest speed;
 * Linux leaves the absent ones as they were instead:
 *
 * @code{.c}
 * #include <dt-bindings/pinctrl/stm32-pinfunc.h>
 *
 * PBL_PINCTRL_STATE_DEFINE(s_usart1_default, STM32_PINCTRL,
 *                          STM32_PINMUX('A', 9, AF7) | PBL_PINCTRL_STM32_SLEW(2),
 *                          STM32_PINMUX('A', 10, AF7) | PBL_PINCTRL_STM32_PULL_UP);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_PINCTRL_STM32_MODE_MSK PBL_GENMASK(7, 0)
#define PBL_PINCTRL_STM32_PIN_MSK  PBL_GENMASK(15, 8)
#define PBL_PINCTRL_STM32_SLEW_MSK PBL_GENMASK(21, 20)
/** @endcond */

/** No pull, as bias-disable. */
#define PBL_PINCTRL_STM32_BIAS_DISABLE 0U
/** Pull-up. */
#define PBL_PINCTRL_STM32_PULL_UP PBL_BIT(16)
/** Pull-down. */
#define PBL_PINCTRL_STM32_PULL_DOWN PBL_BIT(17)
/** Push-pull output, as drive-push-pull. */
#define PBL_PINCTRL_STM32_PUSH_PULL 0U
/** Open-drain output. */
#define PBL_PINCTRL_STM32_OPEN_DRAIN PBL_BIT(18)
/** Output speed, the OSPEEDR value: 0 (low) to 3 (very high). */
#define PBL_PINCTRL_STM32_SLEW(rate) PBL_FIELD_PREP(PBL_PINCTRL_STM32_SLEW_MSK, rate)

/** The pin controller. */
#define STM32_PINCTRL (&pbl_pinctrl_stm32)

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pinctrl_controller pbl_pinctrl_stm32;
/** @endcond */

/** @} */
