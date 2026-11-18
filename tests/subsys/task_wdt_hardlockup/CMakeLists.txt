# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_task_wdt_hardlockup
  SOURCES
    subsys/task_wdt/hardlockup.c
  DEFINES
    CONFIG_TASK_WDT_HARDLOCKUP=1
    CONFIG_TASK_WDT_HARDLOCKUP_TIMEOUT_MS=6000
    CONFIG_TASK_WDT_CHECK_PERIOD_MS=3000
    CONFIG_TASK_WDT_GRACE_MS=1000
    CONFIG_WATCHDOG_TIMEOUT_MS=8000
)
