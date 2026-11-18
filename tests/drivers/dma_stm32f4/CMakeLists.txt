# SPDX-FileCopyrightText: 2026 retake
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_dma_stm32f4
  SOURCES
    drivers/base/device.c
    drivers/dma/dma.c
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/dma_stm32f4/include
    soc/stm32/stm32f4
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_DMA_STM32F4_IRQ_PRIO=14
    CONFIG_CCM_BASE=0x10000000
    CONFIG_CCM_SIZE=0x10000
)
