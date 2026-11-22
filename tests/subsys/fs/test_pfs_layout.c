/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <string.h>

#include <pbl/board/partitions.h>
#include <pbl/crc/crc.h>
#include <pbl/drivers/flash.h>

#include <clar.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fs/pfs/flash_translation.h>
#include <fs/pfs/pfs.h>

#ifdef CONFIG_FS_PFS_PAGE_SIZE
#define TEST_PAGE_SIZE CONFIG_FS_PFS_PAGE_SIZE
#else
#define TEST_PAGE_SIZE SUBSECTOR_SIZE_BYTES
#endif

#define TEST_PFS_VERSION  0x5001
#define TEST_FLAGS_ERASED 0xfe
#define TEST_FLAGS_START  0xfa

#ifdef CONFIG_FS_PFS_VERSION_2
#define TEST_FORMAT_VERSION 0x5002
#else
#define TEST_FORMAT_VERSION TEST_PFS_VERSION
#endif

typedef struct __attribute__((packed)) {
  uint16_t version;
  uint8_t last_written;
  uint8_t page_flags;
  uint8_t rsvd0[4];
  uint32_t erase_count;
  uint8_t rsvd1[9];
  uint8_t next_page_crc;
  uint16_t next_page;
  uint32_t hdr_crc;
} TestPageHeader;

typedef struct __attribute__((packed)) {
  uint32_t file_size;
  uint8_t file_type;
  uint8_t file_namelen;
  uint8_t rsvd[6];
  uint32_t hdr_crc;
} TestFileHeader;

typedef struct __attribute__((packed)) {
  uint16_t tmp_state;
  uint16_t create_state;
  uint16_t delete_state;
  uint8_t rsvd[10];
  uint8_t uuid[16];
} TestFileMetaData;

extern void pfs_reset_all_state(void);

static void prv_raw_write(uint32_t offs, const void *buf, size_t len) {
  pbl_flash_write(FLASH, PART_FILESYSTEM_OFFSET + offs, buf, len);
}

static void prv_raw_read(uint32_t offs, void *buf, size_t len) {
  pbl_flash_read(FLASH, PART_FILESYSTEM_OFFSET + offs, buf, len);
}

static void prv_write_page_header(uint32_t offs, uint8_t flags, bool deleted) {
  TestPageHeader hdr;
  memset(&hdr, 0xff, sizeof(hdr));
  hdr.version = TEST_PFS_VERSION;
  hdr.page_flags = flags;
  hdr.erase_count = 3;
  hdr.hdr_crc = pbl_crc32_legacy(&hdr, offsetof(TestPageHeader, hdr_crc));
  if (deleted) {
    hdr.page_flags &= ~0x2;
  }
  prv_raw_write(offs, &hdr, sizeof(hdr));
}

static void prv_write_erased_header(uint32_t offs) {
  TestPageHeader hdr;
  memset(&hdr, 0xff, sizeof(hdr));
  hdr.version = TEST_PFS_VERSION;
  hdr.page_flags = TEST_FLAGS_ERASED;
  hdr.erase_count = 3;
  prv_raw_write(offs, &hdr, offsetof(TestPageHeader, rsvd1));
}

