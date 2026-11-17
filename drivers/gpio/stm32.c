/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/stm32f4_gpio.h>

static const struct pbl_gpio_stm32 *prv_port(const struct pbl_gpio_port *port) {
  return container_of(port, const struct pbl_gpio_stm32, port);
}

static void prv_field2(const struct pbl_gpio_stm32 *port, uint32_t offset, uint8_t pin,
                       uint32_t value) {
  pbl_modifyl(STM32F4_GPIO_FIELD2(pin), PBL_FIELD_PREP(STM32F4_GPIO_FIELD2(pin), value),
              port->base + offset);
}

static uint32_t prv_pull(uint32_t flags) {
  if (flags & PBL_GPIO_PULL_UP) {
    return STM32F4_GPIO_PULL_UP;
  }

  if (flags & PBL_GPIO_PULL_DOWN) {
    return STM32F4_GPIO_PULL_DOWN;
  }

  return STM32F4_GPIO_PULL_NONE;
}

// The mode goes last, once the pin has its other settings
static void prv_init_pin(const struct pbl_gpio_stm32 *port, uint8_t pin, uint32_t mode,
                         uint32_t flags, uint32_t speed) {
  pbl_modifyl(PBL_BIT(pin), (flags & PBL_GPIO_OPEN_DRAIN) ? PBL_BIT(pin) : 0U,
              port->base + STM32F4_GPIO_OTYPER_OFFSET);
  prv_field2(port, STM32F4_GPIO_OSPEEDR_OFFSET, pin, speed);
  prv_field2(port, STM32F4_GPIO_PUPDR_OFFSET, pin, prv_pull(flags));
  prv_field2(port, STM32F4_GPIO_MODER_OFFSET, pin, mode);
}

static int prv_configure(const struct pbl_gpio_port *gpio_port, uint8_t pin, uint32_t flags) {
  const struct pbl_gpio_stm32 *port = prv_port(gpio_port);

  if (flags & PBL_GPIO_OUTPUT) {
    // The level first, so the pin starts driving it
    if (flags & PBL_GPIO_OUTPUT_INIT_HIGH) {
      pbl_writel(STM32F4_GPIO_BSRR_SET(pin), port->base + STM32F4_GPIO_BSRR_OFFSET);
    } else if (flags & PBL_GPIO_OUTPUT_INIT_LOW) {
      pbl_writel(STM32F4_GPIO_BSRR_RESET(pin), port->base + STM32F4_GPIO_BSRR_OFFSET);
    }

    prv_init_pin(port, pin, STM32F4_GPIO_MODE_OUTPUT, flags, 0U);
  } else if (flags & PBL_GPIO_INPUT) {
    prv_init_pin(port, pin, STM32F4_GPIO_MODE_INPUT, flags, 0U);
  } else {
    return -EINVAL;
  }

  return 0;
}

static int prv_get(const struct pbl_gpio_port *gpio_port, uint8_t pin) {
  return (pbl_readl(prv_port(gpio_port)->base + STM32F4_GPIO_IDR_OFFSET) & PBL_BIT(pin)) != 0U;
}

static int prv_set(const struct pbl_gpio_port *gpio_port, uint8_t pin, bool level) {
  pbl_writel(level ? STM32F4_GPIO_BSRR_SET(pin) : STM32F4_GPIO_BSRR_RESET(pin),
             prv_port(gpio_port)->base + STM32F4_GPIO_BSRR_OFFSET);

  return 0;
}

uint8_t pbl_gpio_stm32_index(const struct pbl_gpio_port *port) {
  return prv_port(port)->index;
}

void pbl_gpio_stm32_af(const struct pbl_gpio *gpio, uint8_t af, uint32_t flags, uint32_t speed) {
  const struct pbl_gpio_stm32 *port = prv_port(gpio->port);

  (void)pbl_device_init(&gpio->port->dev);
  pbl_modifyl(STM32F4_GPIO_FIELD4(gpio->pin), PBL_FIELD_PREP(STM32F4_GPIO_FIELD4(gpio->pin), af),
              port->base + STM32F4_GPIO_AFR_OFFSET(gpio->pin));
  prv_init_pin(port, gpio->pin, STM32F4_GPIO_MODE_AF, flags | gpio->flags, speed);
}

void pbl_gpio_stm32_analog(const struct pbl_gpio *gpio) {
  (void)pbl_device_init(&gpio->port->dev);
  prv_init_pin(prv_port(gpio->port), gpio->pin, STM32F4_GPIO_MODE_ANALOG, 0U, 0U);
}

static const struct pbl_gpio_port_ops s_ops = {
  .configure = prv_configure,
  .get = prv_get,
  .set = prv_set,
};

static int prv_init(const struct pbl_device *dev) {
  return pbl_clk_enable(&prv_port(container_of(dev, const struct pbl_gpio_port, dev))->clk);
}

// The ports are 1 KiB apart, port A first, in a block aligned to 16 of them
#define PORT_INDEX(_base) ((uint8_t)(((_base) / STM32F4_GPIO_PORT_SIZE) % 16U))

#define PBL_GPIO_STM32_DEFINE(sym, _name, _base, _clk)                              \
  PBL_DEVICE_STATE_DEFINE(sym);                                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_stm32, struct pbl_gpio_port, port) {     \
    .port = PBL_GPIO_PORT_INIT(sym, _name, prv_init, nullptr, &s_ops, 16, nullptr), \
    .base = (_base), .clk = _clk, .index = PORT_INDEX(_base),                       \
  }

#include PBL_DT_INSTANCES(st_stm32f429_pinctrl)
