/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define STM32F4_TIM_CR1_OFFSET   0x00
#define STM32F4_TIM_CR2_OFFSET   0x04
#define STM32F4_TIM_SMCR_OFFSET  0x08
#define STM32F4_TIM_DIER_OFFSET  0x0c
#define STM32F4_TIM_SR_OFFSET    0x10
#define STM32F4_TIM_EGR_OFFSET   0x14
#define STM32F4_TIM_CCMR1_OFFSET 0x18
#define STM32F4_TIM_CCMR2_OFFSET 0x1c
#define STM32F4_TIM_CCER_OFFSET  0x20
#define STM32F4_TIM_CNT_OFFSET   0x24
#define STM32F4_TIM_PSC_OFFSET   0x28
#define STM32F4_TIM_ARR_OFFSET   0x2c
#define STM32F4_TIM_RCR_OFFSET   0x30
#define STM32F4_TIM_CCR1_OFFSET  0x34
#define STM32F4_TIM_CCR2_OFFSET  0x38
#define STM32F4_TIM_CCR3_OFFSET  0x3c
#define STM32F4_TIM_CCR4_OFFSET  0x40
#define STM32F4_TIM_BDTR_OFFSET  0x44

/* CCMR of channel ch (0-3), CCR of channel ch */
#define STM32F4_TIM_CCMR_OFFSET(ch) (STM32F4_TIM_CCMR1_OFFSET + 4U * ((ch) / 2U))
#define STM32F4_TIM_CCR_OFFSET(ch)  (STM32F4_TIM_CCR1_OFFSET + 4U * (ch))

/* CR1 */
#define STM32F4_TIM_CR1_CEN      PBL_BIT(0)
#define STM32F4_TIM_CR1_UDIS     PBL_BIT(1)
#define STM32F4_TIM_CR1_URS      PBL_BIT(2)
#define STM32F4_TIM_CR1_OPM      PBL_BIT(3)
#define STM32F4_TIM_CR1_DIR      PBL_BIT(4)
#define STM32F4_TIM_CR1_CMS_MASK PBL_GENMASK(6, 5)
#define STM32F4_TIM_CR1_ARPE     PBL_BIT(7)
#define STM32F4_TIM_CR1_CKD_MASK PBL_GENMASK(9, 8)

/* EGR */
#define STM32F4_TIM_EGR_UG PBL_BIT(0)

/* CCMRx: a byte per channel */
#define STM32F4_TIM_CCMR_CH_SHIFT(ch) (8U * ((ch) % 2U))
#define STM32F4_TIM_CCMR_CH_MASK      PBL_GENMASK(7, 0)
#define STM32F4_TIM_CCMR_CCS_MASK     PBL_GENMASK(1, 0)
#define STM32F4_TIM_CCMR_OCFE         PBL_BIT(2)
#define STM32F4_TIM_CCMR_OCPE         PBL_BIT(3)
#define STM32F4_TIM_CCMR_OCM_MASK     PBL_GENMASK(6, 4)
#define STM32F4_TIM_CCMR_OCCE         PBL_BIT(7)
#define STM32F4_TIM_CCMR_OCM_PWM1     6U

/* CCER: four bits per channel */
#define STM32F4_TIM_CCER_CH_SHIFT(ch) (4U * (ch))
#define STM32F4_TIM_CCER_CCE          PBL_BIT(0)
#define STM32F4_TIM_CCER_CCP          PBL_BIT(1)
#define STM32F4_TIM_CCER_CCNE         PBL_BIT(2)
#define STM32F4_TIM_CCER_CCNP         PBL_BIT(3)

/* 16-bit timer limits */
#define STM32F4_TIM_PSC_MAX 0xffffU
#define STM32F4_TIM_ARR_MAX 0xffffU
