# SPDX-FileCopyrightText: 2026 Yusuf Sevim
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_otp_stm32
  SOURCES
    pebble/mfg/mfg_serials.c
  INCLUDES
    soc/stm32/stm32f4
  DEFINES
    CONFIG_BOARD_SNOWY=1
    CONFIG_SOC_STM32F4=1
)
