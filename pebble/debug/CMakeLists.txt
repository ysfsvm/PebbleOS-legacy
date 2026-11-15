# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB sources CONFIGURE_DEPENDS *.c)
if(NOT CONFIG_LEGACY_PERF_PROBE)
  pbl_filter_sources(sources EXCLUDE perf_probe.c)
endif()
pbl_library_sources(${sources})
