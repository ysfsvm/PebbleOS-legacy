/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! What the Standard Peripheral Library's I2C, GPIO and RCC parts and the board header need from
//! the device header, with the I2C1 registers in memory.

#pragma once

#include <stdint.h>

#define STM32F429_439xx

// I2C_ReadRegister() and I2C_GetITStatus() cast a pointer to uint32_t; unused here.
#pragma clang diagnostic ignored "-Wpointer-to-int-cast"
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"

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

typedef struct {
  __IO uint16_t CR1;
  uint16_t RESERVED0;
  __IO uint16_t CR2;
  uint16_t RESERVED1;
  __IO uint16_t OAR1;
  uint16_t RESERVED2;
  __IO uint16_t OAR2;
  uint16_t RESERVED3;
  __IO uint16_t DR;
  uint16_t RESERVED4;
  __IO uint16_t SR1;
  uint16_t RESERVED5;
  __IO uint16_t SR2;
  uint16_t RESERVED6;
  __IO uint16_t CCR;
  uint16_t RESERVED7;
  __IO uint16_t TRISE;
  uint16_t RESERVED8;
  __IO uint16_t FLTR;
  uint16_t RESERVED9;
} I2C_TypeDef;

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

typedef struct {
  __IO uint16_t CR1;
} SPI_TypeDef;

extern I2C_TypeDef fake_i2c[3];

#define I2C1 (&fake_i2c[0])
#define I2C2 (&fake_i2c[1])
#define I2C3 (&fake_i2c[2])

#define I2C_CR1_PE        ((uint16_t)0x0001)
#define I2C_CR1_ENPEC     ((uint16_t)0x0020)
#define I2C_CR1_ENGC      ((uint16_t)0x0040)
#define I2C_CR1_NOSTRETCH ((uint16_t)0x0080)
#define I2C_CR1_START     ((uint16_t)0x0100)
#define I2C_CR1_STOP      ((uint16_t)0x0200)
#define I2C_CR1_ACK       ((uint16_t)0x0400)
#define I2C_CR1_PEC       ((uint16_t)0x1000)
#define I2C_CR1_ENARP     ((uint16_t)0x0010)
#define I2C_CR1_SWRST     ((uint16_t)0x8000)
#define I2C_CR2_FREQ      ((uint16_t)0x003F)
#define I2C_CR2_ITERREN   ((uint16_t)0x0100)
#define I2C_CR2_ITEVTEN   ((uint16_t)0x0200)
#define I2C_CR2_ITBUFEN   ((uint16_t)0x0400)
#define I2C_CR2_DMAEN     ((uint16_t)0x0800)
#define I2C_CR2_LAST      ((uint16_t)0x1000)
#define I2C_OAR1_ADD0     ((uint16_t)0x0001)
#define I2C_OAR2_ENDUAL   ((uint8_t)0x01)
#define I2C_OAR2_ADD2     ((uint8_t)0xFE)
#define I2C_SR1_SB        ((uint16_t)0x0001)
#define I2C_SR1_ADDR      ((uint16_t)0x0002)
#define I2C_SR1_BTF       ((uint16_t)0x0004)
#define I2C_SR1_STOPF     ((uint16_t)0x0010)
#define I2C_SR1_RXNE      ((uint16_t)0x0040)
#define I2C_SR1_TXE       ((uint16_t)0x0080)
#define I2C_SR1_BERR      ((uint16_t)0x0100)
#define I2C_SR1_ARLO      ((uint16_t)0x0200)
#define I2C_SR1_AF        ((uint16_t)0x0400)
#define I2C_SR1_OVR       ((uint16_t)0x0800)
#define I2C_SR2_MSL       ((uint16_t)0x0001)
#define I2C_SR2_BUSY      ((uint16_t)0x0002)
#define I2C_SR2_TRA       ((uint16_t)0x0004)
#define I2C_CCR_CCR       ((uint16_t)0x0FFF)
#define I2C_CCR_FS        ((uint16_t)0x8000)
#define I2C_FLTR_DNF      ((uint8_t)0x0F)
#define I2C_FLTR_ANOFF    ((uint8_t)0x10)

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_i2c.h"
#include "stm32f4xx_rcc.h"
