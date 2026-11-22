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

static char *s_trace;
static size_t s_trace_len;
static size_t s_trace_cap;
static bool s_trace_on;
static char s_line[512];

static char s_last[160];
static int s_repeat;
static size_t s_last_pos;

static void prv_trace_append(const char *line) {
  if (s_trace_len > 0 && strcmp(line, s_last) == 0) {
    s_repeat++;
    s_trace_len = s_last_pos;
    char rep[192];
    snprintf(rep, sizeof(rep), "%s x%d", s_last, s_repeat);
    line = rep;
    const size_t n = strlen(line);
    if (s_trace_len + n + 2 > s_trace_cap) {
      s_trace_cap = (s_trace_cap + n + 2) * 2;
      s_trace = realloc(s_trace, s_trace_cap);
    }
    memcpy(&s_trace[s_trace_len], line, n);
    s_trace_len += n;
    s_trace[s_trace_len++] = '\n';
    s_trace[s_trace_len] = '\0';
    return;
  }
  snprintf(s_last, sizeof(s_last), "%s", line);
  s_repeat = 1;
  s_last_pos = s_trace_len;
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

static void m_line_flush(void) {
  if (s_line[0] != '\0' && s_trace_on) {
    prv_trace_append(s_line);
  }
  s_line[0] = '\0';
}

static void m_token(const char *fmt, ...) {
  char tok[16];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tok, sizeof(tok), fmt, ap);
  va_end(ap);
  cl_assert(strlen(s_line) + strlen(tok) + 2 < sizeof(s_line));
  if (s_line[0] == '\0') {
    strcpy(s_line, "i2c");
  }
  strcat(s_line, " ");
  strcat(s_line, tok);
}

static void m_trace(const char *fmt, ...) {
  m_line_flush();
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

static void m_trace_start(void) {
  m_line_flush();
  s_trace_len = 0;
  s_last[0] = '\0';
  if (s_trace != NULL) {
    s_trace[0] = '\0';
  }
  s_trace_on = true;
}

static const char *m_trace_stop(void) {
  m_line_flush();
  s_trace_on = false;
  return s_trace != NULL ? s_trace : "";
}

#define PMIC_ADDR 0x28U
#define MAG_ADDR  0x0eU

struct m_dev {
  uint8_t addr;
  bool present;
  uint8_t regs[256];
  uint8_t ptr;
  bool ptr_set;
  int nack_byte;
  int bytes;
};

static struct {
  struct m_dev pmic;
  struct m_dev mag;
  struct m_dev *dev;
  bool read;
  bool stall;
  int sda_held;
  bool busy_glitch;
  bool busy_latched;
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

static void m_devices_reset(void) {
  memset(&m.pmic, 0, sizeof(m.pmic));
  m.pmic.addr = PMIC_ADDR;
  m.pmic.present = true;
  m.pmic.regs[0x00] = 0x01; // CHIP_ID
  m.pmic.regs[0x01] = 0x42; // CHIP_REV
  m.pmic.regs[0x0a] = 0xcb; // CHG_CNTL_A
  m.pmic.regs[0x0e] = 0x12; // BUCK1_VSET
  m.pmic.regs[0x14] = 0x10; // LDO2_CONFIG
  m.pmic.regs[0x16] = 0x14; // LDO3_CONFIG
  memset(&m.mag, 0, sizeof(m.mag));
  m.mag.addr = MAG_ADDR;
  m.mag.present = true;
  m.mag.regs[0x07] = 0xc4; // WHO_AM_I
  m.dev = NULL;
  m.stall = false;
  m.sda_held = 0;
  m.busy_glitch = false;
  m.busy_latched = false;
  m.violations = 0;
  m.violation[0] = '\0';
}

static struct m_dev *prv_find(uint8_t addr) {
  if (addr == PMIC_ADDR && m.pmic.present) {
    return &m.pmic;
  }
  if (addr == MAG_ADDR && m.mag.present) {
    return &m.mag;
  }
  return NULL;
}

static bool m_address(uint8_t byte) {
  m.read = (byte & 1U) != 0U;
  m.dev = prv_find(byte >> 1);
  m_token("%02x%c%c", byte >> 1, m.read ? 'r' : 'w', m.dev != NULL ? '+' : '-');
  if (m.dev != NULL) {
    m.dev->ptr_set = m.read;
    m.dev->bytes = 0;
  }
  return m.dev != NULL;
}

static bool m_write(uint8_t byte) {
  struct m_dev *dev = m.dev;
  cl_assert(dev != NULL && !m.read);
  dev->bytes++;
  const bool ack = dev->nack_byte != dev->bytes;
  m_token("%02x%c", byte, ack ? '+' : '-');
  if (!ack) {
    return false;
  }
  if (!dev->ptr_set) {
    dev->ptr = byte;
    dev->ptr_set = true;
  } else {
    dev->regs[dev->ptr++] = byte;
  }
  return true;
}

static uint8_t m_read(bool ack) {
  struct m_dev *dev = m.dev;
  cl_assert(dev != NULL && m.read);
  const uint8_t reg = dev->ptr++;
  const uint8_t byte = dev->regs[reg];
  if (dev == &m.pmic && (reg == 0x05 || reg == 0x06)) {
    dev->regs[reg] = 0;
  }
  m_token("<%02x%c", byte, ack ? '+' : '-');
  return byte;
}

static void m_start(bool repeated) {
  m_token(repeated ? "Sr" : "S");
  m.dev = NULL;
}

static void m_stop(void) {
  m_token("P");
  m_line_flush();
  m.dev = NULL;
}
