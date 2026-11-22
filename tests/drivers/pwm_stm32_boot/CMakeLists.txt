# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_pwm_stm32_boot
  SOURCES
    drivers/backlight/backlight.c
    drivers/backlight/pwm.c
    drivers/base/device.c
    drivers/clk/clk.c
    drivers/clk/stm32f4.c
    drivers/gpio/gpio.c
    drivers/gpio/stm32.c
    drivers/intc/intc.c
    drivers/pinctrl/pinctrl.c
    drivers/pinctrl/stm32.c
    drivers/pwm/pwm.c
    drivers/pwm/stm32.c
    drivers/vibe/pwm.c
    drivers/vibe/vibe.c
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/pwm_stm32_boot/include
    tests/drivers/pwm_stm32/include
    soc/stm32/stm32f4
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_SOC_STM32F4=1
    PBL_SOC_IRQN_EXTI0=6
    PBL_SOC_IRQN_EXTI1=7
    PBL_SOC_IRQN_EXTI2=8
    PBL_SOC_IRQN_EXTI3=9
    PBL_SOC_IRQN_EXTI4=10
    PBL_SOC_IRQN_EXTI9_5=23
    PBL_SOC_IRQN_EXTI15_10=40
)
