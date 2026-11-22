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
} TestFile;

static TestFile s_files[2 * NUM_SECTORS];
static int s_num_files;
static bool s_used[NUM_SECTORS];
static uint32_t s_next_sector;
static uint8_t *s_image;
static uint8_t *s_stream;

extern void pfs_reset_all_state(void);

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

static void prv_old_header(uint16_t sector, uint8_t flags, uint16_t next, unsigned opts) {
  TestPageHeader hdr;
  memset(&hdr, 0xff, sizeof(hdr));
  hdr.version = 0x5001;
  hdr.page_flags = flags;
  hdr.erase_count = 1 + (sector % 13);
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
  prv_raw_write(sector * SECTOR_SIZE_BYTES, &hdr, sizeof(hdr));
}

static void prv_old_deleted(uint16_t sector, uint8_t flags) {
  const uint8_t deleted = flags & ~FLAG_DELETED;
  prv_raw_write(sector * SECTOR_SIZE_BYTES + offsetof(TestPageHeader, page_flags), &deleted, 1);
}

static void prv_old_erased(uint16_t sector) {
  s_used[sector] = true;
  TestPageHeader hdr;
  memset(&hdr, 0xff, sizeof(hdr));
  hdr.version = 0x5001;
  hdr.page_flags = FLAGS_ERASED;
  hdr.erase_count = 7;
  prv_raw_write(sector * SECTOR_SIZE_BYTES, &hdr, offsetof(TestPageHeader, rsvd1));
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

static TestFile *prv_old_file_in(const uint16_t *sectors, const char *name, uint32_t size,
                                 uint8_t seed, unsigned opts) {
  const size_t namelen = strlen(name);
  const uint32_t len = FILEDATA_LEN + namelen + size;
  const uint32_t n = prv_old_sectors(name, size);

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

  TestFile *f = &s_files[s_num_files++];
  snprintf(f->name, sizeof(f->name), "%s", name);
  f->size = size;
  f->seed = seed;
  f->kept = (opts & (OLD_GONE | OLD_BAD_CHAIN | OLD_DROPPED)) == 0;
  return f;
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
      if (buf[i] != prv_byte(f->seed, done + i)) {
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
    cl_assert((hdr.version == 0x5001) || deleted);

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
  fake_spi_flash_force_op_failure(0, FakeFlashCutBefore, nullptr);
  fake_spi_flash_cleanup();
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
    prv_raw_read(sector * SECTOR_SIZE_BYTES + PAGE_SIZE, &hdr, sizeof(hdr));
    if (hdr.version == 0x5001) {
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

void test_pfs_convert__power_cut_at_every_operation(void) {
  prv_mixed_filesystem();
  fake_spi_flash_save(s_image, PART_FILESYSTEM_OFFSET, PART_FILESYSTEM_SIZE);

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
      prv_assert_files();
      prv_assert_new_layout();
    }

    prv_restore();
    cl_assert(prv_boot_with_cut(cut, FakeFlashCutBits));
    if (prv_boot_with_cut((cut * 7919 + 13) % ops,
                          ((cut % 2) == 0) ? FakeFlashCutHalf : FakeFlashCutBits)) {
      cuts++;
    }
    prv_reboot();
    prv_assert_files();
    prv_assert_new_layout();
  }

  printf("operations %u, cuts %u\n", (unsigned)ops, (unsigned)cuts);
  cl_assert(cuts >= 3 * ops);
}
