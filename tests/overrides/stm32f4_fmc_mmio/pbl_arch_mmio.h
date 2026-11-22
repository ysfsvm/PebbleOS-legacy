/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

uint16_t fake_mmio_readw(uintptr_t addr);
void fake_mmio_writew(uint16_t val, uintptr_t addr);
uint32_t fake_mmio_readl(uintptr_t addr);
void fake_mmio_writel(uint32_t val, uintptr_t addr);

static PBL_ALWAYS_INLINE uint8_t arch_readb(uintptr_t addr) {
  return *(const volatile uint8_t *)addr;
}

static PBL_ALWAYS_INLINE uint16_t arch_readw(uintptr_t addr) {
  return fake_mmio_readw(addr);
}

static PBL_ALWAYS_INLINE void arch_writeb(uint8_t val, uintptr_t addr) {
  *(volatile uint8_t *)addr = val;
}

static PBL_ALWAYS_INLINE void arch_writew(uint16_t val, uintptr_t addr) {
  fake_mmio_writew(val, addr);
}

static PBL_ALWAYS_INLINE uint32_t arch_readl(uintptr_t addr) {
  return fake_mmio_readl(addr);
}

static PBL_ALWAYS_INLINE void arch_writel(uint32_t val, uintptr_t addr) {
  fake_mmio_writel(val, addr);
}
