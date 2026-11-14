# SPDX-FileCopyrightText: 2026 Brian Silvestri
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BT_CONTROLLER_CC2564)
  set(bts ${PBL_BASE}/third_party/ti_bt_sp/TIInit_6.7.16.bts)
  set_source_files_properties(cc2564_bts.c PROPERTIES
    OBJECT_DEPENDS ${bts}
    COMPILE_DEFINITIONS "CC2564_BTS_PATH=\"${bts}\"")

  pbl_library()
  pbl_library_sources(cc2564.c cc2564_bts.c)
endif()
