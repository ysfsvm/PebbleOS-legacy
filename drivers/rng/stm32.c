/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/rng.h>
#include <pbl/drivers/rng/stm32.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/mutex.h>

#include <hardware/stm32f4_rng.h>

// Give up on a peripheral that never becomes ready (e.g. not emulated).
#define MAX_POLLS 100000U

#define SR_SEED_ERROR  (STM32F4_RNG_SR_SECS | STM32F4_RNG_SR_SEIS)
#define SR_CLOCK_ERROR (STM32F4_RNG_SR_CECS | STM32F4_RNG_SR_CEIS)

extern const struct pbl_rng_stm32 pbl_rng_stm32_inst;

static const struct pbl_rng_stm32 *const s_rng = &pbl_rng_stm32_inst;

static PBL_MUTEX_DEFINE(s_rng_lock);

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(s_rng->base + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, s_rng->base + offset);
}

static inline void prv_setbits(uint32_t offset, uint32_t bits) {
  prv_putreg(offset, prv_getreg(offset) | bits);
}

static inline void prv_clrbits(uint32_t offset, uint32_t bits) {
  prv_putreg(offset, prv_getreg(offset) & ~bits);
}

bool rng_rand(uint32_t *rand_out) {
  PBL_ASSERTN(rand_out);

  bool success = false;
  // Seed errors allowed before giving up
  uint8_t attempts_left = 3;
  uint8_t non_equal_count = 0;
  uint32_t previous_value = 0;
  uint32_t polls = 0;

  pbl_mutex_lock(&s_rng_lock, PBL_FOREVER);
  PBL_ASSERTN(pbl_device_init(&s_rng->clk.ctrl->dev) == 0);
  PBL_ASSERTN(pbl_clk_enable(&s_rng->clk) == 0);
  prv_setbits(STM32F4_RNG_CR_OFFSET, STM32F4_RNG_CR_RNGEN);

  while (true) {
    while (attempts_left && polls++ < MAX_POLLS) {
      const uint32_t status = prv_getreg(STM32F4_RNG_SR_OFFSET);

      // RNG clock not 48 MHz
      PBL_ASSERTN((status & SR_CLOCK_ERROR) == 0);

      if (status & SR_SEED_ERROR) {
        // Clear SEI and restart the RNG
        prv_clrbits(STM32F4_RNG_SR_OFFSET, STM32F4_RNG_SR_SEIS);
        prv_clrbits(STM32F4_RNG_CR_OFFSET, STM32F4_RNG_CR_RNGEN);
        prv_setbits(STM32F4_RNG_CR_OFFSET, STM32F4_RNG_CR_RNGEN);
        non_equal_count = 0;
        previous_value = 0;
        --attempts_left;
        continue;
      }

      if (status & STM32F4_RNG_SR_DRDY) {
        break;
      }
    }

    if (!attempts_left || polls >= MAX_POLLS) {
      break;
    }

    // FIPS 140-2 continuous test: never use the first value after enabling,
    // and fail if two consecutive values are equal.
    *rand_out = prv_getreg(STM32F4_RNG_DR_OFFSET);
    if (*rand_out != previous_value) {
      ++non_equal_count;
      if (non_equal_count >= 2) {
        success = true;
        break;
      }
    }
    previous_value = *rand_out;
  }

  prv_clrbits(STM32F4_RNG_CR_OFFSET, STM32F4_RNG_CR_RNGEN);
  (void)pbl_clk_disable(&s_rng->clk);
  pbl_mutex_unlock(&s_rng_lock);

  return success;
}

#include PBL_DT_INSTANCES(st_stm32_rng)
