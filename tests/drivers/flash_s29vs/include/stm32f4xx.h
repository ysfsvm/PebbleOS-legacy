/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

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

#include "stm32f4xx_rcc.h"
