/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fs/pfs/flash_translation.h>
#include <fs/pfs/pfs.h>

#define TEST_PAGE_SIZE      CONFIG_FS_PFS_PAGE_SIZE
#define TEST_PAGE_HDR_SIZE  28
#define TEST_PAGE_PAYLOAD   (TEST_PAGE_SIZE - TEST_PAGE_HDR_SIZE)
#define TEST_NUM_PAGES      (PART_FILESYSTEM_SIZE / TEST_PAGE_SIZE)
#define TEST_NUM_SECTORS    (PART_FILESYSTEM_SIZE / SECTOR_SIZE_BYTES)
#define TEST_PAGES_PER_SECT (SECTOR_SIZE_BYTES / TEST_PAGE_SIZE)
#define TEST_MAX_FILES      TEST_NUM_PAGES

typedef struct {
  char name[16];
  uint32_t size;
  uint8_t seed;
  bool live;
} TestFile;

static TestFile s_files[TEST_MAX_FILES];
static int s_num_files;
static uint8_t s_buf[4 * SECTOR_SIZE_BYTES];

extern void pfs_reset_all_state(void);
extern uint16_t test_get_file_start_page(int fd);
extern void test_force_garbage_collection(uint16_t start_page);

static void prv_fill(uint8_t *buf, uint32_t size, uint8_t seed) {
  for (uint32_t i = 0; i < size; i++) {
    buf[i] = (uint8_t)(seed + i * 7 + (i >> 9));
  }
}

static int prv_create(const char *name, uint32_t size, uint8_t seed) {
  cl_assert(size <= sizeof(s_buf));
  int fd = pfs_open(name, OP_FLAG_WRITE, FILE_TYPE_STATIC, size);
  if (fd < 0) {
    return fd;
  }
  prv_fill(s_buf, size, seed);
  cl_assert_equal_i(pfs_write(fd, s_buf, size), size);
  cl_assert_equal_i(pfs_close(fd), S_SUCCESS);
  return S_SUCCESS;
}

