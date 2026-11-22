/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

//! What the Standard Peripheral Library's GPIO, RCC and RTC parts need from the device header,
//! with the GPIO registers in memory.

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

extern GPIO_TypeDef fake_gpio[11];

#define GPIOA (&fake_gpio[0])
#define GPIOB (&fake_gpio[1])
#define GPIOC (&fake_gpio[2])
#define GPIOD (&fake_gpio[3])
#define GPIOE (&fake_gpio[4])
#define GPIOF (&fake_gpio[5])
#define GPIOG (&fake_gpio[6])
#define GPIOH (&fake_gpio[7])
#define GPIOI (&fake_gpio[8])
#define GPIOJ (&fake_gpio[9])
#define GPIOK (&fake_gpio[10])

#define GPIO_MODER_MODER0      ((uint32_t)0x00000003)
#define GPIO_OSPEEDER_OSPEEDR0 ((uint32_t)0x00000003)
#define GPIO_OTYPER_OT_0       ((uint32_t)0x00000001)
#define GPIO_PUPDR_PUPDR0      ((uint32_t)0x00000003)

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
