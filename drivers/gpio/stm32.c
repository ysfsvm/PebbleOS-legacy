/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/drivers/intc.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/stm32f4_exti.h>
#include <hardware/stm32f4_gpio.h>
#include <hardware/stm32f4_syscfg.h>

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
    // Level before mode
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

void pbl_gpio_stm32_af(const struct pbl_gpio *gpio, uint8_t af, uint32_t flags, uint32_t speed) {
  const struct pbl_gpio_stm32 *port = prv_port(gpio->port);

  (void)pbl_device_init(&gpio->port->dev);
  pbl_modifyl(STM32F4_GPIO_FIELD4(gpio->pin), PBL_FIELD_PREP(STM32F4_GPIO_FIELD4(gpio->pin), af),
              port->base + STM32F4_GPIO_AFR_OFFSET(gpio->pin));
  prv_init_pin(port, gpio->pin, STM32F4_GPIO_MODE_AF, flags | gpio->flags, speed);
}

void pbl_gpio_stm32_output(const struct pbl_gpio *gpio, bool level, uint32_t flags,
                           uint32_t speed) {
  const struct pbl_gpio_stm32 *port = prv_port(gpio->port);

  (void)pbl_device_init(&gpio->port->dev);
  pbl_writel(level ? STM32F4_GPIO_BSRR_SET(gpio->pin) : STM32F4_GPIO_BSRR_RESET(gpio->pin),
             port->base + STM32F4_GPIO_BSRR_OFFSET);
  prv_init_pin(port, gpio->pin, STM32F4_GPIO_MODE_OUTPUT, flags | gpio->flags, speed);
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

const struct pbl_gpio_port *pbl_gpio_stm32_port(uint8_t index) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_gpio_port_class, dev) {
    const struct pbl_gpio_port *port = container_of(dev, const struct pbl_gpio_port, dev);

    if (port->ops == &s_ops && prv_port(port)->index == index) {
      return port;
    }
  }

  return nullptr;
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_gpio_stm32 *port = prv_port(container_of(dev, const struct pbl_gpio_port, dev));
  int ret;

  ret = pbl_clk_enable(&port->clk);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t i = 0U; i < port->num_hogs; i++) {
    uint32_t flags = port->hogs[i].flags;

    if ((flags & PBL_GPIO_ACTIVE_LOW) &&
        (flags & (PBL_GPIO_OUTPUT_INIT_LOW | PBL_GPIO_OUTPUT_INIT_HIGH))) {
      flags ^= PBL_GPIO_OUTPUT_INIT_LOW | PBL_GPIO_OUTPUT_INIT_HIGH;
    }

    ret = prv_configure(&port->port, port->hogs[i].pin, flags & ~PBL_GPIO_OUTPUT_INIT_LOGICAL);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

// Pin interrupts
////////////////////////////////////////////////////////////////////////////////

#define EXTI_IRQ_PRIO 14

struct pbl_gpio_stm32_intc {
  struct pbl_intc intc;
  const struct pbl_gpio_stm32 *port;
  uintptr_t exti;
  uintptr_t syscfg;
  struct pbl_clk syscfg_clk;
};

static struct pbl_intc_line s_exti_lines[STM32F4_EXTI_NUM_PIN_LINES];
// Any controller, for the handlers
static const struct pbl_gpio_stm32_intc *s_exti_intc;

static const struct pbl_gpio_stm32_intc *prv_intc(const struct pbl_intc *intc) {
  return container_of(intc, const struct pbl_gpio_stm32_intc, intc);
}

static pbl_irq_t prv_exti_irqn(uint32_t line) {
  if (line <= 4U) {
    return PBL_IRQN(EXTI0) + line;
  }
  if (line <= 9U) {
    return PBL_IRQN(EXTI9_5);
  }
  return PBL_IRQN(EXTI15_10);
}

static void prv_exti_bit(uintptr_t reg, uint32_t line, bool set) {
  uint32_t value = pbl_readl(reg);

  pbl_writel(set ? value | PBL_BIT(line) : value & ~PBL_BIT(line), reg);
}

static int prv_intc_configure(const struct pbl_intc *intc, uint32_t hwirq, uint32_t type) {
  const struct pbl_gpio_stm32_intc *pin_intc = prv_intc(intc);
  uintptr_t exticr = pin_intc->syscfg + STM32F4_SYSCFG_EXTICR_OFFSET(hwirq);

  if ((type & ~PBL_IRQ_TYPE_EDGE_BOTH) != 0U) {
    return -ENOTSUP;
  }

  prv_exti_bit(pin_intc->exti + STM32F4_EXTI_IMR_OFFSET, hwirq, false);
  pbl_writel((pbl_readl(exticr) & ~STM32F4_SYSCFG_EXTICR_FIELD(hwirq)) |
                 PBL_FIELD_PREP(STM32F4_SYSCFG_EXTICR_FIELD(hwirq), pin_intc->port->index),
             exticr);
  prv_exti_bit(pin_intc->exti + STM32F4_EXTI_RTSR_OFFSET, hwirq,
               (type & PBL_IRQ_TYPE_EDGE_RISING) != 0U);
  prv_exti_bit(pin_intc->exti + STM32F4_EXTI_FTSR_OFFSET, hwirq,
               (type & PBL_IRQ_TYPE_EDGE_FALLING) != 0U);
  pbl_writel(PBL_BIT(hwirq), pin_intc->exti + STM32F4_EXTI_PR_OFFSET);

  return 0;
}

static int prv_intc_enable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_stm32_intc *pin_intc = prv_intc(intc);

  pbl_writel(PBL_BIT(hwirq), pin_intc->exti + STM32F4_EXTI_PR_OFFSET);
  prv_exti_bit(pin_intc->exti + STM32F4_EXTI_IMR_OFFSET, hwirq, true);
  // Shared vector, stays enabled
  pbl_irq_enable(prv_exti_irqn(hwirq));

  return 0;
}

