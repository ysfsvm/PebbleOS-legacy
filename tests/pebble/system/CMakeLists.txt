# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_task_wdt_hardlockup_report
  SOURCES
    pebble/system/task_wdt.c
  DEFINES
    CONFIG_TASK_WDT_HARDLOCKUP=1
)
