/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(CONFIG_SOC_STM32F439)
#include "soc_irqs_stm32f439.h"
#else
#error "Unknown STM32F4 variant"
#endif
