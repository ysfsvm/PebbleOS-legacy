/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include "recorder.h"
#include "traces.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/backlight/pwm.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/drivers/pwm.h>
#include <pbl/drivers/pwm/stm32.h>
#include <pbl/drivers/vibe.h>
#include <pbl/drivers/vibe/pwm.h>
#include <pbl/kernel/irq.h>
#include <pbl/services/battery/battery_monitor.h>
#include <pbl/services/battery/battery_state.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <clar.h>
#include <devicetree/devices.h>
#include <hardware/stm32f4_tim.h>

#define RCC_BASE   0x40023800U
#define GPIOB_BASE 0x40020400U
#define GPIOF_BASE 0x40021400U
#define TIM10_BASE 0x40014400U
#define TIM12_BASE 0x40001800U
#define NUM_REGS   64

#define RCC_PLLCFGR 0x04U
#define RCC_CFGR    0x08U
#define RCC_AHB1ENR 0x30U
#define RCC_AHB2ENR 0x34U
#define RCC_APB1ENR 0x40U
#define RCC_APB2ENR 0x44U

#define GPIO_MODER   0x00U
#define GPIO_OTYPER  0x04U
#define GPIO_OSPEEDR 0x08U
#define GPIO_PUPDR   0x0cU
#define GPIO_ODR     0x14U
#define GPIO_BSRR    0x18U
#define GPIO_AFRL    0x20U
#define GPIO_AFRH    0x24U

#define PLLCFGR_FIRMWARE ((4U << 24) | (192U << 6) | 16U)
#define CFGR_FIRMWARE    ((4U << 13) | (5U << 10) | (2U << 2) | 2U)
#define CFGR_UNDIVIDED   ((2U << 2) | 2U)

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;
static bool s_apb_undivided;

static uint32_t prv_reset_value(uintptr_t addr) {
  switch (addr) {
    case GPIOB_BASE + GPIO_MODER:
      return 0x00000280U;
    case GPIOB_BASE + GPIO_OSPEEDR:
      return 0x000000c0U;
    case GPIOB_BASE + GPIO_PUPDR:
      return 0x00000100U;
    case RCC_BASE + RCC_PLLCFGR:
      return PLLCFGR_FIRMWARE;
    case RCC_BASE + RCC_CFGR:
      return s_apb_undivided ? CFGR_UNDIVIDED : CFGR_FIRMWARE;
    default:
      return 0U;
  }
}

static uint32_t *prv_reg(uintptr_t addr) {
  for (int i = 0; i < s_num_regs; i++) {
    if (s_regs[i].addr == addr) {
      return &s_regs[i].val;
    }
  }
  cl_assert(s_num_regs < NUM_REGS);
  s_regs[s_num_regs].addr = addr;
  s_regs[s_num_regs].val = prv_reset_value(addr);
  return &s_regs[s_num_regs++].val;
}

enum tim_field {
  F_CR1,
  F_CR2,
  F_SMCR,
  F_DIER,
  F_EGR,
  F_CCMR1,
  F_CCMR2,
  F_CCER,
  F_PSC,
  F_ARR,
  F_CCR1,
  F_CCR2,
  F_CCR3,
  F_CCR4,
  F_BDTR,
  NUM_FIELDS,
};

static const char *const s_field_names[NUM_FIELDS] = {
  "CR1", "CR2", "SMCR", "DIER", "EGR",  "CCMR1", "CCMR2", "CCER",
  "PSC", "ARR", "CCR1", "CCR2", "CCR3", "CCR4",  "BDTR",
};

