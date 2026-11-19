# SPDX-FileCopyrightText: 2026 retake
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_mag3110
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DRIVER_IMU_LOG_LEVEL=0
)
