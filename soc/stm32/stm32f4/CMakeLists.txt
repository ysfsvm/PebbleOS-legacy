# SPDX-FileCopyrightText: 2026 Brian Silvestri
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(.)
pbl_library_sources(
  system_stm32f4xx.c
  init.c
  clock.c
  idle.c
  sleep.c
  stuck_buttons.c
)
pbl_library_sources_ifdef(CONFIG_LEGACY_HARDLOCKUP_TEST hardlockup_test.c)
