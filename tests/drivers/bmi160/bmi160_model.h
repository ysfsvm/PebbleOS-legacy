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

#define CHIP_ID_VALUE 0xd1U
#define REG_CMD       0x7eU
#define REG_FIFO_DATA 0x24U
#define FIFO_SIZE     1024U

static char *s_trace;
static size_t s_trace_len;
static size_t s_trace_cap;
static bool s_trace_on;
static char s_reports[16384];

static char s_xfer_tx[3 * 2048];
static char s_xfer_rx[3 * 2048];
static size_t s_xfer_len;

static void prv_trace_append(const char *line) {
  const size_t n = strlen(line);
  if (s_trace_len + n + 2 > s_trace_cap) {
    s_trace_cap = (s_trace_cap + n + 2) * 2;
    s_trace = realloc(s_trace, s_trace_cap);
  }
  memcpy(&s_trace[s_trace_len], line, n);
  s_trace_len += n;
  s_trace[s_trace_len++] = '\n';
  s_trace[s_trace_len] = '\0';
}

static void m_xfer_flush(void) {
  if (s_xfer_len == 0U) {
    return;
  }
  static char line[sizeof(s_xfer_tx) + sizeof(s_xfer_rx) + 8];
  snprintf(line, sizeof(line), "x%s /%s", s_xfer_tx, s_xfer_rx);
  s_xfer_len = 0U;
  s_xfer_tx[0] = '\0';
  s_xfer_rx[0] = '\0';
  if (s_trace_on) {
    prv_trace_append(line);
  }
}

static void m_trace(const char *fmt, ...) {
  m_xfer_flush();
  if (!s_trace_on) {
    return;
  }
  char line[160];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  cl_assert(n > 0 && (size_t)n < sizeof(line) - 1);
  prv_trace_append(line);
}

static void m_report(const char *fmt, ...) {
  if (!s_trace_on) {
    return;
  }
  char line[160];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  cl_assert(n > 0 && strlen(s_reports) + (size_t)n + 2 < sizeof(s_reports));
  strcat(s_reports, line);
  strcat(s_reports, "\n");
}

static void m_trace_start(void) {
  m_xfer_flush();
  s_reports[0] = '\0';
  s_trace_len = 0;
  if (s_trace != NULL) {
    s_trace[0] = '\0';
  }
  s_trace_on = true;
}

static const char *m_trace_stop(void) {
  m_xfer_flush();
  if (s_trace_on && s_reports[0] != '\0') {
    prv_trace_append("reports:");
    s_reports[strlen(s_reports) - 1] = '\0';
    prv_trace_append(s_reports);
  }
  s_trace_on = false;
  return s_trace != NULL ? s_trace : "";
}

static struct {
  bool present;
  bool spi_mode;
  bool selected;
  bool addressed;
  bool read;
  uint8_t addr;
  uint8_t regs[128];
  uint8_t fifo[FIFO_SIZE];
  size_t fifo_len;
  size_t fifo_pos;
  int violations;
  char violation[160];
} m;

static void m_violation(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(m.violation, sizeof(m.violation), fmt, ap);
  va_end(ap);
  m.violations++;
  m_trace("VIOLATION %s", m.violation);
}

static void m_regs_reset(void) {
  memset(m.regs, 0, sizeof(m.regs));
  m.regs[0x00] = CHIP_ID_VALUE;
  m.regs[0x40] = 0x28; // ACC_CONF
  m.regs[0x41] = 0x03; // ACC_RANGE
  m.regs[0x42] = 0x28; // GYR_CONF
  m.regs[0x46] = 0x80; // FIFO_CONFIG_0
  m.regs[0x47] = 0x10; // FIFO_CONFIG_1
  m.regs[0x5f] = 0x00; // INT_MOTION_0
  m.regs[0x60] = 0x14; // INT_MOTION_1
  m.regs[0x61] = 0x0a; // INT_MOTION_2
  m.regs[0x62] = 0x14; // INT_MOTION_3
  m.regs[0x63] = 0x04; // INT_TAP_0
  m.regs[0x64] = 0xda; // INT_TAP_1
  m.fifo_len = 0;
  m.fifo_pos = 0;
}

static void m_power_on(bool present) {
  memset(&m, 0, sizeof(m));
  m.present = present;
  m_regs_reset();
}

static void m_fifo_load(const uint8_t *data, size_t len) {
  cl_assert(len <= FIFO_SIZE);
  memcpy(m.fifo, data, len);
  m.fifo_len = len;
  m.fifo_pos = 0;
}

static void prv_update_fifo_length(void) {
  const size_t left = m.fifo_len - m.fifo_pos;
  m.regs[0x22] = (uint8_t)left;
  m.regs[0x23] = (uint8_t)(left >> 8);
}

static void prv_command(uint8_t cmd) {
  if (cmd == 0xb6) {
    m_regs_reset();
    m.spi_mode = false;
  } else if ((cmd & 0xfcU) == 0x10U) {
    m.regs[0x03] = (uint8_t)((m.regs[0x03] & ~0x30U) | ((cmd & 3U) << 4));
  } else if (cmd == 0xb0) {
    m.fifo_len = 0;
    m.fifo_pos = 0;
  } else if (cmd == 0xb1) {
    m.regs[0x1c] = 0;
    m.regs[0x1d] = 0;
    m.regs[0x1e] = 0;
    m.regs[0x1f] = 0;
  } else {
    m_violation("command %02x", cmd);
  }
}

static uint8_t prv_chip_byte(uint8_t tx) {
  if (!m.present || !m.spi_mode) {
    return 0x00;
  }
  if (!m.addressed) {
    m.addressed = true;
    m.read = (tx & 0x80U) != 0;
    m.addr = tx & 0x7fU;
    return 0x00;
  }
  uint8_t rx = 0x00;
  if (m.read) {
    if (m.addr == REG_FIFO_DATA) {
      rx = (m.fifo_pos < m.fifo_len) ? m.fifo[m.fifo_pos++] : 0x80;
      prv_update_fifo_length();
      return rx;
    }
    if (m.addr == 0x22 || m.addr == 0x23) {
      prv_update_fifo_length();
    }
    rx = m.regs[m.addr];
  } else if (m.addr == REG_CMD) {
    prv_command(tx);
  } else {
    m.regs[m.addr] = tx;
  }
  m.addr = (m.addr + 1U) & 0x7fU;
  return rx;
}

static void m_chip_select(bool selected) {
  if (selected == m.selected) {
    return;
  }
  m.selected = selected;
  m.addressed = false;
  if (!selected) {
    m.spi_mode = true;
  }
}

static uint8_t m_chip_exchange(uint8_t tx) {
  uint8_t rx = m.selected ? prv_chip_byte(tx) : 0xff;
  if (!m.selected) {
    m_violation("byte %02x without chip select", tx);
  }
  if (s_xfer_len < 2048U) {
    char b[4];
    snprintf(b, sizeof(b), " %02x", tx);
    strcat(s_xfer_tx, b);
    snprintf(b, sizeof(b), " %02x", rx);
    strcat(s_xfer_rx, b);
    s_xfer_len++;
  }
  return rx;
}
