/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! STM32F4 timer PWM, as the stock firmware did it.

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/pwm/stm32.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/stm32/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/units.h>

#include <hardware/stm32f4_tim.h>

#define MHZ_PER_HZ 1000U

static const struct pbl_pwm_stm32 *prv_pwm(const struct pbl_pwm_controller *ctrl) {
  return container_of(ctrl, const struct pbl_pwm_stm32, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_pwm_stm32 *pwm, uint32_t offset) {
  return pbl_readl(pwm->base + offset);
}

static inline void prv_putreg(const struct pbl_pwm_stm32 *pwm, uint32_t offset, uint32_t value) {
  pbl_writel(value, pwm->base + offset);
}

static int prv_timing(uint64_t clock, uint32_t period, uint32_t *prescaler, uint32_t *reload) {
  const uint64_t counts = (clock * period) / PBL_NSEC_PER_SEC;
  const uint64_t psc = (counts > 0U) ? (counts - 1U) / (STM32F4_TIM_ARR_MAX + 1U) : 0U;

  if (counts < 2U || psc > STM32F4_TIM_PSC_MAX) {
    return -EINVAL;
  }

  *prescaler = (uint32_t)psc;
  *reload = (uint32_t)(counts / (psc + 1U)) - 1U;

  return 0;
}

//! PWM mode 1 with preload
static void prv_oc_init(const struct pbl_pwm_stm32 *pwm, uint8_t channel, uint32_t pulse,
                        bool inversed) {
  const uint32_t ccmr = STM32F4_TIM_CCMR_OFFSET(channel);
  const uint32_t ccmr_shift = STM32F4_TIM_CCMR_CH_SHIFT(channel);
  const uint32_t ccer_shift = STM32F4_TIM_CCER_CH_SHIFT(channel);
  const uint32_t mode =
      PBL_FIELD_PREP(STM32F4_TIM_CCMR_OCM_MASK, STM32F4_TIM_CCMR_OCM_PWM1) | STM32F4_TIM_CCMR_OCPE;

  prv_putreg(pwm, STM32F4_TIM_CCER_OFFSET,
             prv_getreg(pwm, STM32F4_TIM_CCER_OFFSET) & ~(STM32F4_TIM_CCER_CCE << ccer_shift));
  prv_putreg(
      pwm, ccmr,
      (prv_getreg(pwm, ccmr) & ~(STM32F4_TIM_CCMR_CH_MASK << ccmr_shift)) | (mode << ccmr_shift));
  prv_putreg(pwm, STM32F4_TIM_CCR_OFFSET(channel), pulse);
  prv_putreg(pwm, STM32F4_TIM_CCER_OFFSET,
             (prv_getreg(pwm, STM32F4_TIM_CCER_OFFSET) &
              ~((STM32F4_TIM_CCER_CCP | STM32F4_TIM_CCER_CCNP) << ccer_shift)) |
                 ((STM32F4_TIM_CCER_CCE | (inversed ? STM32F4_TIM_CCER_CCP : 0U)) << ccer_shift));
}

static int prv_apply(const struct pbl_pwm_controller *ctrl, uint8_t channel,
                     const struct pbl_pwm_state *state) {
  const struct pbl_pwm_stm32 *pwm = prv_pwm(ctrl);
  struct pbl_pwm_stm32_state *st = pwm->state;
  const bool inversed = state->polarity == PBL_PWM_POLARITY_INVERSED;
  uint64_t rate_mhz;
  uint32_t prescaler;
  uint32_t reload;
  int ret;

  ret = pbl_clk_get_rate(&pwm->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  const uint64_t clock = rate_mhz / MHZ_PER_HZ;
  ret = prv_timing(clock, state->period, &prescaler, &reload);
  if (ret != 0) {
    return ret;
  }

  const uint8_t others = st->running & ~PBL_BIT(channel);

  if (!state->enabled) {
    if (others == 0U) {
      ret = pbl_pinctrl_select_state(&pwm->pinctrl, PBL_PINCTRL_STATE_SLEEP);
      if (ret != 0) {
        return ret;
      }
    }

    if (st->running != 0U && others == 0U) {
      prv_putreg(pwm, STM32F4_TIM_CR1_OFFSET,
                 prv_getreg(pwm, STM32F4_TIM_CR1_OFFSET) & ~STM32F4_TIM_CR1_CEN);
      (void)pbl_clk_disable(&pwm->clk);
      soc_stm32_stop_release();
    }
    st->running = others;

    return 0;
  }

  if (others != 0U && (prescaler != st->prescaler || reload != st->reload)) {
    return -EBUSY;
  }

  const uint32_t tick_hz = (uint32_t)(clock / (prescaler + 1U));
  const uint32_t pulse =
      MIN((uint32_t)(((uint64_t)state->duty_cycle * tick_hz) / PBL_NSEC_PER_SEC), reload + 1U);

  if (st->running == 0U) {
    soc_stm32_stop_block();
    ret = pbl_clk_enable(&pwm->clk);
    if (ret != 0) {
      soc_stm32_stop_release();
      return ret;
    }

    // Up counting, ARR preloaded; UG loads PSC and ARR now
    prv_putreg(pwm, STM32F4_TIM_CR1_OFFSET, STM32F4_TIM_CR1_ARPE);
    prv_putreg(pwm, STM32F4_TIM_ARR_OFFSET, reload);
    prv_putreg(pwm, STM32F4_TIM_PSC_OFFSET, prescaler);
    prv_putreg(pwm, STM32F4_TIM_EGR_OFFSET, STM32F4_TIM_EGR_UG);

    st->prescaler = prescaler;
    st->reload = reload;
  }

  prv_oc_init(pwm, channel, pulse, inversed);

  if (st->running == 0U) {
    (void)pbl_pinctrl_select_state(&pwm->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
    prv_putreg(pwm, STM32F4_TIM_CR1_OFFSET,
               prv_getreg(pwm, STM32F4_TIM_CR1_OFFSET) | STM32F4_TIM_CR1_CEN);
  }
  st->running = others | PBL_BIT(channel);

  return 0;
}

int pbl_pwm_stm32_init(const struct pbl_device *dev) {
  const struct pbl_pwm_stm32 *pwm = container_of(dev, const struct pbl_pwm_stm32, ctrl.dev);
  int ret;

  ret = pbl_device_init(&pwm->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_pinctrl_select_state(&pwm->pinctrl, PBL_PINCTRL_STATE_SLEEP);
}

const struct pbl_pwm_controller_ops pbl_pwm_stm32_ops = {
  .apply = prv_apply,
};

#include PBL_DT_INSTANCES(st_stm32_pwm)
