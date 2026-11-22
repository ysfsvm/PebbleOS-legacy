/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#define __IO volatile

typedef struct {
  __IO uint16_t CR1;
  uint16_t RESERVED0;
  __IO uint16_t CR2;
  uint16_t RESERVED1;
  __IO uint16_t SR;
  uint16_t RESERVED2;
  __IO uint16_t DR;
  uint16_t RESERVED3;
  __IO uint16_t CRCPR;
  uint16_t RESERVED4;
  __IO uint16_t RXCRCR;
  uint16_t RESERVED5;
  __IO uint16_t TXCRCR;
  uint16_t RESERVED6;
  __IO uint16_t I2SCFGR;
  uint16_t RESERVED7;
  __IO uint16_t I2SPR;
  uint16_t RESERVED8;
} SPI_TypeDef;
