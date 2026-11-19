/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../../../drivers/imu/mag3110/mag3110.c"

#include <errno.h>
#include <string.h>

#include <clar.h>

// The rate changes of the MAG3110 driver: its data ready line is enabled exactly while it samples,
// also when a bus transfer fails on the way.

static struct pbl_mag3110_state s_state;
static const struct pbl_mag3110 s_mag = {.state = &s_state};

static bool s_line_enabled;
static int s_fail_ctrl_reg1_active;
static int s_fail_standby;

// The bus
////////////////////////////////////////////////////////////////////////////////

int pbl_i2c_transfer(const struct pbl_i2c_dev *dev, struct pbl_i2c_msg *msgs, uint8_t num_msgs) {
  const bool write = num_msgs == 1U;

  if (write && msgs[0].buf[0] == CTRL_REG1) {
    if ((msgs[0].buf[1] & CTRL_REG1_ACTIVE) != 0U && s_fail_ctrl_reg1_active > 0) {
      s_fail_ctrl_reg1_active--;
      return -EIO;
    }
    if (msgs[0].buf[1] == 0U && s_fail_standby > 0) {
      s_fail_standby--;
      return -EIO;
    }
  }
  if (!write) {
    // SYSMOD 0: in standby at once; sample registers read as zeros
    memset(msgs[1].buf, 0, msgs[1].len);
  }
  return 0;
}

// The rest of the system
////////////////////////////////////////////////////////////////////////////////

int pbl_intc_enable(const struct pbl_irq_spec *spec) {
  s_line_enabled = true;
  return 0;
}

int pbl_intc_disable(const struct pbl_irq_spec *spec) {
  s_line_enabled = false;
  return 0;
}

int pbl_intc_request(const struct pbl_irq_spec *spec, pbl_intc_handler_t handler, void *arg) {
  return 0;
}

int pbl_device_init(const struct pbl_device *dev) {
  return 0;
}

int pbl_gpio_configure(const struct pbl_gpio *gpio, uint32_t flags) {
  return 0;
}

int pbl_gpio_get_raw(const struct pbl_gpio *gpio) {
  return 0;
}

int pbl_gpio_to_irq(const struct pbl_gpio *gpio, uint32_t type, struct pbl_irq_spec *spec) {
  return 0;
}

void pbl_sleep_ms(uint32_t ms) {
}

bool pbl_work_submit(struct pbl_workq *q, struct pbl_work *work) {
  return false;
}

struct pbl_workq pbl_sys_workq;

void pbl_mag_lock(const struct pbl_mag *mag) {
}

void pbl_mag_unlock(const struct pbl_mag *mag) {
}

void pbl_mag_report_sample(const struct pbl_mag *mag, const MagData *sample) {
}

// Tests
////////////////////////////////////////////////////////////////////////////////

static int prv_set_interval(uint32_t interval_us) {
  uint32_t actual_us;

  return prv_set_sampling_interval(&s_mag.mag, interval_us, &actual_us);
}

void test_mag3110__initialize(void) {
  memset(&s_state, 0, sizeof(s_state));
  s_dev = &s_mag;
  s_line_enabled = false;
  s_fail_ctrl_reg1_active = 0;
  s_fail_standby = 0;
}

void test_mag3110__start_and_stop(void) {
  cl_assert_equal_i(prv_set_interval(50000U), 0);
  cl_assert(s_line_enabled);
  cl_assert_equal_i(s_state.rate_hz, 20);

  cl_assert_equal_i(prv_set_interval(0U), 0);
  cl_assert(!s_line_enabled);
  cl_assert_equal_i(s_state.rate_hz, 0);
}

//! The line was enabled, then the sensor would not start: it must not stay enabled.
void test_mag3110__a_failed_start_leaves_the_line_off(void) {
  s_fail_ctrl_reg1_active = 1;
  cl_assert_equal_i(prv_set_interval(50000U), -EIO);
  cl_assert(!s_line_enabled);
  cl_assert_equal_i(s_state.rate_hz, 0);

  // And the next start works
  cl_assert_equal_i(prv_set_interval(50000U), 0);
  cl_assert(s_line_enabled);
  cl_assert_equal_i(s_state.rate_hz, 20);
}

//! A rate change stops the sensor first: if it does not start again, it is stopped.
void test_mag3110__a_failed_rate_change_stops(void) {
  cl_assert_equal_i(prv_set_interval(200000U), 0);
  cl_assert_equal_i(s_state.rate_hz, 5);

  s_fail_ctrl_reg1_active = 1;
  cl_assert_equal_i(prv_set_interval(50000U), -EIO);
  cl_assert(!s_line_enabled);
  cl_assert_equal_i(s_state.rate_hz, 0);
}

//! A stop whose standby write fails still takes the line away.
void test_mag3110__a_failed_stop_leaves_the_line_off(void) {
  cl_assert_equal_i(prv_set_interval(50000U), 0);

  s_fail_standby = 1;
  cl_assert_equal_i(prv_set_interval(0U), -EIO);
  cl_assert(!s_line_enabled);
  cl_assert_equal_i(s_state.rate_hz, 0);

  // A stop request now sees it stopped; a start goes through standby again
  cl_assert_equal_i(prv_set_interval(50000U), 0);
  cl_assert(s_line_enabled);
}
