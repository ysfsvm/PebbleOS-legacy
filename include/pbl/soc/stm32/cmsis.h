/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! The STM32F4 device header, without its RTC and FLASH instance macros: RTC and FLASH are the
//! board's devices (pbl/drivers/rtc.h, pbl/drivers/flash.h). Include this, not <stm32f4xx.h>.

#include <stm32f4xx.h>

#undef RTC
#undef FLASH

//! The embedded flash interface registers, which <stm32f4xx.h> calls FLASH.
#define STM32_FLASH_IF ((FLASH_TypeDef *)FLASH_R_BASE)
