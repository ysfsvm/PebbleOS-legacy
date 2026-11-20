/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <clar.h>

#define M_IWDG_KR  0x40003000U
#define M_IWDG_PR  0x40003004U
#define M_IWDG_RLR 0x40003008U
#define M_IWDG_SR  0x4000300cU
#define M_RCC_CSR  0x40023874U
#define M_APB1FZ   0xe0042008U

#define M_CSR_LSION (1U << 0)
#define M_CSR_RMVF  (1U << 24)
#define M_CSR_FLAGS (0x7fU << 25)

struct m_access {
  bool write;
  uintptr_t addr;
  uint32_t val;
};

static struct m_access s_log[64];
static int s_log_len;

static struct {
  bool unlocked;
  bool running;
  uint32_t pr;
  uint32_t rlr;
  uint32_t reloads;
  uint32_t csr;
  uint32_t apb1fz;
} s_m;

static void m_log(bool write, uintptr_t addr, uint32_t val) {
  cl_assert(s_log_len < 64);
  s_log[s_log_len++] = (struct m_access){write, addr, val};
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  uint32_t val;

  switch (addr) {
    case M_RCC_CSR:
      val = s_m.csr;
      break;
    case M_APB1FZ:
      val = s_m.apb1fz;
      break;
    default:
      cl_fail("read of an unexpected register");
      val = 0U;
  }
  m_log(false, addr, val);
  return val;
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  m_log(true, addr, val);
  switch (addr) {
    case M_IWDG_KR:
      s_m.unlocked = (val == 0x5555U);
      if (val == 0xccccU) {
        s_m.running = true;
      } else if (val == 0xaaaaU) {
        s_m.reloads++;
      }
      break;
    case M_IWDG_PR:
      cl_assert_(s_m.unlocked, "PR written while locked");
      s_m.pr = val;
      break;
    case M_IWDG_RLR:
      cl_assert_(s_m.unlocked, "RLR written while locked");
      s_m.rlr = val;
      break;
    case M_RCC_CSR:
      cl_assert_(((s_m.csr ^ val) & ~(M_CSR_RMVF | M_CSR_FLAGS)) == 0U,
                 "CSR bit other than RMVF changed");
      if ((val & M_CSR_RMVF) != 0U) {
        s_m.csr &= ~M_CSR_FLAGS;
      }
      break;
    case M_APB1FZ:
      s_m.apb1fz = val;
      break;
    default:
      cl_fail("write to an unexpected register");
  }
}

static void m_reset(void) {
  memset(&s_m, 0, sizeof(s_m));
  s_m.csr = M_CSR_LSION | (1U << 1);
  s_log_len = 0;
}