static const uint32_t s_field_offsets[NUM_FIELDS] = {
  STM32F4_TIM_CR1_OFFSET,   STM32F4_TIM_CR2_OFFSET,  STM32F4_TIM_SMCR_OFFSET,
  STM32F4_TIM_DIER_OFFSET,  STM32F4_TIM_EGR_OFFSET,  STM32F4_TIM_CCMR1_OFFSET,
  STM32F4_TIM_CCMR2_OFFSET, STM32F4_TIM_CCER_OFFSET, STM32F4_TIM_PSC_OFFSET,
  STM32F4_TIM_ARR_OFFSET,   STM32F4_TIM_CCR1_OFFSET, STM32F4_TIM_CCR2_OFFSET,
  STM32F4_TIM_CCR3_OFFSET,  STM32F4_TIM_CCR4_OFFSET, STM32F4_TIM_BDTR_OFFSET,
};

struct tim {
  const char *name;
  uintptr_t base;
  uint32_t gate_off;
  uint32_t gate_bit;
  uint32_t seen[NUM_FIELDS];
};

static struct tim s_tims[] = {
  {.name = "tim10", .base = TIM10_BASE, .gate_off = RCC_APB2ENR, .gate_bit = 17U},
  {.name = "tim12", .base = TIM12_BASE, .gate_off = RCC_APB1ENR, .gate_bit = 6U},
};

static uint32_t prv_tim_field(const struct tim *t, enum tim_field f) {
  return *prv_reg(t->base + s_field_offsets[f]);
}

static struct tim *prv_tim_at(uintptr_t addr) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_tims); i++) {
    if (addr >= s_tims[i].base && addr < s_tims[i].base + 0x400U) {
      return &s_tims[i];
    }
  }
  return NULL;
}

static void prv_tims_reset(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_tims); i++) {
    memset(s_tims[i].seen, 0, sizeof(s_tims[i].seen));
  }
}

static void prv_tims_snapshot(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_tims); i++) {
    struct tim *t = &s_tims[i];
    char line[160];
    int len = snprintf(line, sizeof(line), "%s", t->name);
    bool changed = false;

    for (int f = 0; f < NUM_FIELDS; f++) {
      const uint32_t val = prv_tim_field(t, (enum tim_field)f);

      if (val != t->seen[f]) {
        len += snprintf(&line[len], sizeof(line) - (size_t)len, " %s %04x", s_field_names[f], val);
        t->seen[f] = val;
        changed = true;
      }
    }
    if (changed) {
      m_trace_line("%s", line);
    }
  }
}

static void m_event(const char *fmt, ...) {
  char line[160];
  va_list ap;

  prv_tims_snapshot();
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  m_trace_line("%s", line);
}

static const char *prv_gate_name(uint32_t off, unsigned int bit, char *buf, size_t len) {
  if (off == RCC_AHB1ENR && bit == 1U) {
    return "gpiob";
  }
  if (off == RCC_AHB1ENR && bit == 5U) {
    return "gpiof";
  }
  if (off == RCC_APB1ENR && bit == 6U) {
    return "tim12";
  }
  if (off == RCC_APB2ENR && bit == 17U) {
    return "tim10";
  }
  snprintf(buf, len, "%02x.%u", (unsigned int)off, bit);
  return buf;
}

static void prv_rcc_write(uint32_t off, uint32_t val) {
  uint32_t *reg = prv_reg(RCC_BASE + off);
  const uint32_t changed = *reg ^ val;

  *reg = val;
  if (off != RCC_AHB1ENR && off != RCC_AHB2ENR && off != RCC_APB1ENR && off != RCC_APB2ENR) {
    return;
  }
  for (unsigned int bit = 0U; bit < 32U; bit++) {
    if ((changed & PBL_BIT(bit)) != 0U) {
      char buf[16];

      m_event("clk %s %s", prv_gate_name(off, bit, buf, sizeof(buf)),
              (val & PBL_BIT(bit)) != 0U ? "on" : "off");
    }
  }
}

struct pin {
  uintptr_t port;
  unsigned int num;
  const char *name;
};

static const struct pin s_pins[] = {
  {GPIOB_BASE, 8U, "pb8"},
  {GPIOB_BASE, 14U, "pb14"},
  {GPIOF_BASE, 4U, "pf4"},
};

