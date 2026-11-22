/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! What the RNG driver needs from the device header. Each use of RNG calls the test, which shows
//! the model's registers in memory and sees what the previous use read or wrote.

#pragma once

#include <stdint.h>

#define __IO volatile

typedef struct {
  __IO uint32_t CR;
  __IO uint32_t SR;
  __IO uint32_t DR;
} RNG_TypeDef;

RNG_TypeDef *fake_rng_access(void);

#define RNG (fake_rng_access())

#define RCC_AHB2ENR_RNGEN ((uint32_t)0x00000040)
#define RNG_CR_RNGEN      ((uint32_t)0x00000004)
#define RNG_SR_DRDY       ((uint32_t)0x00000001)
#define RNG_SR_CECS       ((uint32_t)0x00000002)
#define RNG_SR_SECS       ((uint32_t)0x00000004)
#define RNG_SR_CEIS       ((uint32_t)0x00000020)
#define RNG_SR_SEIS       ((uint32_t)0x00000040)
