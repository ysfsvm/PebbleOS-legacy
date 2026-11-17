/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/stm32f4.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <dt-bindings/clock/stm32fx-clock.h>
#include <hardware/stm32f4_rcc.h>

// The bus clocks, after the clocks of the binding
#define ID_HSI    STM32F4_CLK_ID(1, CLK_HSI)
#define ID_SYSCLK STM32F4_CLK_ID(1, CLK_SYSCLK)
#define ID_HCLK   STM32F4_CLK_ID(1, END_PRIMARY_CLK)
#define ID_PCLK1  STM32F4_CLK_ID(1, END_PRIMARY_CLK + 1)
#define ID_PCLK2  STM32F4_CLK_ID(1, END_PRIMARY_CLK + 2)
#define ID_COUNT  STM32F4_CLK_ID(1, END_PRIMARY_CLK + 3)

// Gates are 32 per bus: AHB1, AHB2, AHB3, APB1, APB2
#define GATE_BUS(id) ((id) >> 5)
#define BUS_APB1     3U
#define BUS_APB2     4U

#define MHZ_PER_HZ 1000ULL

struct pbl_clk_stm32f4 {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
};

static const struct pbl_clk_stm32f4 *prv_rcc(const struct pbl_clk_controller *ctrl) {
  return container_of(ctrl, const struct pbl_clk_stm32f4, ctrl);
}

// The clocks between the oscillators and the gates, which always run
static bool prv_is_bus_clk(uint32_t id) {
  return id == ID_HSI || id == ID_SYSCLK || id == ID_HCLK || id == ID_PCLK1 || id == ID_PCLK2;
}

static uintptr_t prv_enr(const struct pbl_clk_stm32f4 *rcc, uint32_t id) {
  uint32_t bus = GATE_BUS(id);

  // APB1ENR follows a reserved word
  return rcc->base + STM32F4_RCC_ENR_OFFSET(bus >= BUS_APB1 ? bus + 1U : bus);
}

// The class calls the ops with interrupts locked, which keeps the read-modify-writes of the
// enable registers whole against soc_stm32_clock_set(), which locks them too.
static int prv_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_stm32f4 *rcc = prv_rcc(ctrl);
  uintptr_t enr;

  if (id >= STM32F4_CLK_NUM_GATES) {
    return prv_is_bus_clk(id) ? 0 : -ENOTSUP;
  }

  enr = prv_enr(rcc, id);
  pbl_writel(pbl_readl(enr) | PBL_BIT(id & 31U), enr);
  // A peripheral can be accessed two bus cycles after its clock is enabled
  (void)pbl_readl(enr);

  return 0;
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_stm32f4 *rcc = prv_rcc(ctrl);
  uintptr_t enr;

  if (id >= STM32F4_CLK_NUM_GATES) {
    return prv_is_bus_clk(id) ? 0 : -ENOTSUP;
  }

  enr = prv_enr(rcc, id);
  pbl_writel(pbl_readl(enr) & ~PBL_BIT(id & 31U), enr);

  return 0;
}

static uint32_t prv_ahb_div(uint32_t cfgr) {
  uint32_t hpre = PBL_FIELD_GET(STM32F4_RCC_CFGR_HPRE_MASK, cfgr);

  if (hpre < 8U) {
    return 1U;
  }
  // 2, 4, 8, 16, then 64 to 512: there is no 32
  return 1U << (hpre - 7U + (hpre >= 12U ? 1U : 0U));
}

static uint32_t prv_apb_div(uint32_t ppre) {
  return ppre < 4U ? 1U : 1U << (ppre - 3U);
}

// SYSCLK is the root: STOP wakes on the HSI before the PLL is back, so its source changes
// under the class, which counts the users of a parent.
static int prv_sysclk_mhz(const struct pbl_clk_stm32f4 *rcc, uint64_t *rate_mhz) {
  uint32_t cfgr = pbl_readl(rcc->base + STM32F4_RCC_CFGR_OFFSET);
  uint32_t pllcfgr = pbl_readl(rcc->base + STM32F4_RCC_PLLCFGR_OFFSET);
  uint32_t m, n, p;

  switch (PBL_FIELD_GET(STM32F4_RCC_CFGR_SWS_MASK, cfgr)) {
    case STM32F4_RCC_CFGR_SWS_HSI:
      *rate_mhz = STM32F4_HSI_HZ * MHZ_PER_HZ;
      return 0;
    case STM32F4_RCC_CFGR_SWS_PLL:
      // No board runs the PLL from an HSE
      if ((pllcfgr & STM32F4_RCC_PLLCFGR_PLLSRC_HSE) != 0U) {
        return -ENOTSUP;
      }
      m = PBL_FIELD_GET(STM32F4_RCC_PLLCFGR_PLLM_MASK, pllcfgr);
      n = PBL_FIELD_GET(STM32F4_RCC_PLLCFGR_PLLN_MASK, pllcfgr);
      p = (PBL_FIELD_GET(STM32F4_RCC_PLLCFGR_PLLP_MASK, pllcfgr) + 1U) * 2U;
      if (m == 0U) {
        return -EIO;
      }
      *rate_mhz = (uint64_t)STM32F4_HSI_HZ * MHZ_PER_HZ * n / (m * p);
      return 0;
    default:
      return -ENOTSUP;
  }
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  const struct pbl_clk_stm32f4 *rcc = prv_rcc(ctrl);
  uint32_t cfgr = pbl_readl(rcc->base + STM32F4_RCC_CFGR_OFFSET);

  if (id < STM32F4_CLK_NUM_GATES) {
    *rate_mhz = parent_mhz;
    return 0;
  }

  switch (id) {
    case ID_HSI:
      *rate_mhz = STM32F4_HSI_HZ * MHZ_PER_HZ;
      return 0;
    case ID_SYSCLK:
      return prv_sysclk_mhz(rcc, rate_mhz);
    case ID_HCLK:
      *rate_mhz = parent_mhz / prv_ahb_div(cfgr);
      return 0;
    case ID_PCLK1:
      *rate_mhz = parent_mhz / prv_apb_div(PBL_FIELD_GET(STM32F4_RCC_CFGR_PPRE1_MASK, cfgr));
      return 0;
    case ID_PCLK2:
      *rate_mhz = parent_mhz / prv_apb_div(PBL_FIELD_GET(STM32F4_RCC_CFGR_PPRE2_MASK, cfgr));
      return 0;
    default:
      return -ENOTSUP;
  }
}

static int prv_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          struct pbl_clk *parent) {
  uint32_t parent_id;

  if (id < STM32F4_CLK_NUM_GATES) {
    switch (GATE_BUS(id)) {
      case BUS_APB1:
        parent_id = ID_PCLK1;
        break;
      case BUS_APB2:
        parent_id = ID_PCLK2;
        break;
      default:
        parent_id = ID_HCLK;
        break;
    }
  } else if (id == ID_PCLK1 || id == ID_PCLK2) {
    parent_id = ID_HCLK;
  } else if (id == ID_HCLK) {
    parent_id = ID_SYSCLK;
  } else {
    return -ENOENT;
  }

  *parent = (struct pbl_clk)PBL_CLK(ctrl, parent_id);

  return 0;
}

static const struct pbl_clk_controller_ops s_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
};

#define PBL_CLK_STM32F4_DEFINE(sym, _name, _base)                                           \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, ID_COUNT);                                           \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_stm32f4, struct pbl_clk_controller, ctrl) {       \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, nullptr, nullptr, &s_ops), .base = (_base), \
  }

#include PBL_DT_INSTANCES(st_stm32f42xx_rcc)
