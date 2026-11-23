# SPDX-FileCopyrightText: 2026 Brian Silvestri
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_le_crypto
  SOURCES
    subsys/bluetooth/le_crypto.c
    third_party/mbedtls/mbedtls/library/aes.c
    third_party/mbedtls/mbedtls/library/platform_util.c
  INCLUDES
    third_party/mbedtls/mbedtls/include
    third_party/mbedtls/include
  DEFINES
    MBEDTLS_CONFIG_FILE="pebble_mbedtls_config.h"
)

pbl_clar_test(test_hci_uart_legacy
  INCLUDES
    tests/subsys/bluetooth/include
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/nimble/transport/common/hci_h4/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
    soc/stm32/stm32f4
  DEFINES
    CONFIG_BT_LOG_LEVEL=0
    CONFIG_NIMBLE_LOG_LEVEL=-1
    CONFIG_BT_HCI_THREAD_PRIO=3
    CONFIG_BT_HCI_THREAD_STACK_SIZE=1024
    CONFIG_BT_CONTROLLER_CC2564=1
    CONFIG_BT_CC2564_SLEEP=1
    CONFIG_SOC_STM32F4=1
)
