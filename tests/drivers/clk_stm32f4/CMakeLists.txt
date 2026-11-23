# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_clk_stm32f4
  SOURCES
    drivers/base/device.c
    drivers/clk/clk.c
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/clk_stm32f4/include
    soc/stm32/stm32f4
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
)
