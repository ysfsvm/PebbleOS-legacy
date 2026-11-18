/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/gpio.h>

#include <devicetree/devices.h>

/**
 * @defgroup drivers_gpio_stm32 STM32 GPIO
 * @ingroup drivers_gpio
 * @brief @ref drivers_gpio ports of the STM32F4 SoCs, the gpio@ banks of the pin controller in
 * the tree.
 * @{
 */

/** @brief STM32 GPIO port. */
struct pbl_gpio_stm32 {
  /** GPIO port. */
  struct pbl_gpio_port port;
  /** Register base address. */
  uintptr_t base;
  /** Port clock. */
  struct pbl_clk clk;
  /** Port index (A = 0), also its EXTI port source. */
  uint8_t index;
};

#define STM32_GPIOA (&dt_gpioa)
#define STM32_GPIOB (&dt_gpiob)
#define STM32_GPIOC (&dt_gpioc)
#define STM32_GPIOD (&dt_gpiod)
#define STM32_GPIOE (&dt_gpioe)
#define STM32_GPIOF (&dt_gpiof)
#define STM32_GPIOG (&dt_gpiog)
#define STM32_GPIOH (&dt_gpioh)

/**
 * @brief Hand a pin to a peripheral.
 *
 * Initialises the pin's port first.
 *
 * @param gpio Pin on an STM32 port.
 * @param af Alternate function number, 0 to 15.
 * @param flags @c PBL_GPIO_PULL_UP, @c PBL_GPIO_PULL_DOWN and/or @c PBL_GPIO_OPEN_DRAIN.
 * @param speed Output speed, 0 (low) to 3 (very high).
 */
void pbl_gpio_stm32_af(const struct pbl_gpio *gpio, uint8_t af, uint32_t flags, uint32_t speed);

/**
 * @brief Find a port.
 *
 * @param index Port index, A = 0.
 * @return The port, or NULL if the tree has none with that index.
 */
const struct pbl_gpio_port *pbl_gpio_stm32_port(uint8_t index);

/**
 * @brief Put a pin in analog mode (lowest power, or for the ADC).
 *
 * @param gpio Pin on an STM32 port.
 */
void pbl_gpio_stm32_analog(const struct pbl_gpio *gpio);

/** @} */
