# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_max14690
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DRIVER_PMIC_LOG_LEVEL=0
    CONFIG_PMIC_MAX14690=1
)
