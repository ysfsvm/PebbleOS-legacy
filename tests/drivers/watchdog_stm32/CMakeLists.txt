# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_watchdog_stm32
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/watchdog_stm32/include
    soc/stm32/stm32f4
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_WATCHDOG=1
    CONFIG_TASK_WDT_HARDLOCKUP=1
)
