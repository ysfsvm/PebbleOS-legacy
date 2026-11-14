/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_LEGACY_ASSERT_CALLER_LR
#include <stdint.h>

#include <pbl/kernel/fatal.h>
#else
#include <pbl/kernel/assert.h>
#endif
#include <pbl/logging/logging.h>

void __assert_func(const char *file, int line, const char *func, const char *e) {
#ifdef CONFIG_LEGACY_ASSERT_CALLER_LR
  // Record the caller's LR, not this function's, so the reboot reason points
  // at the failed assert().
  const uintptr_t lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

#endif
  PBL_LOG_ERR("assert in %s:%d", file, line);
  PBL_LOG_ERR("%s, expr: %s", func, e);
#ifdef CONFIG_LEGACY_ASSERT_CALLER_LR
  pbl_fatal_error_handler(lr);
#else
  PBL_ASSERT(0, "libc assert()");
#endif
}
