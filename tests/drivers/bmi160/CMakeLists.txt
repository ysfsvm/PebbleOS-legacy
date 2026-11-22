# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_bmi160
  SOURCES
    drivers/base/device.c
    drivers/clk/clk.c
    drivers/clk/stm32f4.c
    drivers/gpio/gpio.c
    drivers/gpio/stm32.c
    drivers/intc/intc.c
    third_party/hal_stm32/stm32-sdk/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_spi.c
  OVERRIDES
    stm32f4_mmio
  INCLUDES
    tests/drivers/bmi160/include
    soc/stm32/stm32f4
    third_party/hal_stm32/stm32-sdk/STM32F4xx_StdPeriph_Driver/inc
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_SOC_STM32F4=1
    CONFIG_ACCEL_BMI160=1
    PBL_SOC_IRQN_EXTI0=6
    PBL_SOC_IRQN_EXTI1=7
    PBL_SOC_IRQN_EXTI2=8
    PBL_SOC_IRQN_EXTI3=9
    PBL_SOC_IRQN_EXTI4=10
    PBL_SOC_IRQN_EXTI9_5=23
    PBL_SOC_IRQN_EXTI15_10=40
    SPI_I2S_SendData=st_SPI_I2S_SendData
    SPI_I2S_ReceiveData=st_SPI_I2S_ReceiveData
    SPI_I2S_GetFlagStatus=st_SPI_I2S_GetFlagStatus
)
