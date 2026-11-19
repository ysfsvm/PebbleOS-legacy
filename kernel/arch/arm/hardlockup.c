/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

//! The hard lockup detector's timer on Cortex-M: SysTick, at the most urgent priority, so that
//! the kernel's interrupt lock (BASEPRI) and every interrupt handler leave it running. It counts
//! CPU cycles, stops in deep sleep, and is stopped while the CPU idles, which it would otherwise
//! wake up every period (and make each STOP mode entry it lands in return at once). The system
//! timer must not be SysTick.

#include "arm_cpu.h"

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/time.h>
#include <pbl/task_wdt/arch.h>
#include <pbl/util/bits.h>

#define SYST_CSR 0xE000E010UL
#define SYST_RVR 0xE000E014UL
#define SYST_CVR 0xE000E018UL

#define SYST_CSR_ENABLE    PBL_BIT(0)
#define SYST_CSR_TICKINT   PBL_BIT(1)
#define SYST_CSR_CLKSOURCE PBL_BIT(2)

#define SYST_RVR_MAX PBL_BIT(24)

#define ARM_EXC_SYSTICK 15U

// The stacked frame: r0-r3, r12, lr, pc, xpsr
#define FRAME_LR   5
#define FRAME_PC   6
#define FRAME_XPSR 7
#define IPSR_MASK  0x1ffU

uint32_t arch_hardlockup_start(uint32_t period_ms) {
  const uint32_t hz = pbl_cycles_per_sec();
  uint64_t cycles = (uint64_t)hz * period_ms / 1000U;

  if (cycles > SYST_RVR_MAX) {
    cycles = SYST_RVR_MAX;
  } else if (cycles == 0U) {
    cycles = 1U;
  }

  arch_writel(0U, SYST_CSR);
  arm_shpr_set(ARM_EXC_SYSTICK, 0U);
  arch_writel((uint32_t)cycles - 1U, SYST_RVR);
  arch_writel(0U, SYST_CVR);
  arch_writel(SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE, SYST_CSR);

  const uint32_t started_ms = (uint32_t)(cycles * 1000U / hz);
  return started_ms > 0U ? started_ms : 1U;
}

static bool s_idle_stopped;

// With PRIMASK set, so SysTick cannot be taken in between
void arm_hardlockup_idle_enter(void) {
  const uint32_t csr = arch_readl(SYST_CSR);

  if ((csr & SYST_CSR_ENABLE) == 0U) {
    return;
  }
  arch_writel(csr & ~SYST_CSR_ENABLE, SYST_CSR);
  // A period that ended just now would end the wait at once
  arch_writel(ARM_SCB_ICSR_PENDSTCLR, ARM_SCB_ICSR);
  s_idle_stopped = true;
}

void arm_hardlockup_idle_exit(void) {
  if (!s_idle_stopped) {
    return;
  }
  s_idle_stopped = false;
  arch_writel(arch_readl(SYST_CSR) | SYST_CSR_ENABLE, SYST_CSR);
}

PBL_EXTERNALLY_VISIBLE void arch_hardlockup_tick_c(const uint32_t *frame) {
  pbl_task_wdt_hardlockup_tick(frame[FRAME_PC], frame[FRAME_LR], frame[FRAME_XPSR] & IPSR_MASK);
}

//! The interrupted code's frame is on the stack its EXC_RETURN names: the process stack for a
//! thread, the main stack for an exception handler.
void PBL_NAKED SysTick_Handler(void) {
  __asm volatile(
      "  tst lr, #4                 \n"
      "  ite eq                     \n"
      "  mrseq r0, msp              \n"
      "  mrsne r0, psp              \n"
      "  b arch_hardlockup_tick_c   \n");
}
