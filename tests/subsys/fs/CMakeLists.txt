# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_fs
  MODULES pfs
  SOURCES
    tests/fakes/fake_rtc.c
  PLATFORMS emery
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

pbl_clar_test(test_pfs
  MODULES pfs
  SOURCES
    subsys/logging/hexdump.c
    tests/fakes/fake_rtc.c
  PLATFORMS emery
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

pbl_clar_test(test_pfs_layout
  MODULES pfs
  SOURCES
    subsys/logging/hexdump.c
    tests/fakes/fake_rtc.c
  PLATFORMS emery
  DEFINES DUMA_DISABLED CONFIG_FS_PFS_FORMAT_SECTOR_PAGES=1
  OVERRIDES dummy_board
)

pbl_clar_test(test_pfs_layout_s29vs
  SOURCE test_pfs_layout.c
  MODULES pfs
  SOURCES
    subsys/logging/hexdump.c
    tests/fakes/fake_rtc.c
  PLATFORMS emery
  DEFINES
    DUMA_DISABLED
    CONFIG_BOARD_PARTITIONS_DT=1
    CONFIG_FLASH_S29VS=1
    CONFIG_FS_PFS_PAGE_SIZE=0x2000
    CONFIG_FS_PFS_FORMAT_SECTOR_PAGES=1
    CONFIG_FS_PFS_VERSION_2=1
  OVERRIDES dummy_board snowy_partitions
)

pbl_clar_test(test_pfs_s29vs
  MODULES pfs
  SOURCES
    subsys/logging/hexdump.c
    tests/fakes/fake_rtc.c
  PLATFORMS emery
  DEFINES
    DUMA_DISABLED
    CONFIG_BOARD_PARTITIONS_DT=1
    CONFIG_FLASH_S29VS=1
    CONFIG_FS_PFS_PAGE_SIZE=0x2000
    CONFIG_FS_PFS_FORMAT_SECTOR_PAGES=1
    CONFIG_FS_PFS_VERSION_2=1
  OVERRIDES dummy_board snowy_partitions
)

pbl_clar_test(test_pfs_convert
  MODULES pfs
  SOURCES
    subsys/logging/hexdump.c
    tests/fakes/fake_rtc.c
  PLATFORMS emery
  DEFINES
    DUMA_DISABLED
    CONFIG_BOARD_PARTITIONS_DT=1
    CONFIG_FLASH_S29VS=1
    CONFIG_FS_PFS_PAGE_SIZE=0x2000
    CONFIG_FS_PFS_FORMAT_SECTOR_PAGES=1
    CONFIG_FS_PFS_CONVERT_SECTOR_PAGES=1
    CONFIG_FS_PFS_VERSION_2=1
  OVERRIDES dummy_board snowy_partitions
)