static void prv_write_sector_page_file(uint32_t sector, const char *name, bool deleted) {
  const uint32_t offs = sector * SECTOR_SIZE_BYTES;
  const size_t namelen = strlen(name);
  const uint32_t data_offs =
      sizeof(TestPageHeader) + sizeof(TestFileHeader) + sizeof(TestFileMetaData) + namelen;

  prv_write_page_header(offs, TEST_FLAGS_START, deleted);

  TestFileHeader fhdr = {
    .file_size = SECTOR_SIZE_BYTES - data_offs,
    .file_type = FILE_TYPE_STATIC,
    .file_namelen = namelen,
  };
  memset(fhdr.rsvd, 0xff, sizeof(fhdr.rsvd));
  fhdr.hdr_crc = pbl_crc32_legacy(&fhdr, offsetof(TestFileHeader, hdr_crc));
  prv_raw_write(offs + sizeof(TestPageHeader), &fhdr, sizeof(fhdr));

  TestFileMetaData meta;
  memset(&meta, 0xff, sizeof(meta));
  meta.tmp_state = 0;
  meta.create_state = 0;
  if (deleted) {
    meta.delete_state = 0;
  }
  prv_raw_write(offs + sizeof(TestPageHeader) + sizeof(TestFileHeader), &meta, sizeof(meta));
  prv_raw_write(offs + sizeof(TestPageHeader) + sizeof(fhdr) + sizeof(meta), name, namelen);

  static uint8_t data[SECTOR_SIZE_BYTES];
  for (uint32_t i = 0; i < sizeof(data); i++) {
    data[i] = (uint8_t)(i * 31 + sector);
  }
  prv_raw_write(offs + data_offs, data, SECTOR_SIZE_BYTES - data_offs);
}

static void prv_write_chance_magic(uint32_t offs, uint8_t flags) {
  const uint8_t junk[sizeof(TestPageHeader)] = {0x01, 0x50, 0xff, flags, 0x00, 0x11};
  prv_raw_write(offs, junk, sizeof(junk));
}

static bool prv_page_is_erased_header(uint32_t page) {
  TestPageHeader hdr;
  prv_raw_read(page * TEST_PAGE_SIZE, &hdr, sizeof(hdr));
  return (hdr.version == TEST_FORMAT_VERSION) && (hdr.page_flags == TEST_FLAGS_ERASED);
}

static void prv_create_file(const char *name, uint8_t fill, size_t len) {
  static uint8_t buf[3 * SECTOR_SIZE_BYTES];
  cl_assert(len <= sizeof(buf));
  memset(buf, fill, len);
  int fd = pfs_open(name, OP_FLAG_WRITE, FILE_TYPE_STATIC, len);
  cl_assert(fd >= 0);
  cl_assert_equal_i(pfs_write(fd, buf, len), len);
  cl_assert_equal_i(pfs_close(fd), S_SUCCESS);
}

