# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_rng_stm32
  SOURCES
    drivers/base/device.c
    drivers/clk/clk.c
    drivers/clk/stm32f4.c
    drivers/rng/stm32.c
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/rng_stm32/include
    soc/stm32/stm32f4
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_SOC_STM32F4=1
)
