/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clar.h>

#define NOR_BASE      0x60000000U
#define NOR_SIZE      0x1000000U
#define NOR_BANK_SIZE 0x200000U
#define NOR_CR_RESET  0xDF48U

#define SR_BANK   0x01U
#define SR_SLSB   0x02U
#define SR_PSB    0x10U
#define SR_ESB    0x20U
#define SR_ESSB   0x40U
#define SR_DRB    0x80U
#define NO_SECTOR 0xFFFFFFFFU

static char *s_trace;
static size_t s_trace_len;
static size_t s_trace_cap;
static bool s_trace_on;

static void m_trace(const char *fmt, ...) {
  if (!s_trace_on) {
    return;
  }
  char line[96];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  cl_assert(n > 0 && (size_t)n < sizeof(line) - 1);
  if (s_trace_len + (size_t)n + 2 > s_trace_cap) {
    s_trace_cap = (s_trace_cap + (size_t)n + 2) * 2;
    s_trace = realloc(s_trace, s_trace_cap);
  }
  memcpy(s_trace + s_trace_len, line, (size_t)n);
  s_trace_len += (size_t)n;
  s_trace[s_trace_len++] = '\n';
  s_trace[s_trace_len] = '\0';
}

static void m_trace_start(void) {
  s_trace_len = 0;
  if (s_trace != NULL) {
    s_trace[0] = '\0';
  }
  s_trace_on = true;
}

static const char *m_trace_stop(void) {
  s_trace_on = false;
  return s_trace != NULL ? s_trace : "";
}

enum m_state {
  M_READ,
  M_STATUS,
  M_CONFIG,
  M_CONFIG_WB1,
  M_CONFIG_WB2,
  M_CONFIG_WB3,
  M_WB_COUNT,
  M_WB_DATA,
  M_WB_CONFIRM,
  M_ERASE1,
  M_LOCK1,
  M_LOCK2,
  M_RANGE2,
};

enum m_op {
  OP_NONE,
  OP_PROGRAM,
  OP_ERASE
};

static struct {
  uint16_t *mem;
  enum m_state state;
  uint16_t cr;
  uint16_t cr_pending;
  uint8_t errors;
  enum m_op op;
  uint32_t op_sector;
  int op_busy;
  bool suspended;
  uint32_t suspended_sector;
  int suspended_busy;
  bool protected_mode;
  uint32_t unlocked;
  bool range_set;
  uint32_t range_lo;
  uint32_t range_hi;
  uint32_t range_start;
  uint32_t wb_sector;
  uint32_t wb_count;
  uint32_t wb_n;
  uint32_t wb_addr[32];
  uint16_t wb_data[32];
  uint32_t erase_sector;
  int program_busy_reads;
  int erase_busy_reads;
  bool fail_next_program;
  bool fail_next_erase;
  int violations;
  char violation[96];
} m;

static void m_violation(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(m.violation, sizeof(m.violation), fmt, ap);
  va_end(ap);
  m.violations++;
  m_trace("VIOLATION %s", m.violation);
}

static void m_bus_check(uint32_t off);

static uint32_t m_sector(uint32_t off) {
  return off < 0x20000U ? (off & ~0x7FFFU) : (off & ~0x1FFFFU);
}

static uint32_t m_sector_size(uint32_t sector) {
  return sector < 0x20000U ? 0x8000U : 0x20000U;
}

static uint32_t m_bank(uint32_t off) {
  return off / NOR_BANK_SIZE;
}

static bool m_busy(void) {
  return m.op_busy > 0;
}

static bool m_locked(uint32_t sector) {
  if (m.range_set && sector >= m.range_lo && sector <= m.range_hi) {
    return true;
  }
  return m.protected_mode && sector != m.unlocked;
}

static void m_hw_reset(void) {
  m.state = M_READ;
  m.cr = NOR_CR_RESET;
  m.errors = 0;
  m.op = OP_NONE;
  m.op_busy = 0;
  m.suspended = false;
  m.range_set = false;
}

static void m_power_on(void) {
  if (m.mem == NULL) {
    m.mem = malloc(NOR_SIZE);
  }
  memset(m.mem, 0xFF, NOR_SIZE);
  uint16_t *mem = m.mem;
  memset(&m, 0, sizeof(m));
  m.mem = mem;
  m.unlocked = NO_SECTOR;
  m.program_busy_reads = 1;
  m.erase_busy_reads = 3;
  m_hw_reset();
}

