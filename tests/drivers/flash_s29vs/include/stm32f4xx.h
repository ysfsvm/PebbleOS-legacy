/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

//! What the Standard Peripheral Library's GPIO, FMC, RCC and RTC parts need from the device
//! header, with the GPIO and FMC registers in memory.

#pragma once

#include <stdint.h>

#define STM32F429_439xx

// FMC_NORSRAMStructInit() casts a pointer to uint32_t; unused here.
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
  __IO uint32_t BTCR[8];
} FMC_Bank1_TypeDef;

typedef struct {
  __IO uint32_t BWTR[7];
} FMC_Bank1E_TypeDef;

typedef struct {
  __IO uint32_t PCR2;
  __IO uint32_t SR2;
  __IO uint32_t PMEM2;
  __IO uint32_t PATT2;
  uint32_t RESERVED0;
  __IO uint32_t ECCR2;
} FMC_Bank2_TypeDef;

typedef struct {
  __IO uint32_t PCR3;
  __IO uint32_t SR3;
  __IO uint32_t PMEM3;
  __IO uint32_t PATT3;
  uint32_t RESERVED0;
  __IO uint32_t ECCR3;
} FMC_Bank3_TypeDef;

typedef struct {
  __IO uint32_t PCR4;
  __IO uint32_t SR4;
  __IO uint32_t PMEM4;
  __IO uint32_t PATT4;
  __IO uint32_t PIO4;
} FMC_Bank4_TypeDef;

typedef struct {
  __IO uint32_t SDCR[2];
  __IO uint32_t SDTR[2];
  __IO uint32_t SDCMR;
  __IO uint32_t SDRTR;
  __IO uint32_t SDSR;
} FMC_Bank5_6_TypeDef;

extern GPIO_TypeDef fake_gpio[11];
extern FMC_Bank1_TypeDef fake_fmc_bank1;
extern FMC_Bank1E_TypeDef fake_fmc_bank1e;
extern FMC_Bank2_TypeDef fake_fmc_bank2;
extern FMC_Bank3_TypeDef fake_fmc_bank3;
extern FMC_Bank4_TypeDef fake_fmc_bank4;
extern FMC_Bank5_6_TypeDef fake_fmc_bank5_6;

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

#define FMC_Bank1   (&fake_fmc_bank1)
#define FMC_Bank1E  (&fake_fmc_bank1e)
#define FMC_Bank2   (&fake_fmc_bank2)
#define FMC_Bank3   (&fake_fmc_bank3)
#define FMC_Bank4   (&fake_fmc_bank4)
#define FMC_Bank5_6 (&fake_fmc_bank5_6)

#define FMC_BCR1_MBKEN     ((uint32_t)0x00000001)
#define FMC_BCR1_MUXEN     ((uint32_t)0x00000002)
#define FMC_BCR1_MTYP      ((uint32_t)0x0000000C)
#define FMC_BCR1_MWID      ((uint32_t)0x00000030)
#define FMC_BCR1_FACCEN    ((uint32_t)0x00000040)
#define FMC_BCR1_BURSTEN   ((uint32_t)0x00000100)
#define FMC_BCR1_WAITPOL   ((uint32_t)0x00000200)
#define FMC_BCR1_WRAPMOD   ((uint32_t)0x00000400)
#define FMC_BCR1_WAITCFG   ((uint32_t)0x00000800)
#define FMC_BCR1_WREN      ((uint32_t)0x00001000)
#define FMC_BCR1_WAITEN    ((uint32_t)0x00002000)
#define FMC_BCR1_EXTMOD    ((uint32_t)0x00004000)
#define FMC_BCR1_ASYNCWAIT ((uint32_t)0x00008000)
#define FMC_BCR1_CBURSTRW  ((uint32_t)0x00080000)
#define FMC_BCR1_CCLKEN    ((uint32_t)0x00100000)

#define FMC_BWTR1_ADDSET  ((uint32_t)0x0000000F)
#define FMC_BWTR1_ADDHLD  ((uint32_t)0x000000F0)
#define FMC_BWTR1_DATAST  ((uint32_t)0x0000FF00)
#define FMC_BWTR1_BUSTURN ((uint32_t)0x000F0000)
#define FMC_BWTR1_ACCMOD  ((uint32_t)0x30000000)

