/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! What the Standard Peripheral Library's TIM, GPIO and RCC headers and the timer PWM driver need
//! from the device header, with the TIM10 and TIM12 registers in memory.

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
  __IO uint16_t CR1;
  uint16_t RESERVED0;
  __IO uint16_t CR2;
  uint16_t RESERVED1;
  __IO uint16_t SMCR;
  uint16_t RESERVED2;
  __IO uint16_t DIER;
  uint16_t RESERVED3;
  __IO uint16_t SR;
  uint16_t RESERVED4;
  __IO uint16_t EGR;
  uint16_t RESERVED5;
  __IO uint16_t CCMR1;
  uint16_t RESERVED6;
  __IO uint16_t CCMR2;
  uint16_t RESERVED7;
  __IO uint16_t CCER;
  uint16_t RESERVED8;
  __IO uint32_t CNT;
  __IO uint16_t PSC;
  uint16_t RESERVED9;
  __IO uint32_t ARR;
  __IO uint16_t RCR;
  uint16_t RESERVED10;
  __IO uint32_t CCR1;
  __IO uint32_t CCR2;
  __IO uint32_t CCR3;
  __IO uint32_t CCR4;
  __IO uint16_t BDTR;
  uint16_t RESERVED11;
  __IO uint16_t DCR;
  uint16_t RESERVED12;
  __IO uint16_t DMAR;
  uint16_t RESERVED13;
  __IO uint16_t OR;
  uint16_t RESERVED14;
} TIM_TypeDef;

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

extern TIM_TypeDef fake_tim10;
extern TIM_TypeDef fake_tim12;

#define TIM10 (&fake_tim10)
#define TIM12 (&fake_tim12)

#define TIM_CR1_CEN    ((uint16_t)0x0001)
#define TIM_CR1_ARPE   ((uint16_t)0x0080)
#define TIM_CCER_CC1E  ((uint16_t)0x0001)
#define TIM_CCER_CC1P  ((uint16_t)0x0002)
#define TIM_CCER_CC1NP ((uint16_t)0x0008)

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"
