# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_flash_s29vs
  SOURCES
    drivers/base/device.c
    drivers/flash/flash_guard.c
  OVERRIDES
    stm32f4_fmc_mmio
  INCLUDES
    tests/drivers/flash_s29vs/include
    soc/stm32/stm32f4
    third_party/hal_stm32/stm32-sdk/STM32F4xx_StdPeriph_Driver/inc
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_SOC_STM32F4=1
    CONFIG_FLASH_S29VS=1
    CONFIG_FLASH_GUARD=1
    CONFIG_BOARD_PARTITIONS_DT=1
)