static int prv_intc_disable(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_gpio_stm32_intc *pin_intc = prv_intc(intc);

  prv_exti_bit(pin_intc->exti + STM32F4_EXTI_IMR_OFFSET, hwirq, false);
  pbl_writel(PBL_BIT(hwirq), pin_intc->exti + STM32F4_EXTI_PR_OFFSET);

  return 0;
}

static const struct pbl_intc_ops s_intc_ops = {
  .configure = prv_intc_configure,
  .enable = prv_intc_enable,
  .disable = prv_intc_disable,
};

static void prv_exti_irq_handler(uint32_t lines) {
  const struct pbl_gpio_stm32_intc *pin_intc = s_exti_intc;
  uint32_t pending;

  if (pin_intc == nullptr) {
    return;
  }

  // Clear all pending lines, dispatch unmasked ones
  pending = pbl_readl(pin_intc->exti + STM32F4_EXTI_PR_OFFSET) & lines;
  pbl_writel(pending, pin_intc->exti + STM32F4_EXTI_PR_OFFSET);
  pending &= pbl_readl(pin_intc->exti + STM32F4_EXTI_IMR_OFFSET);

  while (pending != 0U) {
    pbl_intc_dispatch(&pin_intc->intc, (uint32_t)__builtin_ctz(pending));
    pending &= pending - 1U;
  }
}

PBL_IRQ_CONNECT(EXTI0, EXTI_IRQ_PRIO, prv_exti_irq_handler, 0x0001U, 0);
PBL_IRQ_CONNECT(EXTI1, EXTI_IRQ_PRIO, prv_exti_irq_handler, 0x0002U, 0);
PBL_IRQ_CONNECT(EXTI2, EXTI_IRQ_PRIO, prv_exti_irq_handler, 0x0004U, 0);
PBL_IRQ_CONNECT(EXTI3, EXTI_IRQ_PRIO, prv_exti_irq_handler, 0x0008U, 0);
PBL_IRQ_CONNECT(EXTI4, EXTI_IRQ_PRIO, prv_exti_irq_handler, 0x0010U, 0);
PBL_IRQ_CONNECT(EXTI9_5, EXTI_IRQ_PRIO, prv_exti_irq_handler, 0x03e0U, 0);
PBL_IRQ_CONNECT(EXTI15_10, EXTI_IRQ_PRIO, prv_exti_irq_handler, 0xfc00U, 0);

static int prv_intc_init(const struct pbl_device *dev) {
  const struct pbl_gpio_stm32_intc *pin_intc =
      prv_intc(container_of(dev, const struct pbl_intc, dev));
  int ret;

  ret = pbl_clk_enable(&pin_intc->syscfg_clk);
  if (ret != 0) {
    return ret;
  }

  s_exti_intc = pin_intc;

  return 0;
}

#define PORT_INDEX(_base) ((uint8_t)(((_base) / STM32F4_GPIO_PORT_SIZE) % 16U))

#define PBL_GPIO_STM32_DEFINE(sym, _name, _base, _clk, _exti, _syscfg, _syscfg_clk, ...)  \
  extern const struct pbl_intc sym##_intc;                                                \
  __VA_OPT__(static const struct pbl_gpio_stm32_hog sym##_hogs[] = {__VA_ARGS__};)        \
  PBL_DEVICE_STATE_DEFINE(sym);                                                           \
  PBL_DEVICE_STATE_DEFINE(sym##_intc);                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gpio_stm32, struct pbl_gpio_port, port){            \
    .port = PBL_GPIO_PORT_INIT(sym, _name, prv_init, nullptr, &s_ops, 16, &sym##_intc),   \
    .base = (_base),                                                                      \
    .clk = _clk,                                                                          \
    .index = PORT_INDEX(_base),                                                           \
    __VA_OPT__(.hogs = sym##_hogs, .num_hogs = ARRAY_LENGTH(sym##_hogs), )                \
  };                                                                                      \
  PBL_DEVICE_INSTANCE(sym##_intc, struct pbl_gpio_stm32_intc, struct pbl_intc, intc) {    \
    .intc =                                                                               \
        {                                                                                 \
          .dev = PBL_DEVICE_INIT_CLASS(sym##_intc, _name "_intc", prv_intc_init,          \
                                       &sym##_inst.port.dev, nullptr, &pbl_intc_class),   \
          .ops = &s_intc_ops,                                                             \
          .lines = s_exti_lines,                                                          \
          .num_irqs = STM32F4_EXTI_NUM_PIN_LINES,                                         \
        },                                                                                \
    .port = &sym##_inst, .exti = (_exti), .syscfg = (_syscfg), .syscfg_clk = _syscfg_clk, \
  }

#include PBL_DT_INSTANCES(st_stm32f429_pinctrl_gpio)
