/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/init.h>
#include <pbl/soc/stm32/cmsis.h>

void pbl_soc_early_init(void) {
  SystemInit();

  // Only a power-on reset clears these: a debug session could leave the clocks running in STOP
  // and the core in sleep. The stock firmware cleared them at boot too.
  DBGMCU->CR &= ~(DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);

  // Already running if the stock bootloader started us; keep it fed.
  watchdog_init();
  watchdog_start();
}
