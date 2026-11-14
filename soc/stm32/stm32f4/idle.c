/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Idle states: WFI, or STOP mode (clocks off, low-power regulator) with the external flash
//! powered down, as the stock firmware did. The kernel wakes the CPU up from STOP with the RTC
//! clock event (drivers/timer/stm32_rtc.c); any EXTI line wakes it too.

#include <pbl/drivers/flash.h>
#include <pbl/drivers/timer/stm32_rtc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/idle.h>
#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/cmsis.h>
#include <pbl/soc/stm32/sleep.h>
#include <pbl/util/size.h>

enum {
  IDLE_WFI,
  IDLE_STOP,
};

//! STM32F2/F4 erratum: after waking from STOP or SLEEP the core can misbehave
//! depending on the bytes after "bx lr". Keep WFI 16-byte aligned and pad the
//! rest of the cache line with NOPs.
static PBL_NOINLINE PBL_NAKED void prv_do_wfi(void) {
  __asm volatile(
      ".align 4 \n"
      "wfi      \n"
      "nop      \n"
      "bx lr    \n"
      "nop      \n"
      "nop      \n"
      "nop      \n"
      "nop      \n"
      "nop      \n");
}

static volatile uint32_t s_stop_entries;

uint32_t soc_stm32_stop_entries(void) {
  return s_stop_entries;
}

static void prv_wfi(void) {
  stm32_rtc_event_check();
  __DSB();
  prv_do_wfi();
  __ISB();
}

//! Runs with the external flash powered down and interrupts masked.
static void prv_stop_cb(void *arg) {
  soc_stm32_clock_set(SOC_STM32_APB1, RCC_APB1ENR_PWREN, true);

  // Low-power regulator in STOP, not standby. LPUDS (under-drive) stays clear.
  uint32_t cr = PWR->CR;
  cr &= ~PWR_CR_PDDS;
  cr |= PWR_CR_LPDS;
  PWR->CR = cr;

  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  prv_do_wfi();
  __ISB();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

  // STOP leaves us on the HSI: back to the PLL.
  RCC->CR |= RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0) {
  }
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
  }

  // The RTC's shadow registers are stale after STOP; the system clock source reads them.
  stm32_rtc_resync();
}

static void prv_stop(void) {
  stm32_rtc_event_check();
  s_stop_entries++;
  pbl_flash_power_down(FLASH, prv_stop_cb, nullptr);
}

// STOP takes the PLL relock (about 100 us) to leave: one tick covers it.
const struct pbl_idle_state pbl_soc_idle_states[] = {
  [IDLE_WFI] = {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = prv_wfi},
  [IDLE_STOP] = {.name = "stop", .min_ticks = 5, .exit_ticks = 1, .enter = prv_stop},
};
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

int pbl_soc_idle_state_limit(void) {
#ifdef CONFIG_SOC_STM32F4_EMULATED
  // The emulator stops SysTick, the system timer's event there, in STOP.
  return IDLE_WFI;
#else
  return soc_stm32_stop_is_allowed() ? IDLE_STOP : IDLE_WFI;
#endif
}