static void prv_gpio_write(uintptr_t port, uint32_t off, uint32_t val) {
  static const char *const modes[] = {"in", "out", "af", "an"};
  static const char *const pulls[] = {"none", "up", "down", "rsvd"};
  uint32_t before[10];
  uint32_t after[10];

  for (unsigned int i = 0U; i < 10U; i++) {
    before[i] = *prv_reg(port + 4U * i);
  }
  if (off == GPIO_BSRR) {
    uint32_t *odr = prv_reg(port + GPIO_ODR);
    *odr = (*odr | (val & 0xffffU)) & ~(val >> 16);
  } else {
    *prv_reg(port + off) = val;
  }
  for (unsigned int i = 0U; i < 10U; i++) {
    after[i] = *prv_reg(port + 4U * i);
  }

  for (size_t i = 0; i < ARRAY_LENGTH(s_pins); i++) {
    const struct pin *p = &s_pins[i];
    const unsigned int n = p->num;
    const unsigned int afr = (n < 8U ? GPIO_AFRL : GPIO_AFRH) / 4U;
    const unsigned int af_shift = 4U * (n % 8U);

    if (p->port != port) {
      continue;
    }
    if (((before[afr] ^ after[afr]) >> af_shift) & 0xfU) {
      m_event("%s af %u", p->name, (after[afr] >> af_shift) & 0xfU);
    }
    if (((before[1] ^ after[1]) >> n) & 1U) {
      m_event("%s %s", p->name, ((after[1] >> n) & 1U) ? "open-drain" : "push-pull");
    }
    if (((before[2] ^ after[2]) >> (2U * n)) & 3U) {
      m_event("%s speed %u", p->name, (after[2] >> (2U * n)) & 3U);
    }
    if (((before[3] ^ after[3]) >> (2U * n)) & 3U) {
      m_event("%s pull %s", p->name, pulls[(after[3] >> (2U * n)) & 3U]);
    }
    if (((before[5] ^ after[5]) >> n) & 1U) {
      m_event("%s level %u", p->name, (after[5] >> n) & 1U);
    }
    if (((before[0] ^ after[0]) >> (2U * n)) & 3U) {
      m_event("%s %s", p->name, modes[(after[0] >> (2U * n)) & 3U]);
    }
  }
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  return *prv_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  const struct tim *t = prv_tim_at(addr);

  if (t != NULL && (*prv_reg(RCC_BASE + t->gate_off) & PBL_BIT(t->gate_bit)) == 0U) {
    m_trace_line("VIOLATION %s written with its clock off", t->name);
  }
  if (addr >= RCC_BASE && addr < RCC_BASE + 0x400U) {
    prv_rcc_write(addr - RCC_BASE, val);
    return;
  }
  if ((addr >= GPIOB_BASE && addr < GPIOB_BASE + 0x400U) ||
      (addr >= GPIOF_BASE && addr < GPIOF_BASE + 0x400U)) {
    prv_gpio_write(addr & ~0x3ffU, addr & 0x3ffU, val);
    return;
  }
  *prv_reg(addr) = val;
}

static bool s_lockout;

void pbl_irq_enable(pbl_irq_t irq) {
}

void pbl_irq_disable(pbl_irq_t irq) {
}

void soc_stm32_stop_block(void) {
  m_event("stop block");
}

void soc_stm32_stop_release(void) {
  m_event("stop release");
}

bool battery_monitor_critical_lockout(void) {
  return s_lockout;
}

BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){0};
}

uint16_t battery_state_get_voltage(void) {
  return 3800U;
}

uint16_t pmic_get_vsys(void) {
  return 4200U;
}

static const struct pbl_pwm_controller *const s_tim12_ctrl = &dt_pwm12;
static const struct pbl_pwm_controller *const s_tim10_ctrl = &dt_pwm10;
static const struct pbl_backlight *const s_bl = &dt_backlight;
static const struct pbl_vibe *const s_vb = &dt_vibrator;

