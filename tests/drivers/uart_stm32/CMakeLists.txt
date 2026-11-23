# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_uart_stm32
  SOURCES
    drivers/base/device.c
    drivers/uart/uart.c
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/uart_stm32/include
    soc/stm32/stm32f4
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_UART_STM32_IRQ_PRIO=13
)
