/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/board/partitions.h>
#include <pbl/crc/crc.h>
#include <pbl/drivers/flash.h>
#include <pbl/util/math.h>

#include <clar.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fs/pfs/flash_translation.h>
#include <fs/pfs/pfs.h>

#define PAGE_SIZE        CONFIG_FS_PFS_PAGE_SIZE
#define HDR_SIZE         28
#define FILEDATA_LEN     (16 + 32)
#define OLD_PAYLOAD      (SECTOR_SIZE_BYTES - HDR_SIZE)
#define NEW_PAYLOAD      (PAGE_SIZE - HDR_SIZE)
#define NUM_SECTORS      (PART_FILESYSTEM_SIZE / SECTOR_SIZE_BYTES)
#define NUM_PAGES        (PART_FILESYSTEM_SIZE / PAGE_SIZE)
#define PAGES_PER_SECTOR (SECTOR_SIZE_BYTES / PAGE_SIZE)
#define NO_PAGE          0xffff
#define FIRST_VERSION    0x5001
#define NEW_VERSION      0x5002

#define FLAGS_ERASED 0xfe
#define FLAGS_START  0xfa
#define FLAGS_CONT   0xf6
#define FLAG_DELETED 0x02

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

enum {
  OLD_TMP = 1 << 0,
  OLD_INCOMPLETE = 1 << 1,
  OLD_DELETED = 1 << 2,
  OLD_HALF_DELETED = 1 << 3,
  OLD_BAD_CRC = 1 << 4,
  OLD_BAD_CHAIN = 1 << 5,
  OLD_TAGGED = 1 << 6,
  OLD_DROPPED = 1 << 7,
};

#define OLD_GONE (OLD_TMP | OLD_INCOMPLETE | OLD_DELETED | OLD_HALF_DELETED | OLD_BAD_CRC)

typedef struct {
  char name[40];
  uint32_t size;
  uint8_t seed;
  bool kept;
  uint8_t *data;
} TestFile;

#define SMALL_FILES 500

static TestFile s_files[SMALL_FILES];
static int s_num_files;
static bool s_used[NUM_SECTORS];
static uint32_t s_next_sector;
static uint8_t *s_image;
static uint8_t *s_stream;

extern void pfs_reset_all_state(void);
extern void test_override_convert_max_steps(uint32_t max_steps);
extern bool test_convert_needed(void);
extern void test_force_garbage_collection(uint16_t start_page);
extern void test_rerun_reboot_cleanup(void);
extern void test_skip_first_version_rewrite(bool skip);

static uint8_t prv_byte(uint8_t seed, uint32_t i) {
  if ((((i / 3000) + seed) % 4) == 0) {
    return 0xff;
  }
  return (uint8_t)(seed * 29 + i * 7 + (i >> 11));
}

static void prv_raw_write(uint32_t offs, const void *buf, size_t len) {
  pbl_flash_write(FLASH, PART_FILESYSTEM_OFFSET + offs, buf, len);
}

static void prv_raw_read(uint32_t offs, void *buf, size_t len) {
  pbl_flash_read(FLASH, PART_FILESYSTEM_OFFSET + offs, buf, len);
}

static void prv_page_header(uint32_t offs, uint32_t erase_count, uint8_t flags, uint16_t next,
                            unsigned opts) {
  TestPageHeader hdr;
  memset(&hdr, 0xff, sizeof(hdr));
  prv_raw_read(offs + offsetof(TestPageHeader, erase_count), &hdr.erase_count,
               sizeof(hdr.erase_count));
  if (hdr.erase_count == 0xffffffff) {
    hdr.erase_count = erase_count;
  }
  hdr.version = FIRST_VERSION;
  hdr.page_flags = flags;
  hdr.next_page = next;
  if (next != NO_PAGE) {
    hdr.next_page_crc = pbl_crc8_reversed(0, &next, sizeof(next));
    if ((opts & OLD_BAD_CHAIN) != 0) {
      hdr.next_page_crc ^= 0x5a;
    }
  }
  hdr.hdr_crc = pbl_crc32_legacy(&hdr, offsetof(TestPageHeader, hdr_crc));
  if ((opts & OLD_BAD_CRC) != 0) {
    hdr.hdr_crc ^= 1;
  }
  if ((opts & OLD_TAGGED) != 0) {
    hdr.last_written = 0xfe;
  }
  prv_raw_write(offs, &hdr, sizeof(hdr));
}

static void prv_old_header(uint16_t sector, uint8_t flags, uint16_t next, unsigned opts) {
  prv_page_header(sector * SECTOR_SIZE_BYTES, 1 + (sector % 13), flags, next, opts);
}

static void prv_deleted_at(uint32_t offs, uint8_t flags) {
  const uint8_t deleted = flags & ~FLAG_DELETED;
  prv_raw_write(offs + offsetof(TestPageHeader, page_flags), &deleted, 1);
}

static void prv_old_deleted(uint16_t sector, uint8_t flags) {
  prv_deleted_at(sector * SECTOR_SIZE_BYTES, flags);
}

static void prv_erased_header_at(uint32_t offs, uint32_t erase_count) {
  TestPageHeader hdr;
  memset(&hdr, 0xff, sizeof(hdr));
  hdr.version = FIRST_VERSION;
  hdr.page_flags = FLAGS_ERASED;
  hdr.erase_count = erase_count;
  prv_raw_write(offs, &hdr, offsetof(TestPageHeader, rsvd1));
}

static void prv_old_erased(uint16_t sector) {
  s_used[sector] = true;
  prv_erased_header_at(sector * SECTOR_SIZE_BYTES, 7);
}

static uint16_t prv_alloc_sector(void) {
  do {
    s_next_sector = (s_next_sector + 37) % NUM_SECTORS;
  } while (s_used[s_next_sector]);
  s_used[s_next_sector] = true;
  return s_next_sector;
}

static uint32_t prv_old_sectors(const char *name, uint32_t size) {
  return DIVIDE_CEIL(FILEDATA_LEN + strlen(name) + size, OLD_PAYLOAD);
}

static TestFile *prv_add_file(const char *name, uint32_t size, uint8_t seed, unsigned opts) {
  TestFile *f = &s_files[s_num_files++];
  snprintf(f->name, sizeof(f->name), "%s", name);
  f->size = size;
  f->seed = seed;
  f->kept = (opts & (OLD_GONE | OLD_BAD_CHAIN | OLD_DROPPED)) == 0;
  f->data = nullptr;
  return f;
}

