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

static char s_last[160];
static int s_repeat;
static size_t s_last_pos;

static void prv_trace_put(const char *line) {
  const size_t n = strlen(line);

  if (s_trace_len + n + 2 > s_trace_cap) {
    s_trace_cap = (s_trace_cap + n + 2) * 2;
    s_trace = realloc(s_trace, s_trace_cap);
    cl_assert(s_trace != NULL);
  }
  memcpy(&s_trace[s_trace_len], line, n);
  s_trace_len += n;
  s_trace[s_trace_len++] = '\n';
  s_trace[s_trace_len] = '\0';
}

static void prv_trace_append(const char *line) {
  if (s_trace_len > 0 && strcmp(line, s_last) == 0) {
    char rep[192];

    s_repeat++;
    s_trace_len = s_last_pos;
    snprintf(rep, sizeof(rep), "%s x%d", s_last, s_repeat);
    prv_trace_put(rep);
    return;
  }
  snprintf(s_last, sizeof(s_last), "%s", line);
  s_repeat = 1;
  s_last_pos = s_trace_len;
  prv_trace_put(line);
}

static void m_trace_line(const char *fmt, ...) {
  char line[160];
  va_list ap;

  if (!s_trace_on) {
    return;
  }
  va_start(ap, fmt);
  const int n = vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  cl_assert(n > 0 && (size_t)n < sizeof(line) - 1);
  prv_trace_append(line);
}

static void m_trace_start(void) {
  s_trace_len = 0;
  s_last[0] = '\0';
  if (s_trace != NULL) {
    s_trace[0] = '\0';
  }
  s_trace_on = true;
}

static const char *m_trace_stop(void) {
  s_trace_on = false;
  return s_trace != NULL ? s_trace : "";
}