#define FMC_PATT2_ATTHIZ2      ((uint32_t)0xFF000000)
#define FMC_PATT2_ATTHOLD2     ((uint32_t)0x00FF0000)
#define FMC_PATT2_ATTSET2      ((uint32_t)0x000000FF)
#define FMC_PATT2_ATTWAIT2     ((uint32_t)0x0000FF00)
#define FMC_PATT4_ATTHIZ4      ((uint32_t)0xFF000000)
#define FMC_PATT4_ATTHOLD4     ((uint32_t)0x00FF0000)
#define FMC_PATT4_ATTSET4      ((uint32_t)0x000000FF)
#define FMC_PATT4_ATTWAIT4     ((uint32_t)0x0000FF00)
#define FMC_PCR2_ECCEN         ((uint32_t)0x00000040)
#define FMC_PCR2_ECCPS         ((uint32_t)0x000E0000)
#define FMC_PCR2_PBKEN         ((uint32_t)0x00000004)
#define FMC_PCR2_PTYP          ((uint32_t)0x00000008)
#define FMC_PCR2_PWAITEN       ((uint32_t)0x00000002)
#define FMC_PCR2_PWID          ((uint32_t)0x00000030)
#define FMC_PCR2_TAR           ((uint32_t)0x0001E000)
#define FMC_PCR2_TCLR          ((uint32_t)0x00001E00)
#define FMC_PCR4_PWAITEN       ((uint32_t)0x00000002)
#define FMC_PCR4_PWID          ((uint32_t)0x00000030)
#define FMC_PCR4_TAR           ((uint32_t)0x0001E000)
#define FMC_PCR4_TCLR          ((uint32_t)0x00001E00)
#define FMC_PIO4_IOHIZ4        ((uint32_t)0xFF000000)
#define FMC_PIO4_IOHOLD4       ((uint32_t)0x00FF0000)
#define FMC_PIO4_IOSET4        ((uint32_t)0x000000FF)
#define FMC_PIO4_IOWAIT4       ((uint32_t)0x0000FF00)
#define FMC_PMEM2_MEMHIZ2      ((uint32_t)0xFF000000)
#define FMC_PMEM2_MEMHOLD2     ((uint32_t)0x00FF0000)
#define FMC_PMEM2_MEMSET2      ((uint32_t)0x000000FF)
#define FMC_PMEM2_MEMWAIT2     ((uint32_t)0x0000FF00)
#define FMC_PMEM4_MEMHIZ4      ((uint32_t)0xFF000000)
#define FMC_PMEM4_MEMHOLD4     ((uint32_t)0x00FF0000)
#define FMC_PMEM4_MEMSET4      ((uint32_t)0x000000FF)
#define FMC_PMEM4_MEMWAIT4     ((uint32_t)0x0000FF00)
#define FMC_SDCR1_CAS          ((uint32_t)0x00000180)
#define FMC_SDCR1_MWID         ((uint32_t)0x00000030)
#define FMC_SDCR1_NB           ((uint32_t)0x00000040)
#define FMC_SDCR1_NC           ((uint32_t)0x00000003)
#define FMC_SDCR1_NR           ((uint32_t)0x0000000C)
#define FMC_SDCR1_RBURST       ((uint32_t)0x00001000)
#define FMC_SDCR1_RPIPE        ((uint32_t)0x00006000)
#define FMC_SDCR1_SDCLK        ((uint32_t)0x00000C00)
#define FMC_SDCR1_WP           ((uint32_t)0x00000200)
#define FMC_SDRTR_CRE          ((uint32_t)0x00000001)
#define FMC_SDSR_MODES1        ((uint32_t)0x00000006)
#define FMC_SDSR_MODES2        ((uint32_t)0x00000018)
#define FMC_SDSR_RE            ((uint32_t)0x00000001)
#define FMC_SDTR1_TMRD         ((uint32_t)0x0000000F)
#define FMC_SDTR1_TRAS         ((uint32_t)0x00000F00)
#define FMC_SDTR1_TRCD         ((uint32_t)0x0F000000)
#define FMC_SDTR1_TRC          ((uint32_t)0x0000F000)
#define FMC_SDTR1_TRP          ((uint32_t)0x00F00000)
#define FMC_SDTR1_TWR          ((uint32_t)0x000F0000)
#define FMC_SDTR1_TXSR         ((uint32_t)0x000000F0)
#define GPIO_MODER_MODER0      ((uint32_t)0x00000003)
#define GPIO_OSPEEDER_OSPEEDR0 ((uint32_t)0x00000003)
#define GPIO_OTYPER_OT_0       ((uint32_t)0x00000001)
#define GPIO_PUPDR_PUPDR0      ((uint32_t)0x00000003)

#include "stm32f4xx_fmc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