static void prv_assert_content(const char *name, uint32_t size, uint8_t seed) {
  static uint8_t expected[sizeof(s_buf)];
  int fd = pfs_open(name, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  cl_assert(fd >= 0);
  cl_assert_equal_i(pfs_get_file_size(fd), size);
  cl_assert_equal_i(pfs_read(fd, s_buf, size), size);
  prv_fill(expected, size, seed);
  cl_assert(memcmp(s_buf, expected, size) == 0);
  cl_assert_equal_i(pfs_close(fd), S_SUCCESS);
}

static void prv_assert_all_files(void) {
  for (int i = 0; i < s_num_files; i++) {
    if (s_files[i].live) {
      prv_assert_content(s_files[i].name, s_files[i].size, s_files[i].seed);
    } else {
      cl_assert_equal_i(pfs_open(s_files[i].name, OP_FLAG_READ, 0, 0), E_DOES_NOT_EXIST);
    }
  }
}

static void prv_reboot(void) {
  pfs_reset_all_state();
  pfs_init(true);
}

static uint32_t s_last_erases[TEST_NUM_SECTORS];
static int s_rounds_erased[TEST_NUM_SECTORS];

static uint32_t prv_sector_erases(uint32_t sector) {
  return fake_flash_sector_erase_count(PART_FILESYSTEM_OFFSET + sector * SECTOR_SIZE_BYTES);
}

static void prv_start_counting_erases(void) {
  for (uint32_t sector = 0; sector < TEST_NUM_SECTORS; sector++) {
    s_last_erases[sector] = prv_sector_erases(sector);
    s_rounds_erased[sector] = 0;
  }
}

static void prv_count_sector_erases(void) {
  for (uint32_t sector = 0; sector < TEST_NUM_SECTORS; sector++) {
    const uint32_t erases = prv_sector_erases(sector);
    s_rounds_erased[sector] += (erases != s_last_erases[sector]) ? 1 : 0;
    s_last_erases[sector] = erases;
  }
}

static int prv_min_rounds_erased(void) {
  int min = INT32_MAX;
  for (uint32_t sector = 0; sector < TEST_NUM_SECTORS; sector++) {
    min = MIN(min, s_rounds_erased[sector]);
  }
  return min;
}

void test_pfs_s29vs__initialize(void) {
  fake_spi_flash_init(0, PART_FILESYSTEM_OFFSET + PART_FILESYSTEM_SIZE);
  s_num_files = 0;
  prv_reboot();
}

void test_pfs_s29vs__cleanup(void) {
  fake_spi_flash_force_future_failure(0, nullptr);
  fake_spi_flash_cleanup();
}

void test_pfs_s29vs__capacity(void) {
  const uint32_t size = PART_FILESYSTEM_SIZE - SECTOR_SIZE_BYTES;
  cl_assert_equal_i(pfs_get_size(), size);
  cl_assert_equal_i(get_available_pfs_space(), (size * 8) / 10);
  cl_assert_equal_i(pfs_sector_optimal_size(1, 4), TEST_PAGE_PAYLOAD - 16 - 32 - 4);
  cl_assert_equal_i(pfs_sector_optimal_size(4096, 4), TEST_PAGE_PAYLOAD - 16 - 32 - 4);
  cl_assert_equal_i(pfs_sector_optimal_size(TEST_PAGE_PAYLOAD, 4),
                    (2 * TEST_PAGE_PAYLOAD) - 16 - 32 - 4);

  int created = 0;
  char name[16];
  while (true) {
    snprintf(name, sizeof(name), "f%d", created);
    if (prv_create(name, 10, created) != S_SUCCESS) {
      break;
    }
    created++;
  }

  cl_assert_equal_i(created, size / TEST_PAGE_SIZE);
  cl_assert_equal_i(get_available_pfs_space(), 0);
  prv_reboot();
  for (int i = 0; i < created; i += 97) {
    snprintf(name, sizeof(name), "f%d", i);
    prv_assert_content(name, 10, i);
  }
}

void test_pfs_s29vs__fill_delete_refill(void) {
  const uint32_t sizes[] = {
    10, 3000, TEST_PAGE_PAYLOAD - 100, TEST_PAGE_PAYLOAD + 1, 3 * TEST_PAGE_SIZE
  };
  int round = 0;
  int next = 0;

  prv_start_counting_erases();
  for (; round < 60 && prv_min_rounds_erased() < 5; round++) {
    while (s_num_files < TEST_MAX_FILES) {
      TestFile *f = &s_files[s_num_files];
      snprintf(f->name, sizeof(f->name), "r%d_%d", round, next);
      f->size = sizes[next % ARRAY_LENGTH(sizes)];
      f->seed = (uint8_t)(next * 13 + round);
      next++;
      if (prv_create(f->name, f->size, f->seed) != S_SUCCESS) {
        break;
      }
      f->live = true;
      s_num_files++;
    }

    if ((round % 3) == 2) {
      prv_reboot();
    }
    prv_assert_all_files();

    int kept = 0;
    for (int i = 0; i < s_num_files; i++) {
      if (((i + round) % 2) == 0) {
        cl_assert_equal_i(pfs_remove(s_files[i].name), S_SUCCESS);
      } else {
        s_files[kept++] = s_files[i];
      }
    }
    s_num_files = kept;
    prv_count_sector_erases();
  }

  printf("rounds %d, erases %u\n", round, (unsigned)fake_flash_erase_count());
  cl_assert(prv_min_rounds_erased() >= 5);
  prv_reboot();
  prv_assert_all_files();
}

void test_pfs_s29vs__file_larger_than_a_sector(void) {
  const uint32_t size = 3 * SECTOR_SIZE_BYTES + 1000;
  cl_assert_equal_i(prv_create("big", size, 0x42), S_SUCCESS);
  prv_assert_content("big", size, 0x42);

  int fd = pfs_open("big", OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  cl_assert(fd >= 0);
  const uint32_t off = size - 77;
  cl_assert_equal_i(pfs_seek(fd, off, FSeekSet), off);
  uint8_t tail[77];
  cl_assert_equal_i(pfs_read(fd, tail, sizeof(tail)), sizeof(tail));
  prv_fill(s_buf, size, 0x42);
  cl_assert(memcmp(tail, &s_buf[off], sizeof(tail)) == 0);
  pfs_close(fd);

  prv_reboot();
  prv_assert_content("big", size, 0x42);
}

static uint16_t prv_gc_setup(void) {
  uint16_t start_page = 0;
  s_num_files = 0;
  for (int i = 0; i < TEST_PAGES_PER_SECT; i++) {
    TestFile *f = &s_files[s_num_files++];
    snprintf(f->name, sizeof(f->name), "g%d", i);
    f->size = pfs_sector_optimal_size(1, strlen(f->name));
    f->seed = (uint8_t)i;
    f->live = true;
    cl_assert_equal_i(prv_create(f->name, f->size, f->seed), S_SUCCESS);
    if (i == 0) {
      int fd = pfs_open(f->name, OP_FLAG_READ, 0, 0);
      start_page = test_get_file_start_page(fd);
      pfs_close(fd);
    }
  }
  for (int i = 1; i < TEST_PAGES_PER_SECT; i += 2) {
    cl_assert_equal_i(pfs_remove(s_files[i].name), S_SUCCESS);
    s_files[i].live = false;
  }
  return start_page;
}

void test_pfs_s29vs__power_cut_during_gc(void) {
  volatile int cuts = 0;
  volatile bool completed = false;

  for (volatile int n = 0; !completed; n += (n < 512) ? 1 : 97) {
    fake_spi_flash_init(0, PART_FILESYSTEM_OFFSET + PART_FILESYSTEM_SIZE);
    prv_reboot();
    const uint16_t start_page = prv_gc_setup();

    jmp_buf cut;
    if (setjmp(cut) == 0) {
      fake_spi_flash_force_future_failure(n, &cut);
      test_force_garbage_collection(start_page);
      completed = true;
    } else {
      cuts++;
    }
    fake_spi_flash_force_future_failure(0, nullptr);

    prv_reboot();
    prv_assert_all_files();
    cl_assert_equal_i(prv_create("after", 100, 0x77), S_SUCCESS);
    prv_assert_content("after", 100, 0x77);
  }

  printf("cuts %d\n", cuts);
  cl_assert(cuts > 1000);
}

void test_pfs_s29vs__gc_wear_spreads_when_runs_of_files_are_deleted(void) {
  const uint32_t size = TEST_PAGE_PAYLOAD - 100;
  int next = 0;

  prv_start_counting_erases();
  for (int round = 0; round < 20; round++) {
    while (get_available_pfs_space() >= size) {
      TestFile *f = &s_files[s_num_files++];
      snprintf(f->name, sizeof(f->name), "b%d", next);
      f->size = size;
      f->seed = (uint8_t)next;
      f->live = true;
      next++;
      cl_assert_equal_i(prv_create(f->name, f->size, f->seed), S_SUCCESS);
    }

    int kept = 0;
    for (int i = 0; i < s_num_files; i++) {
      if ((((i / TEST_PAGES_PER_SECT) + round) % 2) == 0) {
        cl_assert_equal_i(pfs_remove(s_files[i].name), S_SUCCESS);
      } else {
        s_files[kept++] = s_files[i];
      }
    }
    s_num_files = kept;
  }

  uint32_t total = 0;
  uint32_t max = 0;
  for (uint32_t sector = 0; sector < TEST_NUM_SECTORS; sector++) {
    const uint32_t erases = prv_sector_erases(sector) - s_last_erases[sector];
    total += erases;
    max = MAX(max, erases);
  }
  printf("erases %u, most in one sector %u\n", (unsigned)total, (unsigned)max);
  cl_assert(max * TEST_NUM_SECTORS <= 4 * total);
  prv_reboot();
  prv_assert_all_files();
}