static void m_complete(void) {
  if (m.op == OP_ERASE) {
    if (m.fail_next_erase) {
      m.fail_next_erase = false;
      m.errors |= SR_ESB;
    } else {
      memset(&m.mem[m.op_sector / 2U], 0xFF, m_sector_size(m.op_sector));
    }
  } else if (m.op == OP_PROGRAM) {
    if (m.fail_next_program) {
      m.fail_next_program = false;
      m.errors |= SR_PSB;
    } else {
      for (uint32_t i = 0; i < m.wb_n; i++) {
        m.mem[m.wb_addr[i] / 2U] &= m.wb_data[i];
      }
    }
  }
  m.op = OP_NONE;
}

static uint8_t m_status(uint32_t off) {
  if (m.op_busy > 0) {
    const uint8_t sr = (m_bank(off) != m_bank(m.op_sector)) ? SR_BANK : 0U;
    if (--m.op_busy == 0) {
      m_complete();
    }
    return sr;
  }
  return SR_DRB | m.errors | (m.suspended ? SR_ESSB : 0U);
}

static bool m_command_refused(uint32_t off, uint16_t val) {
  if (val != 0x25 && val != 0x80 && val != 0xD0) {
    return false;
  }
  if (m_busy()) {
    return true;
  }
  return m.suspended && (val != 0x25 || m_sector(off) == m.suspended_sector);
}

static void m_command(uint32_t off, uint16_t val) {
  if ((off & 0xFFFU) != 0xAAAU) {
    m_violation("write %04x at %06x in read mode", val, off);
    return;
  }
  if (m_command_refused(off, val)) {
    m_violation("command %02x at %06x while %s", val, off, m_busy() ? "busy" : "suspended");
    return;
  }
  switch (val) {
    case 0xF0:
      m.state = M_READ;
      break;
    case 0x70:
      m.state = M_STATUS;
      break;
    case 0x71:
      if (!m_busy()) {
        m.errors = 0;
      }
      m.state = M_READ;
      break;
    case 0x25:
      m.wb_sector = m_sector(off);
      m.state = M_WB_COUNT;
      break;
    case 0x80:
      m.erase_sector = m_sector(off);
      m.state = M_ERASE1;
      break;
    case 0x60:
      m.state = M_LOCK1;
      break;
    case 0xB0:
      if (m.op == OP_ERASE && m.op_busy > 0 && m_sector(off) == m.op_sector) {
        m.suspended = true;
        m.suspended_sector = m.op_sector;
        m.suspended_busy = m.op_busy;
        m.op = OP_NONE;
        m.op_busy = 0;
      }
      m.state = M_READ;
      break;
    case 0x30:
      if (m.suspended && m_sector(off) == m.suspended_sector) {
        m.suspended = false;
        m.op = OP_ERASE;
        m.op_sector = m.suspended_sector;
        m.op_busy = m.suspended_busy;
      }
      m.state = M_READ;
      break;
    case 0xD0:
      m.state = M_CONFIG;
      break;
    default:
      m_violation("command %02x at %06x", val, off);
      break;
  }
}

static void m_start_program(void) {
  const uint32_t sector = m.wb_sector;
  if (m_locked(sector)) {
    m.errors |= SR_SLSB;
    return;
  }
  m.op = OP_PROGRAM;
  m.op_sector = sector;
  m.op_busy = m.program_busy_reads;
  if (m.op_busy == 0) {
    m_complete();
  }
}

static void m_start_erase(void) {
  const uint32_t sector = m.erase_sector;
  if (m_locked(sector)) {
    m.errors |= SR_SLSB;
    return;
  }
  m.op = OP_ERASE;
  m.op_sector = sector;
  m.op_busy = m.erase_busy_reads;
}

