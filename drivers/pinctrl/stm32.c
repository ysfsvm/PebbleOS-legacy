/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/drivers/pinctrl/stm32.h>
#include <pbl/util/bits.h>

#include <devicetree/devices.h>

// Pin modes of the pinmux cells: GPIO, then AF0-AF15, analog and reserved
#define MODE_AF0    1U
#define MODE_AF15   16U
#define MODE_ANALOG 17U
#define MODE_RSVD   18U

// Bits of a cell: the pin, its mode and its options
#define CELL_MSK                                                                        \
  (PBL_PINCTRL_STM32_MODE_MSK | PBL_PINCTRL_STM32_PIN_MSK | PBL_PINCTRL_STM32_PULL_UP | \
   PBL_PINCTRL_STM32_PULL_DOWN | PBL_PINCTRL_STM32_OPEN_DRAIN | PBL_PINCTRL_STM32_SLEW_MSK)

// Ports A to K, as the binding numbers pins
#define NUM_PADS (11U * 16U)

struct pbl_pinctrl_stm32 {
  struct pbl_pinctrl_controller ctrl;
};

static int prv_configure_pin(uint32_t pin) {
  uint32_t mode = PBL_FIELD_GET(PBL_PINCTRL_STM32_MODE_MSK, pin);
  uint32_t num = PBL_FIELD_GET(PBL_PINCTRL_STM32_PIN_MSK, pin);
  const struct pbl_gpio_port *port;
  uint32_t flags = 0U;

  if ((pin & ~CELL_MSK) != 0U) {
    return -EINVAL;
  }

  // A reserved pin is not muxed
  if (mode == MODE_RSVD) {
    return 0;
  }

  // A GPIO pin is the GPIO driver's to set up
  if (mode < MODE_AF0 || mode > MODE_ANALOG) {
    return -EINVAL;
  }

  port = pbl_gpio_stm32_port(num / 16U);
  if (port == nullptr || pbl_device_init(&port->dev) != 0) {
    return -ENODEV;
  }

  const struct pbl_gpio gpio = PBL_GPIO(port, num % 16U, 0);

  if (mode == MODE_ANALOG) {
    pbl_gpio_stm32_analog(&gpio);
    return 0;
  }

  if ((pin & PBL_PINCTRL_STM32_PULL_UP) != 0U) {
    flags |= PBL_GPIO_PULL_UP;
  }
  if ((pin & PBL_PINCTRL_STM32_PULL_DOWN) != 0U) {
    flags |= PBL_GPIO_PULL_DOWN;
  }
  if ((pin & PBL_PINCTRL_STM32_OPEN_DRAIN) != 0U) {
    flags |= PBL_GPIO_OPEN_DRAIN;
  }

  pbl_gpio_stm32_af(&gpio, mode - MODE_AF0, flags, PBL_FIELD_GET(PBL_PINCTRL_STM32_SLEW_MSK, pin));

  return 0;
}

static int prv_configure(const struct pbl_pinctrl_controller *ctrl, const uint32_t *pins,
                         uint8_t num_pins, uintptr_t reg) {
  (void)ctrl;
  (void)reg;

  for (uint8_t i = 0U; i < num_pins; i++) {
    int ret = prv_configure_pin(pins[i]);

    if (ret < 0) {
      return ret;
    }
  }

  return 0;
}

static const struct pbl_pinctrl_controller_ops s_ops = {
  .configure = prv_configure,
};

#define PBL_PINCTRL_STM32_DEFINE(sym, _name)                                                \
  PBL_DEVICE_STATE_DEFINE(sym);                                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_pinctrl_stm32, struct pbl_pinctrl_controller, ctrl) { \
    .ctrl = PBL_PINCTRL_CONTROLLER_INIT(sym, _name, &s_ops, NUM_PADS),                      \
  }

#define PBL_PINCTRL_STM32_DT_STATE(sym, ...)          \
  static const uint32_t sym##_pins[] = {__VA_ARGS__}; \
  const struct pbl_pinctrl_state sym = {              \
    .ctrl = STM32_PINCTRL,                            \
    .pins = sym##_pins,                               \
    .num_pins = ARRAY_LENGTH(sym##_pins),             \
  }

#include PBL_DT_INSTANCES(st_stm32f429_pinctrl_pinctrl_controller)
#include PBL_DT_INSTANCES(st_stm32f429_pinctrl_pinctrl)
