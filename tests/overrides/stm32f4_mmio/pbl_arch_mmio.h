/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

//! 32-bit register accesses go to the test, which models the peripheral's registers. Byte and
//! halfword accesses go to memory, as on the host by default.

uint32_t fake_mmio_readl(uintptr_t addr);
void fake_mmio_writel(uint32_t val, uintptr_t addr);

static PBL_ALWAYS_INLINE uint8_t arch_readb(uintptr_t addr) {
  return *(const volatile uint8_t *)addr;
}

static PBL_ALWAYS_INLINE uint16_t arch_readw(uintptr_t addr) {
  return *(const volatile uint16_t *)addr;
}

static PBL_ALWAYS_INLINE void arch_writeb(uint8_t val, uintptr_t addr) {
  *(volatile uint8_t *)addr = val;
}

static PBL_ALWAYS_INLINE void arch_writew(uint16_t val, uintptr_t addr) {
  *(volatile uint16_t *)addr = val;
}

static PBL_ALWAYS_INLINE uint32_t arch_readl(uintptr_t addr) {
  return fake_mmio_readl(addr);
}

static PBL_ALWAYS_INLINE void arch_writel(uint32_t val, uintptr_t addr) {
  fake_mmio_writel(val, addr);
}