static void prv_devices_reset(void) {
  const struct pbl_device *devs[] = {
    &dt_rcc.dev,        &dt_gpiob.dev,      &dt_gpiof.dev, &dt_gpiob_intc.dev, &dt_gpiof_intc.dev,
    &s_tim12_ctrl->dev, &s_tim10_ctrl->dev, &s_bl->dev,    &s_vb->dev,
  };

  for (size_t i = 0; i < ARRAY_LENGTH(devs); i++) {
    *devs[i]->state = (struct pbl_device_state){0};
  }
  memset(dt_rcc.use_counts, 0, dt_rcc.num_clks);
  memset(s_tim12_ctrl->states, 0, sizeof(s_tim12_ctrl->states[0]) * s_tim12_ctrl->num_channels);
  memset(s_tim10_ctrl->states, 0, sizeof(s_tim10_ctrl->states[0]) * s_tim10_ctrl->num_channels);
  memset(container_of(s_tim12_ctrl, const struct pbl_pwm_stm32, ctrl)->state, 0,
         sizeof(struct pbl_pwm_stm32_state));
  memset(container_of(s_tim10_ctrl, const struct pbl_pwm_stm32, ctrl)->state, 0,
         sizeof(struct pbl_pwm_stm32_state));
  s_bl->state->brightness = 0U;
}

static void prv_dump_trace(const char *name, const char *trace) {
  const char *path = getenv("PWM_TRACE_OUT");

  if (path == NULL) {
    return;
  }
  FILE *f = fopen(path, "a");
  cl_assert(f != NULL);
  fprintf(f, "#define %s \\\n", name);
  for (const char *line = trace; *line != '\0';) {
    const char *end = strchr(line, '\n');
    fprintf(f, "  \"%.*s\\n\" \\\n", (int)(end - line), line);
    line = end + 1;
  }
  fprintf(f, "  \"\"\n\n");
  fclose(f);
}

