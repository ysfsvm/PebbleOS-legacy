/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/cmsis.h>

void soc_stm32_clock_set(enum soc_stm32_bus bus, uint32_t mask, bool enable) {
  __IO uint32_t *reg;

  switch (bus) {
    case SOC_STM32_AHB1:
      reg = &RCC->AHB1ENR;
      break;
    case SOC_STM32_AHB2:
      reg = &RCC->AHB2ENR;
      break;
    case SOC_STM32_AHB3:
      reg = &RCC->AHB3ENR;
      break;
    case SOC_STM32_APB1:
      reg = &RCC->APB1ENR;
      break;
    default:
      reg = &RCC->APB2ENR;
      break;
  }

  // Interrupts above the kernel's lock change these registers too: mask them all.
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (enable) {
    *reg |= mask;
  } else {
    *reg &= ~mask;
  }
  // A peripheral can be accessed two clock cycles after its enable bit is set.
  (void)*reg;
  __set_PRIMASK(primask);
}
