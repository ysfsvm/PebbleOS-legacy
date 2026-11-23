# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_hardlockup_test
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_WATCHDOG=1
)