#define CHECK_TRACE(name)               \
  do {                                  \
    prv_tims_snapshot();                \
    const char *trace = m_trace_stop(); \
    prv_dump_trace(#name, trace);       \
    cl_assert_equal_s(trace, name);     \
  } while (0)

static void prv_step(const char *fmt, ...) {
  char line[64];
  va_list ap;

  prv_tims_snapshot();
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  m_trace_line("= %s", line);
}

static void prv_trace_start(void) {
  prv_tims_snapshot();
  m_trace_start();
}

static void prv_init_devices(void) {
  cl_assert_equal_i(pbl_device_init(&s_bl->dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_vb->dev), 0);
}

static void prv_brightness(uint8_t brightness) {
  prv_step("backlight %u", brightness);
  cl_assert_equal_i(pbl_backlight_set_brightness(s_bl, brightness), 0);
}

static void prv_vibe_start(int8_t strength) {
  prv_step("vibe %d", strength);
  cl_assert_equal_i(pbl_vibe_start(s_vb, strength), 0);
}

static void prv_vibe_stop(void) {
  prv_step("vibe stop");
  cl_assert_equal_i(pbl_vibe_stop(s_vb), 0);
}

void test_pwm_stm32__initialize(void) {
  s_apb_undivided = false;
  s_lockout = false;
  s_trace_on = false;
  memset(s_regs, 0, sizeof(s_regs));
  s_num_regs = 0;
  prv_tims_reset();
  prv_devices_reset();
  cl_assert_equal_i(pbl_device_init(&dt_rcc.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_gpiob.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_gpiof.dev), 0);
}

void test_pwm_stm32__cleanup(void) {
  s_trace_on = false;
}

void test_pwm_stm32__init(void) {
  prv_trace_start();
  prv_init_devices();
  CHECK_TRACE(TRACE_INIT);
}

void test_pwm_stm32__backlight_on_off(void) {
  prv_init_devices();
  prv_trace_start();
  prv_brightness(80);
  prv_brightness(0);
  prv_brightness(0);
  prv_brightness(100);
  prv_brightness(0);
  CHECK_TRACE(TRACE_BACKLIGHT_ON_OFF);
}

void test_pwm_stm32__backlight_every_level(void) {
  prv_init_devices();
  prv_trace_start();
  for (unsigned int b = 1U; b <= PBL_BACKLIGHT_BRIGHTNESS_MAX; b++) {
    prv_brightness((uint8_t)b);
  }
  prv_brightness(0);
  CHECK_TRACE(TRACE_BACKLIGHT_EVERY_LEVEL);
}

void test_pwm_stm32__backlight_fade(void) {
  prv_init_devices();
  prv_brightness(80);
  prv_trace_start();
  for (int intensity = 95; intensity > 0; intensity -= 5) {
    prv_brightness((uint8_t)DIVIDE_CEIL(intensity * 80, 100));
  }
  prv_brightness(0);
  CHECK_TRACE(TRACE_BACKLIGHT_FADE);
}

void test_pwm_stm32__backlight_apb_undivided(void) {
  s_apb_undivided = true;
  *prv_reg(RCC_BASE + RCC_CFGR) = CFGR_UNDIVIDED;
  prv_init_devices();
  prv_trace_start();
  prv_brightness(80);
  prv_brightness(0);
  CHECK_TRACE(TRACE_BACKLIGHT_APB_UNDIVIDED);
}

void test_pwm_stm32__vibe_on_off(void) {
  prv_init_devices();
  prv_trace_start();
  prv_vibe_start(VIBE_STRENGTH_MAX);
  prv_vibe_stop();
  prv_vibe_stop();
  prv_vibe_start(50);
  prv_vibe_start(VIBE_STRENGTH_MIN);
  prv_vibe_stop();
  prv_vibe_start(VIBE_STRENGTH_OFF);
  prv_vibe_stop();
  CHECK_TRACE(TRACE_VIBE_ON_OFF);
}

void test_pwm_stm32__vibe_every_strength(void) {
  prv_init_devices();
  prv_trace_start();
  for (int s = VIBE_STRENGTH_MIN; s <= VIBE_STRENGTH_MAX; s++) {
    prv_vibe_start((int8_t)s);
  }
  prv_vibe_stop();
  CHECK_TRACE(TRACE_VIBE_EVERY_STRENGTH);
}

void test_pwm_stm32__vibe_critical_battery(void) {
  prv_init_devices();
  prv_trace_start();
  s_lockout = true;
  prv_vibe_start(VIBE_STRENGTH_MAX);
  s_lockout = false;
  prv_vibe_start(VIBE_STRENGTH_MAX);
  s_lockout = true;
  prv_vibe_start(VIBE_STRENGTH_MAX);
  CHECK_TRACE(TRACE_VIBE_CRITICAL_BATTERY);
}

void test_pwm_stm32__backlight_and_vibe(void) {
  prv_init_devices();
  prv_trace_start();
  prv_brightness(80);
  prv_vibe_start(VIBE_STRENGTH_MAX);
  prv_brightness(40);
  prv_vibe_stop();
  prv_vibe_start(VIBE_STRENGTH_MAX);
  prv_brightness(0);
  prv_vibe_stop();
  CHECK_TRACE(TRACE_BACKLIGHT_AND_VIBE);
}

void test_pwm_stm32__period_too_short(void) {
  const struct pbl_pwm pwm = PBL_PWM(s_tim12_ctrl, 0, 20U, 0);
  struct pbl_pwm_state state;

  prv_init_devices();
  prv_trace_start();
  pbl_pwm_init_state(&pwm, &state);
  state.duty_cycle = 10U;
  state.enabled = true;
  cl_assert_equal_i(pbl_pwm_apply(&pwm, &state), -EINVAL);
  CHECK_TRACE(TRACE_PERIOD_TOO_SHORT);
}
