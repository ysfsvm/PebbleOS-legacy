# SPDX-FileCopyrightText: 2026 retake
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_gpio_stm32
  SOURCES
    drivers/base/device.c
    drivers/gpio/gpio.c
    drivers/intc/intc.c
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/gpio_stm32/include
    soc/stm32/stm32f4
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
)