static void prv_assert_file(const char *name, uint8_t fill, size_t len) {
  static uint8_t buf[3 * SECTOR_SIZE_BYTES];
  cl_assert(len <= sizeof(buf));
  int fd = pfs_open(name, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  cl_assert(fd >= 0);
  cl_assert_equal_i(pfs_get_file_size(fd), len);
  cl_assert_equal_i(pfs_read(fd, buf, len), len);
  for (size_t i = 0; i < len; i++) {
    cl_assert_equal_i(buf[i], fill);
  }
  cl_assert_equal_i(pfs_close(fd), S_SUCCESS);
}

static void prv_reboot(void) {
  pfs_reset_all_state();
  pfs_init(true);
}

void test_pfs_layout__initialize(void) {
  fake_spi_flash_init(0, PART_FILESYSTEM_OFFSET + PART_FILESYSTEM_SIZE);
  pfs_reset_all_state();
}

void test_pfs_layout__cleanup(void) {
  fake_spi_flash_cleanup();
}

void test_pfs_layout__sector_pages_are_formatted(void) {
  prv_write_sector_page_file(0, "old0", false);
  prv_write_erased_header(1 * SECTOR_SIZE_BYTES);
  prv_write_sector_page_file(2, "old2", true);
  prv_write_sector_page_file(3, "old3", false);
  prv_write_chance_magic(3 * SECTOR_SIZE_BYTES + TEST_PAGE_SIZE, TEST_FLAGS_START);
  prv_write_chance_magic(4 * SECTOR_SIZE_BYTES + TEST_PAGE_SIZE, TEST_FLAGS_START);
  prv_write_chance_magic(4 * SECTOR_SIZE_BYTES + 2 * TEST_PAGE_SIZE, TEST_FLAGS_ERASED);
  prv_write_erased_header(5 * SECTOR_SIZE_BYTES);

  pfs_init(true);

  cl_assert_equal_i(pfs_open("old0", OP_FLAG_READ, 0, 0), E_DOES_NOT_EXIST);
  cl_assert_equal_i(pfs_open("old3", OP_FLAG_READ, 0, 0), E_DOES_NOT_EXIST);
  for (uint32_t page = 0; page < PART_FILESYSTEM_SIZE / TEST_PAGE_SIZE; page++) {
    cl_assert(prv_page_is_erased_header(page));
  }
  cl_assert_equal_i(get_available_pfs_space(), (pfs_get_size() * 8) / 10);
}

void test_pfs_layout__sector_pages_are_formatted_after_a_cut(void) {
  for (uint32_t sector = 0; sector < 8; sector++) {
    prv_write_sector_page_file(sector, "old", false);
  }
  pbl_flash_erase(FLASH, PART_FILESYSTEM_OFFSET, 4 * SECTOR_SIZE_BYTES);

  pfs_init(true);

  for (uint32_t page = 0; page < PART_FILESYSTEM_SIZE / TEST_PAGE_SIZE; page++) {
    cl_assert(prv_page_is_erased_header(page));
  }
}

void test_pfs_layout__blank_flash_is_formatted(void) {
  pfs_init(true);

  cl_assert(pfs_active());
  prv_create_file("a", 0x5a, 100);
  prv_reboot();
  prv_assert_file("a", 0x5a, 100);
}

void test_pfs_layout__small_pages_are_kept(void) {
  pfs_init(true);
  prv_create_file("a", 0x11, 3 * TEST_PAGE_SIZE);
  prv_create_file("b", 0x22, 10);
  prv_create_file("c", 0x33, 2 * SECTOR_SIZE_BYTES);
  cl_assert_equal_i(pfs_remove("b"), S_SUCCESS);

  prv_reboot();

  prv_assert_file("a", 0x11, 3 * TEST_PAGE_SIZE);
  prv_assert_file("c", 0x33, 2 * SECTOR_SIZE_BYTES);
  cl_assert_equal_i(pfs_open("b", OP_FLAG_READ, 0, 0), E_DOES_NOT_EXIST);
}

extern uint16_t test_get_file_start_page(int fd);

void test_pfs_layout__one_sector_of_small_pages_is_kept(void) {
  pfs_init(true);
  prv_create_file("a", 0x44, 2 * TEST_PAGE_SIZE);
  int fd = pfs_open("a", OP_FLAG_READ, 0, 0);
  const uint32_t keep = (test_get_file_start_page(fd) * TEST_PAGE_SIZE) / SECTOR_SIZE_BYTES;
  pfs_close(fd);

  for (uint32_t sector = 0; sector < PART_FILESYSTEM_SIZE / SECTOR_SIZE_BYTES; sector++) {
    if (sector == keep) {
      continue;
    }
    pbl_flash_erase(FLASH, PART_FILESYSTEM_OFFSET + sector * SECTOR_SIZE_BYTES, SECTOR_SIZE_BYTES);
    prv_write_erased_header(sector * SECTOR_SIZE_BYTES);
  }

  prv_reboot();

  prv_assert_file("a", 0x44, 2 * TEST_PAGE_SIZE);
}

void test_pfs_layout__deleted_pages_inside_sectors_are_kept(void) {
  const uint32_t pages_per_sector = SECTOR_SIZE_BYTES / TEST_PAGE_SIZE;
  const uint32_t num_pages = PART_FILESYSTEM_SIZE / TEST_PAGE_SIZE;

  for (uint32_t page = 0; page < num_pages; page++) {
    if ((page % pages_per_sector) == 0) {
      prv_write_erased_header(page * TEST_PAGE_SIZE);
    } else {
      prv_write_page_header(page * TEST_PAGE_SIZE, TEST_FLAGS_START, true);
    }
  }

  pfs_init(true);

  for (uint32_t page = pages_per_sector; page < num_pages; page++) {
    if ((page % pages_per_sector) != 0) {
      cl_assert(!prv_page_is_erased_header(page));
    }
  }
}
