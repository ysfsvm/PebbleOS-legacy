# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_pinctrl_stm32
  SOURCES
    drivers/base/device.c
    drivers/gpio/gpio.c
    drivers/gpio/stm32.c
    drivers/intc/intc.c
    drivers/pinctrl/pinctrl.c
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/pinctrl_stm32/include
    tests/drivers/gpio_stm32/include
    soc/stm32/stm32f4
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    PBL_SOC_IRQN_EXTI0=6
    PBL_SOC_IRQN_EXTI1=7
    PBL_SOC_IRQN_EXTI2=8
    PBL_SOC_IRQN_EXTI3=9
    PBL_SOC_IRQN_EXTI4=10
    PBL_SOC_IRQN_EXTI9_5=23
    PBL_SOC_IRQN_EXTI15_10=40
)
