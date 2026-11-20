/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! STM32F4 independent watchdog (IWDG). The stock bootloader starts it
//! before jumping to the firmware, and it cannot be stopped once running.

#include <stdint.h>

#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/mmio.h>
#include <pbl/task_wdt/hardlockup.h>

#include <hardware/stm32f4_dbgmcu.h>
#include <hardware/stm32f4_iwdg.h>
#include <hardware/stm32f4_rcc.h>

#define IWDG_BASE_ADDR 0x40003000U
#define RCC_BASE_ADDR  0x40023800U

// LSI ~32 kHz / 64 = 500 Hz; 0xfff ticks is ~8 s, matching the bootloader.
#define IWDG_PRESCALER STM32F4_IWDG_PR_DIV64
#define IWDG_RELOAD    0xfffU

static inline void prv_iwdg_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, IWDG_BASE_ADDR + offset);
}

static inline uint32_t prv_csr(void) {
  return pbl_readl(RCC_BASE_ADDR + STM32F4_RCC_CSR_OFFSET);
}

void watchdog_init(void) {
  prv_iwdg_putreg(STM32F4_IWDG_KR_OFFSET, STM32F4_IWDG_KR_UNLOCK);
  prv_iwdg_putreg(STM32F4_IWDG_PR_OFFSET, IWDG_PRESCALER);
  prv_iwdg_putreg(STM32F4_IWDG_RLR_OFFSET, IWDG_RELOAD);
  prv_iwdg_putreg(STM32F4_IWDG_KR_OFFSET, STM32F4_IWDG_KR_LOCK);

  // Freeze the watchdog while a debugger halts the core.
  const uintptr_t apb1fz = STM32F4_DBGMCU_BASE + STM32F4_DBGMCU_APB1FZ_OFFSET;
  pbl_writel(pbl_readl(apb1fz) | STM32F4_DBGMCU_APB1FZ_IWDG_STOP, apb1fz);
}

void watchdog_start(void) {
  prv_iwdg_putreg(STM32F4_IWDG_KR_OFFSET, STM32F4_IWDG_KR_START);
  watchdog_feed();
}

void watchdog_stop(void) {
  // The IWDG cannot be stopped once started.
}

void watchdog_feed(void) {
  prv_iwdg_putreg(STM32F4_IWDG_KR_OFFSET, STM32F4_IWDG_KR_RELOAD);
  pbl_task_wdt_hardlockup_feed();
}

bool watchdog_check_reset_flag(void) {
  return (prv_csr() & STM32F4_RCC_CSR_IWDGRSTF) != 0U;
}

McuRebootReason watchdog_get_reset_flag(void) {
  const uint32_t csr = prv_csr();

  return (McuRebootReason){
    .brown_out_reset = (csr & STM32F4_RCC_CSR_BORRSTF) != 0U,
    .pin_reset = (csr & STM32F4_RCC_CSR_PINRSTF) != 0U,
    .power_on_reset = (csr & STM32F4_RCC_CSR_PORRSTF) != 0U,
    .software_reset = (csr & STM32F4_RCC_CSR_SFTRSTF) != 0U,
    .independent_watchdog_reset = (csr & STM32F4_RCC_CSR_IWDGRSTF) != 0U,
    .window_watchdog_reset = (csr & STM32F4_RCC_CSR_WWDGRSTF) != 0U,
    .low_power_manager_reset = (csr & STM32F4_RCC_CSR_LPWRRSTF) != 0U,
  };
}

McuRebootReason watchdog_clear_reset_flag(void) {
  const McuRebootReason reason = watchdog_get_reset_flag();
  const uintptr_t csr = RCC_BASE_ADDR + STM32F4_RCC_CSR_OFFSET;
  pbl_writel(pbl_readl(csr) | STM32F4_RCC_CSR_RMVF, csr);
  return reason;
}