static void m_write(uint32_t off, uint16_t val) {
  const uint32_t low = off & 0xFFFU;

  switch (m.state) {
    case M_READ:
    case M_STATUS:
      m_command(off, val);
      break;
    case M_CONFIG:
      if (low == 0xAAAU && val == 0xF0) {
        m.state = M_READ;
      } else if (low == 0xAAAU && val == 0x25) {
        m.state = M_CONFIG_WB1;
      } else {
        m_violation("write %04x at %06x in configuration mode", val, off);
      }
      break;
    case M_CONFIG_WB1:
      if (low != 0x554U || val != 0) {
        m_violation("configuration write cycle 2: %04x at %06x", val, off);
      }
      m.state = M_CONFIG_WB2;
      break;
    case M_CONFIG_WB2:
      if (off != 0) {
        m_violation("configuration write cycle 3 at %06x", off);
      }
      m.cr_pending = val;
      m.state = M_CONFIG_WB3;
      break;
    case M_CONFIG_WB3:
      if (low != 0xAAAU || val != 0x29) {
        m_violation("configuration write cycle 4: %04x at %06x", val, off);
      }
      m.cr = m.cr_pending;
      m.state = M_CONFIG;
      break;
    case M_WB_COUNT:
      if (low != 0x554U || m_sector(off) != m.wb_sector || val >= 32U) {
        m_violation("write buffer count %04x at %06x", val, off);
        m.state = M_READ;
        break;
      }
      m.wb_count = val + 1U;
      m.wb_n = 0;
      m.state = M_WB_DATA;
      break;
    case M_WB_DATA:
      if (m.wb_n > 0 && (off & ~0x3FU) != (m.wb_addr[0] & ~0x3FU)) {
        m_violation("write buffer data at %06x outside the page of %06x", off, m.wb_addr[0]);
      }
      if (m_sector(off) != m.wb_sector) {
        m_violation("write buffer data at %06x outside sector %06x", off, m.wb_sector);
      }
      if (m.wb_n > 0 && off <= m.wb_addr[m.wb_n - 1]) {
        m_violation("write buffer data at %06x out of order", off);
      }
      m.wb_addr[m.wb_n] = off;
      m.wb_data[m.wb_n] = val;
      if (++m.wb_n == m.wb_count) {
        m.state = M_WB_CONFIRM;
      }
      break;
    case M_WB_CONFIRM:
      if (off != m.wb_sector + 0xAAAU || val != 0x29) {
        m_violation("write buffer confirm %04x at %06x", val, off);
      } else {
        m_start_program();
      }
      m.state = M_READ;
      break;
    case M_ERASE1:
      if (low != 0x554U || m_sector(off) != m.erase_sector || val != 0x30) {
        m_violation("erase cycle 2: %04x at %06x", val, off);
      } else {
        m_start_erase();
      }
      m.state = M_READ;
      break;
    case M_LOCK1:
      if (low != 0x554U || val != 0x60) {
        m_violation("lock cycle 2: %04x at %06x", val, off);
      }
      m.state = M_LOCK2;
      break;
    case M_LOCK2:
      if (val == 0x60) {
        const uint32_t sector = m_sector(off);
        m.protected_mode = true;
        if (off & 0x80U) {
          m.unlocked = sector;
        } else if (m.unlocked == sector) {
          m.unlocked = NO_SECTOR;
        }
        m.state = M_READ;
      } else if (val == 0x61) {
        if (off & 0x80U) {
          m_violation("lock range cycle 3 with A6 set at %06x", off);
        }
        m.range_start = off & ~0x1FFFFU;
        m.state = M_RANGE2;
      } else {
        m_violation("lock cycle 3: %04x at %06x", val, off);
        m.state = M_READ;
      }
      break;
    case M_RANGE2:
      if (val != 0x61 || (off & 0x80U) || (off & ~0x1FFFFU) < m.range_start) {
        m_violation("lock range cycle 4: %04x at %06x", val, off);
      } else if (!m.range_set) {
        m.range_set = true;
        m.range_lo = m.range_start;
        m.range_hi = off & ~0x1FFFFU;
      }
      m.state = M_READ;
      break;
  }
}

static uint16_t m_read(uint32_t off) {
  switch (m.state) {
    case M_STATUS:
      m.state = M_READ;
      return m_status(off);
    case M_CONFIG:
      return m.cr;
    case M_READ:
      if (m_busy() && m_bank(off) == m_bank(m.op_sector)) {
        m_violation("array read at %06x in a busy bank", off);
      } else if (m.suspended && m_sector(off) == m.suspended_sector) {
        m_violation("array read at %06x in the erase-suspended sector", off);
      }
      return m.mem[off / 2U];
    default:
      m_violation("read at %06x in the middle of a command", off);
      return 0xFFFF;
  }
}

uint16_t fake_mmio_readw(uintptr_t addr) {
  cl_assert(addr >= NOR_BASE && addr < NOR_BASE + NOR_SIZE && (addr & 1U) == 0);
  const uint32_t off = (uint32_t)(addr - NOR_BASE);
  m_bus_check(off);
  const uint16_t val = m_read(off);
  m_trace("R %06x %04x", off, val);
  return val;
}

void fake_mmio_writew(uint16_t val, uintptr_t addr) {
  cl_assert(addr >= NOR_BASE && addr < NOR_BASE + NOR_SIZE && (addr & 1U) == 0);
  const uint32_t off = (uint32_t)(addr - NOR_BASE);
  m_bus_check(off);
  m_trace("W %06x %04x", off, val);
  m_write(off, val);
}
