/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! The stock firmware's EXTI calls on top of the interrupt controllers of the GPIO ports, for the
//! drivers that still use them.

#include <pbl/drivers/exti.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/drivers/intc.h>
#include <pbl/kernel/assert.h>
#include <pbl/util/size.h>

#include <devicetree/devices.h>

// By port index, A = 0
static const struct pbl_intc *const s_intcs[] = {
  &dt_gpioa_intc, &dt_gpiob_intc, &dt_gpioc_intc, &dt_gpiod_intc,
  &dt_gpioe_intc, &dt_gpiof_intc, &dt_gpiog_intc, &dt_gpioh_intc,
};

static void prv_call(void *arg) {
  ((ExtiHandlerCallback)arg)();
}

static struct pbl_irq_spec prv_spec(ExtiConfig cfg, uint8_t type) {
  PBL_ASSERTN(cfg.exti_port_source < ARRAY_LENGTH(s_intcs));

  return (struct pbl_irq_spec)PBL_IRQ_SPEC(s_intcs[cfg.exti_port_source], cfg.exti_line, type);
}

void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb) {
  static const uint8_t s_types[] = {
    [ExtiTrigger_Rising] = PBL_IRQ_TYPE_EDGE_RISING,
    [ExtiTrigger_Falling] = PBL_IRQ_TYPE_EDGE_FALLING,
    [ExtiTrigger_RisingFalling] = PBL_IRQ_TYPE_EDGE_BOTH,
  };
  const struct pbl_irq_spec spec = prv_spec(cfg, s_types[trigger]);

  PBL_ASSERTN(pbl_device_init(&spec.intc->dev) == 0);
  // Configuring a pin again replaces its handler
  (void)pbl_intc_free(&spec);
  PBL_ASSERTN(pbl_intc_request(&spec, prv_call, cb) == 0);
}

void exti_enable(ExtiConfig config) {
  const struct pbl_irq_spec spec = prv_spec(config, PBL_IRQ_TYPE_EDGE_RISING);

  (void)pbl_intc_enable(&spec);
}

void exti_disable(ExtiConfig config) {
  const struct pbl_irq_spec spec = prv_spec(config, PBL_IRQ_TYPE_EDGE_RISING);

  (void)pbl_intc_disable(&spec);
}
