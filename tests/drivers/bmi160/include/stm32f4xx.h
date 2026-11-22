/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

//! What the Standard Peripheral Library's SPI, GPIO and RCC parts and the board header need from
//! the device header, with the SPI1 registers in memory.

#pragma once

#include <stdint.h>

#define STM32F429_439xx

#define __IO volatile

typedef enum {
  RESET = 0,
  SET = !RESET
} FlagStatus, ITStatus;
typedef enum {
  DISABLE = 0,
  ENABLE = !DISABLE
} FunctionalState;
typedef enum {
  ERROR = 0,
  SUCCESS = !ERROR
} ErrorStatus;

#define assert_param(expr) ((void)0)

#define HSE_VALUE ((uint32_t)16000000)
#define HSI_VALUE ((uint32_t)16000000)

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

typedef struct {
  __IO uint32_t MODER;
  __IO uint32_t OTYPER;
  __IO uint32_t OSPEEDR;
  __IO uint32_t PUPDR;
  __IO uint32_t IDR;
  __IO uint32_t ODR;
  __IO uint32_t BSRR;
  __IO uint32_t LCKR;
  __IO uint32_t AFR[2];
} GPIO_TypeDef;

//! Only what the library's I2S code reads; never reached here.
typedef struct {
  __IO uint32_t CFGR;
  __IO uint32_t PLLCFGR;
  __IO uint32_t PLLI2SCFGR;
} RCC_TypeDef;

extern SPI_TypeDef fake_spi[6];
extern RCC_TypeDef fake_rcc;

#define SPI1    (&fake_spi[0])
#define SPI2    (&fake_spi[1])
#define SPI3    (&fake_spi[2])
#define SPI4    (&fake_spi[3])
#define SPI5    (&fake_spi[4])
#define SPI6    (&fake_spi[5])
#define I2S2ext (&fake_spi[1])
#define I2S3ext (&fake_spi[2])
#define RCC     (&fake_rcc)

#define SPI_CR1_SPE         ((uint16_t)0x0040)
#define SPI_CR1_CRCNEXT     ((uint16_t)0x1000)
#define SPI_CR1_CRCEN       ((uint16_t)0x2000)
#define SPI_CR2_SSOE        ((uint8_t)0x04)
#define SPI_I2SCFGR_I2SE    ((uint16_t)0x0400)
#define SPI_I2SCFGR_I2SMOD  ((uint16_t)0x0800)
#define RCC_CFGR_I2SSRC     ((uint32_t)0x00800000)
#define RCC_PLLCFGR_PLLM    ((uint32_t)0x0000003F)
#define RCC_PLLCFGR_PLLSRC  ((uint32_t)0x00400000)
#define RCC_PLLCFGR_PLLSRC_HSE ((uint32_t)0x00400000)
#define RCC_PLLI2SCFGR_PLLI2SN ((uint32_t)0x00007FC0)
#define RCC_PLLI2SCFGR_PLLI2SR ((uint32_t)0x70000000)

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_spi.h"
