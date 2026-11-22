/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/reset/stm32f4.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32f4_rcc.h>

static const struct pbl_reset_stm32f4 *prv_rcc(const struct pbl_reset_controller *ctrl) {
  return container_of(ctrl, const struct pbl_reset_stm32f4, ctrl);
}

// Line id: (register offset * 8) + bit
static int prv_reg(const struct pbl_reset_controller *ctrl, uint32_t id, uintptr_t *reg) {
  uint32_t offset = (id >> 5) << 2;

  if (offset < STM32F4_RCC_RSTR_OFFSET(0) || offset == STM32F4_RCC_RSTR_RESERVED_OFFSET) {
    return -EINVAL;
  }

  *reg = prv_rcc(ctrl)->base + offset;

  return 0;
}

static int prv_assert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  uintptr_t reg;
  int ret;

  ret = prv_reg(ctrl, id, &reg);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(0U, PBL_BIT(id & 31U), reg);

  return 0;
}

static int prv_deassert(const struct pbl_reset_controller *ctrl, uint32_t id) {
  uintptr_t reg;
  int ret;

  ret = prv_reg(ctrl, id, &reg);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(PBL_BIT(id & 31U), 0U, reg);

  return 0;
}

const struct pbl_reset_controller_ops pbl_reset_stm32f4_ops = {
  .assert = prv_assert,
  .deassert = prv_deassert,
};

#include PBL_DT_INSTANCES(st_stm32f42xx_rcc_reset)