static uint32_t prv_stream(const char *name, uint32_t size, uint8_t seed, unsigned opts) {
  const size_t namelen = strlen(name);
  TestFileHeader fh;
  memset(&fh, 0xff, sizeof(fh));
  fh.file_size = size;
  fh.file_type = FILE_TYPE_STATIC;
  fh.file_namelen = namelen;
  fh.hdr_crc = pbl_crc32_legacy(&fh, offsetof(TestFileHeader, hdr_crc));

  TestFileMetaData meta;
  memset(&meta, 0xff, sizeof(meta));
  meta.tmp_state = ((opts & OLD_TMP) != 0) ? 0xffff : 0;
  meta.create_state = ((opts & OLD_INCOMPLETE) != 0) ? 0xffff : 0;
  meta.delete_state = ((opts & OLD_DELETED) != 0) ? 0 : 0xffff;

  memcpy(s_stream, &fh, sizeof(fh));
  memcpy(&s_stream[sizeof(fh)], &meta, sizeof(meta));
  memcpy(&s_stream[FILEDATA_LEN], name, namelen);
  for (uint32_t i = 0; i < size; i++) {
    s_stream[FILEDATA_LEN + namelen + i] = prv_byte(seed, i);
  }
  return FILEDATA_LEN + namelen + size;
}

static TestFile *prv_old_file_in(const uint16_t *sectors, const char *name, uint32_t size,
                                 uint8_t seed, unsigned opts) {
  const uint32_t len = prv_stream(name, size, seed, opts);
  const uint32_t n = prv_old_sectors(name, size);

  for (uint32_t k = 0; k < n; k++) {
    const uint16_t next = (k + 1 < n) ? sectors[k + 1] : NO_PAGE;
    const unsigned hdr_opts = (k == 0) ? opts : (opts & ~(OLD_BAD_CRC | OLD_TAGGED));
    s_used[sectors[k]] = true;
    prv_old_header(sectors[k], (k == 0) ? FLAGS_START : FLAGS_CONT, next, hdr_opts);
    const uint32_t pos = k * OLD_PAYLOAD;
    prv_raw_write(sectors[k] * SECTOR_SIZE_BYTES + HDR_SIZE, &s_stream[pos],
                  MIN(OLD_PAYLOAD, len - pos));
    if (((opts & OLD_DELETED) != 0) || (((opts & OLD_HALF_DELETED) != 0) && (k == 0))) {
      prv_old_deleted(sectors[k], (k == 0) ? FLAGS_START : FLAGS_CONT);
    }
  }

  return prv_add_file(name, size, seed, opts);
}

static void prv_new_pages(const uint16_t *pages, uint32_t len, unsigned opts) {
  const uint32_t n = DIVIDE_CEIL(len, NEW_PAYLOAD);

  for (uint32_t k = 0; k < n; k++) {
    const uint32_t offs = pages[k] * PAGE_SIZE;
    const uint16_t next = (k + 1 < n) ? pages[k + 1] : NO_PAGE;
    prv_page_header(offs, 2, (k == 0) ? FLAGS_START : FLAGS_CONT, next, 0);
    const uint32_t pos = k * NEW_PAYLOAD;
    prv_raw_write(offs + HDR_SIZE, &s_stream[pos], MIN(NEW_PAYLOAD, len - pos));
    if ((opts & OLD_DELETED) != 0) {
      prv_deleted_at(offs, (k == 0) ? FLAGS_START : FLAGS_CONT);
    }
  }
}

static TestFile *prv_new_file_at(const uint16_t *pages, const char *name, uint32_t size,
                                 uint8_t seed, unsigned opts) {
  prv_new_pages(pages, prv_stream(name, size, seed, opts), opts);
  return prv_add_file(name, size, seed, opts);
}

static void prv_capture_old(TestFile *f, const uint16_t *sectors) {
  const uint32_t len = FILEDATA_LEN + strlen(f->name) + f->size;
  for (uint32_t pos = 0; pos < len; pos += OLD_PAYLOAD) {
    prv_raw_read(sectors[pos / OLD_PAYLOAD] * SECTOR_SIZE_BYTES + HDR_SIZE, &s_stream[pos],
                 MIN(OLD_PAYLOAD, len - pos));
  }
  f->data = malloc(f->size);
  cl_assert(f->data != nullptr);
  memcpy(f->data, &s_stream[FILEDATA_LEN + strlen(f->name)], f->size);
}

static void prv_small_page_format(void) {
  for (uint32_t page = 0; page < NUM_PAGES; page++) {
    prv_erased_header_at(page * PAGE_SIZE, 2);
  }
}

static void prv_raw_erase(uint16_t sector) {
  pbl_flash_erase(FLASH, PART_FILESYSTEM_OFFSET + sector * SECTOR_SIZE_BYTES, SECTOR_SIZE_BYTES);
}

static TestFile *prv_old_file(const char *name, uint32_t size, uint8_t seed, unsigned opts) {
  uint16_t sectors[NUM_SECTORS];
  const uint32_t n = prv_old_sectors(name, size);
  for (uint32_t k = 0; k < n; k++) {
    sectors[k] = prv_alloc_sector();
  }
  return prv_old_file_in(sectors, name, size, seed, opts);
}

static uint32_t prv_full_size(const char *name) {
  return OLD_PAYLOAD - FILEDATA_LEN - strlen(name);
}

static void prv_reboot(void) {
  pfs_reset_all_state();
  test_rerun_reboot_cleanup();
  pfs_init(true);
}

static void prv_assert_file(const TestFile *f) {
  int fd = pfs_open(f->name, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  cl_assert_(fd >= 0, f->name);
  cl_assert_equal_i(pfs_get_file_size(fd), f->size);
  static uint8_t buf[4096];
  for (uint32_t done = 0; done < f->size; done += sizeof(buf)) {
    const uint32_t n = MIN(sizeof(buf), f->size - done);
    cl_assert_equal_i(pfs_read(fd, buf, n), n);
    for (uint32_t i = 0; i < n; i++) {
      const uint8_t want = (f->data != nullptr) ? f->data[done + i] : prv_byte(f->seed, done + i);
      if (buf[i] != want) {
        cl_fail(f->name);
      }
    }
  }
  cl_assert_equal_i(pfs_close(fd), S_SUCCESS);
}

static bool prv_kept(const char *name) {
  for (int i = 0; i < s_num_files; i++) {
    if (s_files[i].kept && (strcmp(s_files[i].name, name) == 0)) {
      return true;
    }
  }
  return false;
}

static int prv_count_files(void) {
  int count = 0;
  PFSFileListEntry *list = pfs_create_file_list(nullptr);
  for (PFSFileListEntry *e = list; e != nullptr;
       e = (PFSFileListEntry *)list_get_next(&e->list_node)) {
    cl_assert_(prv_kept(e->name), e->name);
    count++;
  }
  pfs_delete_file_list(list);
  return count;
}

static void prv_assert_files(void) {
  int kept = 0;
  for (int i = 0; i < s_num_files; i++) {
    if (s_files[i].kept) {
      prv_assert_file(&s_files[i]);
      kept++;
    } else if (!prv_kept(s_files[i].name)) {
      cl_assert_equal_i(pfs_open(s_files[i].name, OP_FLAG_READ, 0, 0), E_DOES_NOT_EXIST);
    }
  }
  cl_assert_equal_i(prv_count_files(), kept);
}

static void prv_assert_new_layout(void) {
  for (uint32_t page = 0; page < NUM_PAGES; page++) {
    TestPageHeader hdr;
    prv_raw_read(page * PAGE_SIZE, &hdr, sizeof(hdr));
    const bool deleted = (hdr.page_flags == (FLAGS_START & ~FLAG_DELETED)) ||
                         (hdr.page_flags == (FLAGS_CONT & ~FLAG_DELETED));
    const bool blank = (hdr.version == 0xffff) && (hdr.page_flags == 0xff);
    cl_assert((hdr.version == FIRST_VERSION) || (hdr.version == NEW_VERSION) || deleted || blank);

    TestFileMetaData meta;
    prv_raw_read(page * PAGE_SIZE + HDR_SIZE + sizeof(TestFileHeader), &meta, sizeof(meta));
    if ((hdr.page_flags == (FLAGS_START & ~FLAG_DELETED)) && (meta.delete_state != 0)) {
      cl_assert_equal_i(hdr.next_page, NO_PAGE);
    } else if (((hdr.page_flags & 0x04) == 0) && ((hdr.page_flags & FLAG_DELETED) != 0)) {
      cl_assert_equal_i(meta.create_state, 0);
      cl_assert_equal_i(meta.tmp_state, 0);
    }
  }
}

static int prv_count_version(uint16_t version) {
  int count = 0;
  for (uint32_t page = 0; page < NUM_PAGES; page++) {
    TestPageHeader hdr;
    prv_raw_read(page * PAGE_SIZE, &hdr, sizeof(hdr));
    count += (hdr.version == version) ? 1 : 0;
  }
  return count;
}

static bool prv_sector_page_firmware_mounts(void) {
  for (uint32_t sector = 0; sector < NUM_SECTORS; sector++) {
    TestPageHeader hdr;
    prv_raw_read(sector * SECTOR_SIZE_BYTES, &hdr, sizeof(hdr));
    if ((hdr.version >> 8) != (FIRST_VERSION >> 8)) {
      continue;
    }
    if (hdr.version > FIRST_VERSION) {
      return false;
    }
    const uint8_t flags = hdr.page_flags;
    const bool deleted =
        (flags == (FLAGS_START & ~FLAG_DELETED)) || (flags == (FLAGS_CONT & ~FLAG_DELETED));
    const bool typed = ((~flags & 0x0d) != 0) && ((~flags & FLAG_DELETED) == 0);
    if (typed || deleted) {
      return true;
    }
  }
  return false;
}

static int prv_count_tagged(void) {
  int count = 0;
  for (uint32_t page = 0; page < NUM_PAGES; page++) {
    TestPageHeader hdr;
    prv_raw_read(page * PAGE_SIZE, &hdr, sizeof(hdr));
    count += (hdr.last_written == 0xfe) ? 1 : 0;
  }
  return count;
}

static void prv_assert_usable(void) {
  TestFile *f = &s_files[s_num_files++];
  snprintf(f->name, sizeof(f->name), "after%d", s_num_files);
  f->size = 3 * PAGE_SIZE + 5;
  f->seed = 0x99;
  f->kept = true;
  f->data = nullptr;
  int fd = pfs_open(f->name, OP_FLAG_WRITE, FILE_TYPE_STATIC, f->size);
  cl_assert(fd >= 0);
  for (uint32_t i = 0; i < f->size; i++) {
    const uint8_t b = prv_byte(f->seed, i);
    cl_assert_equal_i(pfs_write(fd, &b, 1), 1);
  }
  cl_assert_equal_i(pfs_close(fd), S_SUCCESS);
  prv_reboot();
  prv_assert_files();
}

static void prv_assert_unavailable(void) {
  const uint32_t ops = fake_flash_op_count();
  size_t size;
  for (int i = 0; i < s_num_files; i++) {
    cl_assert(pfs_open(s_files[i].name, OP_FLAG_READ, FILE_TYPE_STATIC, 0) < 0);
    cl_assert(pfs_stat(s_files[i].name, &size) < 0);
    cl_assert(pfs_remove(s_files[i].name) < 0);
  }
  cl_assert(pfs_open("created", OP_FLAG_WRITE, FILE_TYPE_STATIC, 100) < 0);
  cl_assert(pfs_create_file_list(nullptr) == nullptr);
  cl_assert_equal_i(pfs_get_size(), 0);
  cl_assert_equal_i(get_available_pfs_space(), 0);
  pfs_format(true);
  cl_assert_equal_i(fake_flash_op_count(), ops);
}

static void prv_mixed_filesystem(void) {
  const uint16_t first[] = {0};
  prv_old_file_in(first, "zero", 700, 14, OLD_DELETED);
  prv_old_file("s", 100, 1, OLD_TAGGED);
  prv_old_file("settings", prv_full_size("settings"), 2, 0);
  prv_old_file("two", OLD_PAYLOAD + 5000, 3, 0);
  prv_old_file("emoji", 2 * OLD_PAYLOAD + 7777, 4, 0);
  prv_old_file("gone", 4000, 5, OLD_DELETED);
  prv_old_file("half", OLD_PAYLOAD + 10, 6, OLD_HALF_DELETED);
  prv_old_file("tmp", 300, 7, OLD_TMP);
  prv_old_file("new", 300, 8, OLD_INCOMPLETE);
  prv_old_file("exact", NEW_PAYLOAD - FILEDATA_LEN - 5, 9, 0);
  prv_old_erased(prv_alloc_sector());
  prv_old_erased(prv_alloc_sector());
}

void test_pfs_convert__initialize(void) {
  fake_spi_flash_init(0, PART_FILESYSTEM_OFFSET + PART_FILESYSTEM_SIZE);
  pfs_reset_all_state();
  memset(s_used, 0, sizeof(s_used));
  s_num_files = 0;
  s_next_sector = 0;
  s_stream = malloc(NUM_SECTORS * SECTOR_SIZE_BYTES);
  s_image = malloc(PART_FILESYSTEM_SIZE);
  cl_assert((s_stream != nullptr) && (s_image != nullptr));
}

void test_pfs_convert__cleanup(void) {
  test_override_convert_max_steps(0);
  test_skip_first_version_rewrite(false);
  fake_spi_flash_force_op_failure(0, FakeFlashCutBefore, nullptr);
  fake_spi_flash_cleanup();
  for (int i = 0; i < s_num_files; i++) {
    free(s_files[i].data);
  }
  free(s_stream);
  free(s_image);
}

void test_pfs_convert__every_kind_of_page(void) {
  prv_mixed_filesystem();
  prv_old_file("corrupt", 500, 10, OLD_BAD_CRC);
  prv_old_file("chain", OLD_PAYLOAD + 99, 11, OLD_BAD_CHAIN);
  prv_old_file("GC", 64, 12, OLD_DROPPED);
  const uint8_t gc_data[] = {1, 0xfe, 0, 0};
  prv_raw_write(s_next_sector * SECTOR_SIZE_BYTES + HDR_SIZE + FILEDATA_LEN + 2, gc_data,
                sizeof(gc_data));
  prv_old_file("PFS-CONVERT", 4, 13, OLD_DROPPED);

  prv_reboot();

  prv_assert_files();
  prv_assert_new_layout();
  cl_assert_equal_i(prv_count_version(NEW_VERSION), NUM_PAGES);
  cl_assert_equal_i(prv_count_tagged(), 1);
  prv_assert_usable();
}

void test_pfs_convert__second_copy_is_dropped(void) {
  const uint16_t first[] = {3};
  const uint16_t second[] = {40};
  prv_old_file_in(first, "dup", prv_full_size("dup"), 1, 0);
  prv_old_file_in(second, "dup", 2000, 2, OLD_DROPPED);

  prv_reboot();

  prv_assert_files();
}

void test_pfs_convert__full_filesystem(void) {
  char name[16];
  prv_old_file("emoji", 2 * OLD_PAYLOAD + 100000, 1, 0);
  prv_old_file("activity_sleep", OLD_PAYLOAD + 130000, 2, 0);
  for (int i = 0; i < 61; i++) {
    snprintf(name, sizeof(name), "set%d", i);
    prv_old_file(name, prv_full_size(name), (uint8_t)(10 + i), 0);
  }
  for (int i = 0; i < 10; i++) {
    snprintf(name, sizeof(name), "app%d", i);
    prv_old_file(name, 2000 + i * 700, (uint8_t)(80 + i), 0);
  }
  for (int i = 0; i < 12; i++) {
    snprintf(name, sizeof(name), "del%d", i);
    prv_old_file(name, 4000, (uint8_t)i, OLD_DELETED);
  }
  for (uint16_t sector = 0; sector < NUM_SECTORS; sector++) {
    if (!s_used[sector]) {
      prv_old_erased(sector);
    }
  }

  prv_reboot();

  prv_assert_files();
  prv_assert_new_layout();
  cl_assert_equal_i(prv_count_version(NEW_VERSION), NUM_PAGES);
  prv_assert_usable();
}

void test_pfs_convert__no_free_sector(void) {
  char name[16];
  for (int i = 0; i < NUM_SECTORS; i++) {
    snprintf(name, sizeof(name), "f%d", i);
    prv_old_file(name, (i == 50) ? prv_full_size(name) : 9000 + i, (uint8_t)i, 0);
  }

  prv_reboot();

  int kept = 0;
  for (int i = 0; i < s_num_files; i++) {
    int fd = pfs_open(s_files[i].name, OP_FLAG_READ, 0, 0);
    s_files[i].kept = fd >= 0;
    if (fd >= 0) {
      pfs_close(fd);
      kept++;
    }
  }
  cl_assert(!s_files[50].kept);
  cl_assert_equal_i(kept, NUM_SECTORS - 1);
  prv_assert_files();
  prv_assert_usable();
}

void test_pfs_convert__unreadable_files_only(void) {
  prv_old_file("chain", OLD_PAYLOAD + 99, 1, OLD_BAD_CHAIN);
  prv_old_file("tmp", 10, 2, OLD_TMP);

  prv_reboot();

  prv_assert_files();
  prv_assert_new_layout();
  prv_assert_usable();
}

void test_pfs_convert__step_limit_leaves_the_files_to_a_later_boot(void) {
  prv_mixed_filesystem();
  test_override_convert_max_steps(3);

  prv_reboot();

  prv_assert_unavailable();
  test_override_convert_max_steps(0);
  prv_reboot();
  prv_assert_files();
  prv_assert_new_layout();
  prv_assert_usable();
}

static void prv_boot_with_failing_sector(uint16_t sector) {
  const uint16_t gone0[] = {0};
  const uint16_t a[] = {1};
  const uint16_t gone2[] = {2};
  const uint16_t big[] = {3, 4};
  prv_old_file_in(gone0, "gone0", prv_full_size("gone0"), 1, OLD_DELETED);
  prv_old_file_in(a, "a", 100, 2, 0);
  prv_old_file_in(gone2, "gone2", prv_full_size("gone2"), 3, OLD_DELETED);
  prv_old_file_in(big, "big", OLD_PAYLOAD + 50000, 4, 0);
  const uint32_t addr = PART_FILESYSTEM_OFFSET + sector * SECTOR_SIZE_BYTES;
  fake_spi_flash_fail_erase(true, addr);

  prv_reboot();

  prv_assert_unavailable();
  cl_assert_equal_i(fake_flash_sector_erase_count(addr), 1);
  fake_spi_flash_fail_erase(false, 0);
  prv_reboot();
  prv_assert_files();
  prv_assert_new_layout();
  prv_assert_usable();
}

void test_pfs_convert__sector_that_never_erases(void) {
  prv_boot_with_failing_sector(2);
}

void test_pfs_convert__marker_sector_that_never_erases(void) {
  prv_boot_with_failing_sector(0);
}

void test_pfs_convert__marker_sector_that_never_erases_without_an_error(void) {
  fake_spi_flash_fail_erase_silently(true);
  prv_boot_with_failing_sector(0);
}

void test_pfs_convert__second_boot_writes_nothing(void) {
  prv_mixed_filesystem();
  prv_reboot();
  const uint32_t ops = fake_flash_op_count();

  prv_reboot();

  cl_assert_equal_i(fake_flash_op_count(), ops);
  prv_assert_files();
}

void test_pfs_convert__small_page_filesystem_is_left_alone(void) {
  prv_reboot();
  TestFile *f = &s_files[s_num_files++];
  snprintf(f->name, sizeof(f->name), "kept");
  f->size = SECTOR_SIZE_BYTES + 3;
  f->seed = 0x31;
  f->kept = true;
  f->data = nullptr;
  int fd = pfs_open(f->name, OP_FLAG_WRITE, FILE_TYPE_STATIC, f->size);
  cl_assert(fd >= 0);
  for (uint32_t i = 0; i < f->size; i++) {
    const uint8_t b = prv_byte(f->seed, i);
    pfs_write(fd, &b, 1);
  }
  pfs_close(fd);
  const uint32_t ops = fake_flash_op_count();

  prv_reboot();

  cl_assert_equal_i(fake_flash_op_count(), ops);
  prv_assert_files();
}

static bool prv_boot_with_cut(uint32_t cut, FakeFlashCut how) {
  jmp_buf jb;
  if (setjmp(jb) != 0) {
    fake_spi_flash_force_op_failure(0, FakeFlashCutBefore, nullptr);
    return true;
  }
  fake_spi_flash_force_op_failure(cut, how, &jb);
  prv_reboot();
  fake_spi_flash_force_op_failure(0, FakeFlashCutBefore, nullptr);
  return false;
}

static void prv_restore(void) {
  fake_spi_flash_restore(s_image, PART_FILESYSTEM_OFFSET, PART_FILESYSTEM_SIZE);
}

static uint16_t prv_first_new_sector(void) {
  for (uint16_t sector = 0; sector < NUM_SECTORS; sector++) {
    TestPageHeader hdr;
    prv_raw_read((sector + 1) * SECTOR_SIZE_BYTES - PAGE_SIZE, &hdr, sizeof(hdr));
    if (hdr.version == NEW_VERSION) {
      return sector;
    }
  }
  return NUM_SECTORS;
}

void test_pfs_convert__half_deleted_page_does_not_delete_a_copy(void) {
  prv_old_file("a", 3 * NEW_PAYLOAD, 1, 0);
  fake_spi_flash_save(s_image, PART_FILESYSTEM_OFFSET, PART_FILESYSTEM_SIZE);

  uint16_t sector = NUM_SECTORS;
  for (uint32_t cut = 0; sector == NUM_SECTORS; cut++) {
    prv_restore();
    cl_assert(prv_boot_with_cut(cut, FakeFlashCutBefore));
    sector = prv_first_new_sector();
  }

  const uint32_t page = sector * PAGES_PER_SECTOR + 2;
  TestPageHeader hdr;
  prv_raw_read(page * PAGE_SIZE, &hdr, sizeof(hdr));
  cl_assert_equal_i(hdr.page_flags, FLAGS_ERASED);
  hdr.page_flags = FLAGS_START;
  hdr.next_page = page + 1;
  hdr.next_page_crc = pbl_crc8_reversed(0, &hdr.next_page, sizeof(hdr.next_page));
  hdr.hdr_crc = pbl_crc32_legacy(&hdr, offsetof(TestPageHeader, hdr_crc));
  prv_raw_write(page * PAGE_SIZE, &hdr, sizeof(hdr));
  const uint8_t deleted = FLAGS_START & ~FLAG_DELETED;
  prv_raw_write(page * PAGE_SIZE + offsetof(TestPageHeader, page_flags), &deleted, 1);

  prv_reboot();

  prv_assert_files();
  prv_assert_new_layout();
  prv_assert_usable();
}

static void prv_assert_after_cut(bool rewrite) {
  prv_assert_files();
  if (rewrite) {
    cl_assert(!prv_sector_page_firmware_mounts());
  } else {
    prv_assert_new_layout();
  }
}

static void prv_power_cut_sweep_with(bool rewrite) {
  test_skip_first_version_rewrite(!rewrite);
  prv_restore();

  uint32_t ops = fake_flash_op_count();
  prv_reboot();
  ops = fake_flash_op_count() - ops;
  prv_assert_files();

  volatile uint32_t cuts = 0;
  for (volatile uint32_t cut = 0; cut < ops; cut++) {
    for (volatile int how = FakeFlashCutBefore; how <= FakeFlashCutBits; how++) {
      prv_restore();
      cl_assert(prv_boot_with_cut(cut, how));
      cuts++;
      prv_reboot();
      prv_assert_after_cut(rewrite);
    }

    prv_restore();
    cl_assert(prv_boot_with_cut(cut, FakeFlashCutBits));
    if (prv_boot_with_cut((cut * 7919 + 13) % ops,
                          ((cut % 2) == 0) ? FakeFlashCutHalf : FakeFlashCutBits)) {
      cuts++;
    }
    prv_reboot();
    prv_assert_after_cut(rewrite);
  }

  printf("operations %u, cuts %u\n", (unsigned)ops, (unsigned)cuts);
  cl_assert(cuts >= 3 * ops);
  test_skip_first_version_rewrite(false);
}

static void prv_power_cut_sweep(void) {
  fake_spi_flash_save(s_image, PART_FILESYSTEM_OFFSET, PART_FILESYSTEM_SIZE);
  prv_power_cut_sweep_with(false);
  prv_power_cut_sweep_with(true);
}

void test_pfs_convert__power_cut_at_every_operation(void) {
  prv_mixed_filesystem();
  prv_power_cut_sweep();
}

static void prv_legacy1_over_small_pages(void) {
  prv_small_page_format();

  const uint16_t set_a8[] = {1, 2, 3};
  prv_new_file_at(set_a8, "set_a", 2 * NEW_PAYLOAD, 1, OLD_DROPPED);
  const uint16_t app_bin[] = {4, 5, 6, 7, 8, 9};
  prv_new_file_at(app_bin, "app_bin", 5 * NEW_PAYLOAD + 100, 2, 0);
  const uint16_t gone8[] = {10, 11};
  prv_new_file_at(gone8, "gone8", NEW_PAYLOAD + 10, 3, OLD_DELETED);
  const uint16_t small0[] = {16};
  prv_new_file_at(small0, "small0", 900, 4, 0);
  const uint16_t big0[] = {32, 35, 33};
  prv_new_file_at(big0, "big0", 2 * NEW_PAYLOAD + 50, 5, 0);
  const uint16_t cross[] = {47, 48, 49};
  prv_new_file_at(cross, "cross", 2 * NEW_PAYLOAD + 7, 6, 0);
  const uint16_t vis[] = {64};
  prv_new_file_at(vis, "dup_vis", 300, 7, 0);
  const uint16_t old_s8[] = {80};
  prv_new_file_at(old_s8, "old_s", 200, 8, OLD_DELETED);
  const uint16_t vis_big[] = {96};
  prv_new_file_at(vis_big, "dup_big", 700, 15, 0);
  const uint16_t hid[] = {12};
  prv_new_file_at(hid, "hid", 100, 16, OLD_DROPPED);
  const uint16_t hid_vis[] = {112, 113};
  prv_new_file_at(hid_vis, "hid", NEW_PAYLOAD + 1, 17, 0);

  const uint16_t set_a[] = {20};
  prv_capture_old(prv_old_file_in(set_a, "set_a", prv_full_size("set_a"), 9, 0), set_a);
  const uint16_t emoji[] = {21, 22, 23};
  prv_capture_old(prv_old_file_in(emoji, "emoji", 2 * OLD_PAYLOAD + 5000, 10, 0), emoji);
  const uint16_t dup[] = {30};
  prv_old_file_in(dup, "dup_vis", 4000, 11, OLD_DROPPED);
  const uint16_t app[] = {31};
  prv_capture_old(prv_old_file_in(app, "app", 500, 12, 0), app);
  const uint16_t old_s[] = {33};
  prv_capture_old(prv_old_file_in(old_s, "old_s", OLD_PAYLOAD / 2, 13, 0), old_s);
  const uint16_t dup_big[] = {34};
  prv_old_file_in(dup_big, "dup_big", prv_full_size("dup_big"), 18, OLD_DROPPED);
  prv_raw_erase(40);
  prv_old_erased(40);
  const uint16_t pure[] = {41};
  prv_raw_erase(41);
  prv_capture_old(prv_old_file_in(pure, "pure", OLD_PAYLOAD - 1000, 14, 0), pure);
}

void test_pfs_convert__old_files_over_small_page_leftovers(void) {
  prv_legacy1_over_small_pages();
  cl_assert(test_convert_needed());

  prv_reboot();

  cl_assert(!test_convert_needed());
  cl_assert(!prv_sector_page_firmware_mounts());
  prv_assert_files();
  prv_assert_new_layout();
  cl_assert_equal_i(prv_count_tagged(), 1);
  const uint32_t ops = fake_flash_op_count();
  prv_reboot();
  cl_assert_equal_i(fake_flash_op_count(), ops);
  prv_assert_usable();
}

void test_pfs_convert__deleted_old_file_over_small_page_leftovers(void) {
  prv_small_page_format();
  const uint16_t gone[] = {0, 1, 2};
  prv_old_file_in(gone, "gone", 2 * OLD_PAYLOAD + 5000, 1, OLD_DELETED);
  const uint16_t set_a[] = {20};
  prv_capture_old(prv_old_file_in(set_a, "set_a", prv_full_size("set_a"), 2, 0), set_a);
  const uint16_t emoji[] = {21, 22, 23};
  prv_capture_old(prv_old_file_in(emoji, "emoji", 2 * OLD_PAYLOAD + 7000, 3, 0), emoji);

  prv_reboot();

  cl_assert(!test_convert_needed());
  prv_assert_files();
  prv_assert_new_layout();
  prv_assert_usable();
}

void test_pfs_convert__copy_beside_an_older_copy_with_power_cuts(void) {
  prv_small_page_format();
  const uint16_t older[] = {1, 2, 3};
  prv_new_file_at(older, "set_a", 2 * NEW_PAYLOAD, 7, OLD_DROPPED);
  const uint16_t set_a[] = {20};
  prv_capture_old(prv_old_file_in(set_a, "set_a", prv_full_size("set_a"), 9, 0), set_a);
  uint16_t copy[17];
  for (uint16_t i = 0; i < 17; i++) {
    copy[i] = 17 + i;
  }
  prv_new_pages(copy, OLD_PAYLOAD, 0);
  prv_power_cut_sweep();
}

static void prv_small_pages_beside_old_files(void) {
  prv_small_page_format();
  const uint16_t chain8[] = {48, 64, 65};
  prv_new_file_at(chain8, "chain8", 2 * NEW_PAYLOAD + 9, 1, 0);
  const uint16_t tagged[] = {3};
  prv_new_file_at(tagged, "tagged", 100, 2, 0);
  const uint8_t tag = 0xfe;
  prv_raw_write(3 * PAGE_SIZE + offsetof(TestPageHeader, last_written), &tag, 1);
  const uint16_t marker[] = {13};
  prv_new_file_at(marker, "PFS-CONVERT", 4, 3, OLD_DROPPED);
}

static void prv_boot_and_assert_converted(void) {
  cl_assert(test_convert_needed());

  prv_reboot();

  cl_assert(!test_convert_needed());
  cl_assert(!prv_sector_page_firmware_mounts());
  prv_assert_files();
  prv_assert_new_layout();
  cl_assert_equal_i(prv_count_tagged(), 1);
  prv_assert_usable();
}

void test_pfs_convert__single_sector_old_file_over_small_page_leftovers(void) {
  prv_small_pages_beside_old_files();
  const uint16_t set_a[] = {20};
  prv_capture_old(prv_old_file_in(set_a, "set_a", prv_full_size("set_a"), 4, 0), set_a);
  prv_boot_and_assert_converted();
}

void test_pfs_convert__multi_sector_old_file_over_small_page_leftovers(void) {
  prv_small_pages_beside_old_files();
  prv_page_header(5 * PAGE_SIZE, 2, FLAGS_CONT, 22 * PAGES_PER_SECTOR, 0);
  prv_deleted_at(5 * PAGE_SIZE, FLAGS_CONT);
  const uint16_t emoji[] = {21, 22, 23};
  prv_capture_old(prv_old_file_in(emoji, "emoji", 2 * OLD_PAYLOAD + 7000, 5, 0), emoji);
  prv_boot_and_assert_converted();
}

void test_pfs_convert__old_files_over_small_page_leftovers_with_power_cuts(void) {
  prv_legacy1_over_small_pages();
  prv_power_cut_sweep();
}

static void prv_write_file(const char *name, uint32_t size, uint8_t seed) {
  static uint8_t buf[2048];
  int fd = pfs_open(name, OP_FLAG_WRITE, FILE_TYPE_STATIC, size);
  cl_assert_(fd >= 0, name);
  for (uint32_t done = 0; done < size; done += sizeof(buf)) {
    const uint32_t n = MIN(sizeof(buf), size - done);
    for (uint32_t i = 0; i < n; i++) {
      buf[i] = prv_byte(seed, done + i);
    }
    cl_assert_equal_i(pfs_write(fd, buf, n), n);
  }
  cl_assert_equal_i(pfs_close(fd), S_SUCCESS);
}

static void prv_small_page_workload(void) {
  char name[16];
  for (int i = 0; i < 24; i++) {
    snprintf(name, sizeof(name), "w%d", i);
    prv_write_file(name, 2 * NEW_PAYLOAD - 500, (uint8_t)(200 + i));
    if ((i % 3) == 2) {
      snprintf(name, sizeof(name), "w%d", i - 1);
      pfs_remove(name);
    }
  }
}

void test_pfs_convert__small_page_filesystem_states_are_left_alone(void) {
  prv_reboot();
  char name[16];
  for (int i = 0; i < SMALL_FILES; i++) {
    snprintf(name, sizeof(name), "f%d", i);
    prv_write_file(name, 2 * NEW_PAYLOAD + 100 + i, (uint8_t)i);
  }
  for (int i = 0; i < SMALL_FILES; i += 2) {
    snprintf(name, sizeof(name), "f%d", i);
    pfs_remove(name);
  }
  prv_reboot();
  fake_spi_flash_save(s_image, PART_FILESYSTEM_OFFSET, PART_FILESYSTEM_SIZE);
  cl_assert(!test_convert_needed());

  uint32_t ops = fake_flash_op_count();
  const uint32_t erases = fake_flash_erase_count();
  prv_small_page_workload();
  ops = fake_flash_op_count() - ops;
  cl_assert(fake_flash_erase_count() > erases);

  const uint32_t stride = MAX(1, ops / 1500);
  volatile uint32_t cuts = 0;
  for (volatile uint32_t cut = 0; cut < ops; cut += stride) {
    for (volatile int how = FakeFlashCutBefore; how <= FakeFlashCutBits; how++) {
      prv_restore();
      pfs_reset_all_state();
      test_rerun_reboot_cleanup();
      pfs_init(false);
      jmp_buf jb;
      if (setjmp(jb) == 0) {
        fake_spi_flash_force_op_failure(cut, how, &jb);
        prv_small_page_workload();
      }
      fake_spi_flash_force_op_failure(0, FakeFlashCutBefore, nullptr);
      cl_assert(!test_convert_needed());
      cuts++;
    }
  }
  printf("operations %u, cuts %u\n", (unsigned)ops, (unsigned)cuts);

  for (int i = 1; i < SMALL_FILES; i += 2) {
    snprintf(name, sizeof(name), "f%d", i);
    prv_add_file(name, 2 * NEW_PAYLOAD + 100 + i, (uint8_t)i, 0);
  }
  prv_restore();
  prv_reboot();
  prv_assert_files();
}

static void prv_full_small_page_filesystem(void) {
  prv_small_page_format();

  uint16_t pages[NUM_PAGES];
  for (uint16_t i = 0; i < 21; i++) {
    pages[i] = i;
  }
  prv_new_file_at(pages, "big", 20 * NEW_PAYLOAD + 100, 1, 0);
  const uint16_t other[] = {21, 22};
  prv_new_file_at(other, "other", NEW_PAYLOAD + 5, 2, 0);
  for (uint16_t page = 23; page < 2 * PAGES_PER_SECTOR; page++) {
    const uint16_t gone[] = {page};
    prv_new_file_at(gone, "gone", 50, 3, OLD_DELETED);
  }

  char name[16];
  for (uint16_t sector = 2; sector < NUM_SECTORS - 1; sector++) {
    for (uint16_t i = 0; i < PAGES_PER_SECTOR; i++) {
      pages[i] = sector * PAGES_PER_SECTOR + i;
    }
    snprintf(name, sizeof(name), "fill%u", (unsigned)sector);
    prv_new_file_at(pages, name, PAGES_PER_SECTOR * NEW_PAYLOAD - FILEDATA_LEN - strlen(name),
                    (uint8_t)sector, (sector >= NUM_SECTORS - 8) ? OLD_DELETED : 0);
  }
}

void test_pfs_convert__small_page_garbage_collection_cut_is_left_alone(void) {
  prv_full_small_page_filesystem();
  fake_spi_flash_save(s_image, PART_FILESYSTEM_OFFSET, PART_FILESYSTEM_SIZE);
  cl_assert(!test_convert_needed());

  uint32_t ops = fake_flash_op_count();
  const uint32_t erases = fake_flash_erase_count();
  prv_reboot();
  prv_write_file("x", 100, 9);
  ops = fake_flash_op_count() - ops;
  cl_assert(fake_flash_erase_count() > erases);

  volatile uint32_t cuts = 0;
  for (volatile uint32_t cut = 0; cut < ops; cut++) {
    for (volatile int how = FakeFlashCutBefore; how <= FakeFlashCutBits; how++) {
      prv_restore();
      jmp_buf jb;
      if (setjmp(jb) == 0) {
        fake_spi_flash_force_op_failure(cut, how, &jb);
        prv_reboot();
        prv_write_file("x", 100, 9);
      }
      fake_spi_flash_force_op_failure(0, FakeFlashCutBefore, nullptr);
      cl_assert(!test_convert_needed());
      prv_reboot();
      prv_assert_file(&s_files[0]);
      prv_assert_file(&s_files[1]);
      cuts++;
    }
  }
  printf("operations %u, cuts %u\n", (unsigned)ops, (unsigned)cuts);

  pfs_remove("x");
  prv_assert_files();
}

void test_pfs_convert__blank_flash_is_formatted_with_the_new_version(void) {
  prv_reboot();

  cl_assert_equal_i(prv_count_version(NEW_VERSION), NUM_PAGES);
}

void test_pfs_convert__files_written_over_first_version_headers_read_back(void) {
  prv_small_page_format();
  const uint16_t kept[] = {16, 17};
  prv_new_file_at(kept, "kept", NEW_PAYLOAD + 10, 1, 0);
  prv_reboot();

  prv_write_file("a", 3 * NEW_PAYLOAD + 5, 2);
  prv_add_file("a", 3 * NEW_PAYLOAD + 5, 2, 0);
  prv_write_file("b", 100, 3);
  prv_add_file("b", 100, 3, 0);
  prv_reboot();

  prv_assert_files();
  prv_assert_new_layout();
  cl_assert_equal_i(prv_count_version(FIRST_VERSION & NEW_VERSION), 0);
}

void test_pfs_convert__garbage_collection_writes_the_new_version(void) {
  prv_small_page_format();
  const uint16_t live[] = {33, 35};
  prv_new_file_at(live, "live", NEW_PAYLOAD + 10, 1, 0);
  const uint16_t gone[] = {34, 36};
  prv_new_file_at(gone, "gone", NEW_PAYLOAD + 10, 2, OLD_DELETED);
  prv_reboot();

  test_force_garbage_collection(2 * PAGES_PER_SECTOR);
  prv_reboot();

  prv_assert_files();
  for (uint32_t page = 2 * PAGES_PER_SECTOR; page < 3 * PAGES_PER_SECTOR; page++) {
    TestPageHeader hdr;
    prv_raw_read(page * PAGE_SIZE, &hdr, sizeof(hdr));
    cl_assert_equal_i(hdr.version, NEW_VERSION);
  }
}

static void prv_first_version_small_pages(void) {
  prv_small_page_format();
  const uint16_t zero[] = {0, 1, 20};
  prv_new_file_at(zero, "zero", 2 * NEW_PAYLOAD + 70, 1, 0);
  const uint16_t gone[] = {2, 3};
  prv_new_file_at(gone, "gone", NEW_PAYLOAD + 5, 2, OLD_DELETED);
  const uint16_t small[] = {4};
  prv_new_file_at(small, "small", 300, 3, 0);
  const uint16_t tagged[] = {15};
  prv_new_file_at(tagged, "tagged", 40, 4, 0);
  const uint8_t tag = 0xfe;
  prv_raw_write(15 * PAGE_SIZE + offsetof(TestPageHeader, last_written), &tag, 1);
  const uint16_t far[] = {50, 70, 16};
  prv_new_file_at(far, "far", 2 * NEW_PAYLOAD + 9, 5, 0);
}

static void prv_assert_sector_version(uint16_t sector, uint16_t version) {
  for (uint32_t page = sector * PAGES_PER_SECTOR; page < (sector + 1) * PAGES_PER_SECTOR; page++) {
    TestPageHeader hdr;
    prv_raw_read(page * PAGE_SIZE, &hdr, sizeof(hdr));
    cl_assert_equal_i(hdr.version, version);
  }
}

void test_pfs_convert__first_version_sector_is_rewritten_where_sector_page_firmware_looks(void) {
  prv_first_version_small_pages();
  cl_assert(prv_sector_page_firmware_mounts());

  prv_reboot();

  cl_assert(!prv_sector_page_firmware_mounts());
  prv_assert_sector_version(0, NEW_VERSION);
  prv_assert_files();
  cl_assert_equal_i(prv_count_tagged(), 1);
  const uint32_t ops = fake_flash_op_count();
  prv_reboot();
  cl_assert_equal_i(fake_flash_op_count(), ops);
  prv_assert_usable();
}

void test_pfs_convert__first_version_sector_rewrite_with_power_cuts(void) {
  prv_first_version_small_pages();
  fake_spi_flash_save(s_image, PART_FILESYSTEM_OFFSET, PART_FILESYSTEM_SIZE);
  prv_power_cut_sweep_with(true);
}

void test_pfs_convert__full_first_version_sector_is_rewritten_once_a_page_is_free(void) {
  prv_small_page_format();
  uint16_t pages[8];
  for (uint16_t i = 0; i < 8; i++) {
    pages[i] = i;
  }
  prv_new_file_at(pages, "a", 8 * NEW_PAYLOAD - FILEDATA_LEN - 1, 1, 0);
  for (uint16_t i = 0; i < 8; i++) {
    pages[i] = 8 + i;
  }
  TestFile *b = prv_new_file_at(pages, "b", 8 * NEW_PAYLOAD - FILEDATA_LEN - 1, 2, 0);
  const uint8_t tag = 0xfe;
  prv_raw_write(40 * PAGE_SIZE + offsetof(TestPageHeader, last_written), &tag, 1);

  prv_reboot();

  cl_assert(prv_sector_page_firmware_mounts());
  prv_assert_files();

  cl_assert_equal_i(pfs_remove("b"), S_SUCCESS);
  b->kept = false;
  prv_reboot();

  cl_assert(!prv_sector_page_firmware_mounts());
  prv_assert_sector_version(0, NEW_VERSION);
  prv_assert_files();
}

void test_pfs_convert__blank_first_sector_rewrites_the_next(void) {
  prv_small_page_format();
  const uint16_t far[] = {17, 40};
  prv_new_file_at(far, "far", NEW_PAYLOAD + 9, 1, 0);
  prv_raw_erase(0);
  cl_assert(prv_sector_page_firmware_mounts());

  prv_reboot();

  cl_assert(!prv_sector_page_firmware_mounts());
  prv_assert_sector_version(1, NEW_VERSION);
  prv_assert_files();
}

static void prv_empty_first_version_sector(uint16_t tagged_page) {
  prv_small_page_format();
  const uint8_t tag = 0xfe;
  prv_raw_write(tagged_page * PAGE_SIZE + offsetof(TestPageHeader, last_written), &tag, 1);
  const uint16_t far[] = {70, 71};
  prv_new_file_at(far, "far", NEW_PAYLOAD + 9, 1, 0);
  fake_spi_flash_save(s_image, PART_FILESYSTEM_OFFSET, PART_FILESYSTEM_SIZE);
}

static void prv_assert_empty_sector_rewrite_with_power_cuts(void) {
  uint32_t ops = fake_flash_op_count();
  prv_reboot();
  ops = fake_flash_op_count() - ops;
  prv_assert_sector_version(0, NEW_VERSION);

  for (volatile uint32_t cut = 0; cut < ops; cut++) {
    for (volatile int how = FakeFlashCutBefore; how <= FakeFlashCutBits; how++) {
      prv_restore();
      cl_assert(prv_boot_with_cut(cut, how));
      prv_reboot();
      prv_assert_files();
      cl_assert(!prv_sector_page_firmware_mounts());
      prv_assert_sector_version(0, NEW_VERSION);
    }
  }
  printf("operations %u\n", (unsigned)ops);
}

void test_pfs_convert__empty_first_version_sector_rewrite_with_power_cuts(void) {
  prv_empty_first_version_sector(90 * PAGES_PER_SECTOR + 3);
  prv_assert_empty_sector_rewrite_with_power_cuts();
}

void test_pfs_convert__first_version_gc_sector_rewrite_with_power_cuts(void) {
  prv_empty_first_version_sector(3);
  prv_assert_empty_sector_rewrite_with_power_cuts();
}

void test_pfs_convert__corrupt_page_where_sector_page_firmware_looks_gets_the_new_version(void) {
  prv_small_page_format();
  const uint16_t kept[] = {1, 2};
  prv_new_file_at(kept, "kept", NEW_PAYLOAD + 9, 1, 0);
  prv_page_header(0, 2, FLAGS_CONT, NO_PAGE, OLD_BAD_CRC);
  cl_assert(prv_sector_page_firmware_mounts());

  prv_reboot();

  cl_assert(!prv_sector_page_firmware_mounts());
  TestPageHeader hdr;
  prv_raw_read(0, &hdr, sizeof(hdr));
  cl_assert(pbl_crc32_legacy(&hdr, offsetof(TestPageHeader, hdr_crc)) != hdr.hdr_crc);
  prv_assert_files();
  const uint32_t ops = fake_flash_op_count();
  prv_reboot();
  cl_assert_equal_i(fake_flash_op_count(), ops);
}

void test_pfs_convert__garbage_collection_of_the_rewritten_sector_keeps_the_new_version(void) {
  prv_first_version_small_pages();
  prv_reboot();
  cl_assert(!prv_sector_page_firmware_mounts());

  cl_assert_equal_i(pfs_remove("small"), S_SUCCESS);
  for (int i = 0; i < s_num_files; i++) {
    if (strcmp(s_files[i].name, "small") == 0) {
      s_files[i].kept = false;
    }
  }
  test_force_garbage_collection(0);
  cl_assert(!prv_sector_page_firmware_mounts());
  prv_assert_sector_version(0, NEW_VERSION);

  prv_reboot();
  cl_assert(!prv_sector_page_firmware_mounts());
  prv_assert_files();
}
