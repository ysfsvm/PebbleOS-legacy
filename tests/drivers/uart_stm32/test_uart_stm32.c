/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/kernel/irq.h>

#include <clar.h>

// The board's bus clocks: PCLK1 24 MHz, PCLK2 48 MHz
#define CLK_PCLK1 0U
#define CLK_PCLK2 1U

PBL_DEVICE_CLASS_DEFINE(pbl_clk_controller_class, "clk");
PBL_CLK_CONTROLLER_STATE_DEFINE(s_rcc, 2);
static const struct pbl_clk_controller s_rcc =
    PBL_CLK_CONTROLLER_INIT(s_rcc, "rcc", nullptr, nullptr, nullptr);

static int s_clk_users;

int pbl_clk_enable(const struct pbl_clk *clk) {
  s_clk_users++;
  return 0;
}

int pbl_clk_get_rate(const struct pbl_clk *clk, uint64_t *rate_mhz) {
  *rate_mhz = (clk->id == CLK_PCLK2 ? 48000000ULL : 24000000ULL) * 1000ULL;
  return 0;
}

// Pin states: which one each port selected
static const struct pbl_pinctrl_state s_usart3_default;
static const struct pbl_pinctrl_state s_usart1_default;
static const struct pbl_pinctrl_state *s_selected[4];
static int s_num_selected;

int pbl_pinctrl_select_state(const struct pbl_pinctrl *pinctrl, enum pbl_pinctrl_state_id id) {
  const struct pbl_pinctrl_state *state = pinctrl->states[id];

  if (state == nullptr) {
    return -ENOENT;
  }
  cl_assert(s_num_selected < 4);
  s_selected[s_num_selected++] = state;
  return 0;
}

static bool s_irq_enabled[64];

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

void pbl_irq_enable(pbl_irq_t irq) {
  s_irq_enabled[irq] = true;
}

#include "../../../drivers/uart/stm32.c"

// Registers
////////////////////////////////////////////////////////////////////////////////
// Reading DR takes the received byte and, after SR was read, clears the error flags. Writing DR
// takes TXE and TC until the test gives them back: TXE after s_txe_after SR reads, TC after
// s_tc_after. Line settings written while the USART is enabled are noted in s_live_write, the
// USART disabled before a byte written here has left in s_cut_byte.

#define USART3_BASE 0x40004800U
#define USART1_BASE 0x40011000U
#define USART2_BASE 0x40004400U
#define NUM_REGS    32

#define SR_ERRORS \
  (STM32F4_USART_SR_PE | STM32F4_USART_SR_FE | STM32F4_USART_SR_NF | STM32F4_USART_SR_ORE)

static struct {
  uintptr_t addr;
  uint32_t val;
} s_regs[NUM_REGS];
static int s_num_regs;

static uint8_t s_rx_data;
static int s_dr_reads;
static uint8_t s_tx[16];
static int s_num_tx;
static int s_sr_reads;
static int s_txe_after;
static int s_tc_after;
static bool s_live_write;
static bool s_tx_in_flight;
static bool s_cut_byte;
static int s_sr_reads_at_tx;

static uint32_t *prv_fake_reg(uintptr_t addr) {
  for (int i = 0; i < s_num_regs; i++) {
    if (s_regs[i].addr == addr) {
      return &s_regs[i].val;
    }
  }
  cl_assert(s_num_regs < NUM_REGS);
  s_regs[s_num_regs].addr = addr;
  s_regs[s_num_regs].val = 0U;
  return &s_regs[s_num_regs++].val;
}

static uintptr_t prv_base(uintptr_t addr) {
  return addr & ~(uintptr_t)0x3ffU;
}

uint32_t fake_mmio_readl(uintptr_t addr) {
  uint32_t *sr = prv_fake_reg(prv_base(addr) + STM32F4_USART_SR_OFFSET);

  if (addr == prv_base(addr) + STM32F4_USART_SR_OFFSET) {
    // A wait for a flag that never comes
    cl_assert(++s_sr_reads < 1000);
    if (s_txe_after > 0 && --s_txe_after == 0) {
      *sr |= STM32F4_USART_SR_TXE;
    }
    if (s_tc_after > 0 && --s_tc_after == 0) {
      *sr |= STM32F4_USART_SR_TC;
      s_tx_in_flight = false;
    }
  } else if (addr == prv_base(addr) + STM32F4_USART_DR_OFFSET) {
    s_dr_reads++;
    *sr &= ~(STM32F4_USART_SR_RXNE | SR_ERRORS);
    return s_rx_data;
  }
  return *prv_fake_reg(addr);
}

void fake_mmio_writel(uint32_t val, uintptr_t addr) {
  if (addr == prv_base(addr) + STM32F4_USART_DR_OFFSET) {
    cl_assert(s_num_tx < (int)sizeof(s_tx));
    s_tx[s_num_tx++] = (uint8_t)val;
    s_sr_reads_at_tx = s_sr_reads;
    s_tx_in_flight = true;
    *prv_fake_reg(prv_base(addr) + STM32F4_USART_SR_OFFSET) &=
        ~(STM32F4_USART_SR_TXE | STM32F4_USART_SR_TC);
    return;
  }

  uint32_t cr1 = *prv_fake_reg(prv_base(addr) + STM32F4_USART_CR1_OFFSET);
  uint32_t changed = *prv_fake_reg(addr) ^ val;

  if (addr == prv_base(addr) + STM32F4_USART_CR1_OFFSET) {
    if ((cr1 & ~val & STM32F4_USART_CR1_UE) != 0U && s_tx_in_flight) {
      s_cut_byte = true;
    }
    cr1 &= val;
    changed &= CR1_FRAME | STM32F4_USART_CR1_OVER8;
  } else if (addr == prv_base(addr) + STM32F4_USART_CR3_OFFSET) {
    changed &= CR3_FLOW;
  }
  if ((cr1 & STM32F4_USART_CR1_UE) != 0U && changed != 0U) {
    s_live_write = true;
  }
  *prv_fake_reg(addr) = val;
}

static uint32_t prv_usart(uintptr_t base, uint32_t offset) {
  return *prv_fake_reg(base + offset);
}

//! A byte arrives with @p errors, then the port's interrupt is taken if it is enabled.
static void prv_receive(const struct pbl_uart_stm32 *usart, uint8_t c, uint32_t errors) {
  s_rx_data = c;
  *prv_fake_reg(usart->base + STM32F4_USART_SR_OFFSET) |= STM32F4_USART_SR_RXNE | errors;
  pbl_uart_stm32_irq_handler(usart);
}

// Callback
////////////////////////////////////////////////////////////////////////////////

static uint8_t s_rx[16];
static int s_num_rx;
static bool s_disable_in_cb;

static void prv_rx(const struct pbl_uart *uart, uint8_t c, void *arg) {
  cl_assert(s_num_rx < (int)sizeof(s_rx));
  s_rx[s_num_rx++] = c;
  if (s_disable_in_cb) {
    cl_assert_equal_i(pbl_uart_rx_disable(uart), 0);
  }
}

// Tests
////////////////////////////////////////////////////////////////////////////////

void test_uart_stm32__initialize(void) {
  memset(s_regs, 0, sizeof(s_regs));
  s_num_regs = 0;
  memset(s_irq_enabled, 0, sizeof(s_irq_enabled));
  s_clk_users = 0;
  s_num_selected = 0;
  s_rx_data = 0U;
  s_dr_reads = 0;
  s_num_tx = 0;
  s_sr_reads = 0;
  s_txe_after = 0;
  s_tc_after = 0;
  s_live_write = false;
  s_tx_in_flight = false;
  s_cut_byte = false;
  s_sr_reads_at_tx = 0;
  s_num_rx = 0;
  s_disable_in_cb = false;

  // From reset, TXE and TC are set
  for (uintptr_t base = USART2_BASE; base != 0U;
       base = base == USART2_BASE ? USART3_BASE : (base == USART3_BASE ? USART1_BASE : 0U)) {
    *prv_fake_reg(base + STM32F4_USART_SR_OFFSET) = STM32F4_USART_SR_TXE | STM32F4_USART_SR_TC;
  }

  memset(&dt_usart3_stm32_state, 0, sizeof(dt_usart3_stm32_state));
  memset(&dt_usart1_stm32_state, 0, sizeof(dt_usart1_stm32_state));
  memset(&dt_usart3_uart_state, 0, sizeof(dt_usart3_uart_state));
  memset(&dt_usart1_uart_state, 0, sizeof(dt_usart1_uart_state));
  memset(&dt_usart2_uart_state, 0, sizeof(dt_usart2_uart_state));
  memset(&dt_usart3_device_state, 0, sizeof(dt_usart3_device_state));
  memset(&dt_usart1_device_state, 0, sizeof(dt_usart1_device_state));
  memset(&dt_usart2_device_state, 0, sizeof(dt_usart2_device_state));
}

void test_uart_stm32__cleanup(void) {
  cl_assert(!s_live_write);
  cl_assert(!s_cut_byte);
}

void test_uart_stm32__init_sets_up_8n1_at_the_board_rate(void) {
  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);

  cl_assert_equal_i(s_clk_users, 1);
  cl_assert_equal_i(s_num_selected, 1);
  cl_assert_equal_p(s_selected[0], &s_usart3_default);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET),
                    STM32F4_USART_CR1_UE | STM32F4_USART_CR1_TE | STM32F4_USART_CR1_RE);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR2_OFFSET), 0U);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR3_OFFSET), 0U);
  // 24 MHz / 115200 = 208.3: mantissa 13, fraction 0, as the ST library computed it
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_BRR_OFFSET), 0xd0U);
  cl_assert(s_irq_enabled[39]);
}

void test_uart_stm32__baud_divisors(void) {
  struct pbl_uart_config cfg = {.baudrate = 230400};

  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_usart1.dev), 0);

  // 24 MHz / 230400 = 104.2
  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_BRR_OFFSET), 0x68U);

  // 24 MHz / 57600 = 416.7, rounded up
  cfg.baudrate = 57600;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_BRR_OFFSET), 0x1a1U);

  // 48 MHz / 921600 = 52.1
  cfg.baudrate = 921600;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart1, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART1_BASE, STM32F4_USART_BRR_OFFSET), 0x34U);
  cl_assert_equal_i(prv_usart(USART1_BASE, STM32F4_USART_CR1_OFFSET) & STM32F4_USART_CR1_OVER8, 0U);

  // 24 MHz / 2 Mbaud = 12: 8x oversampling, USARTDIV 1.5
  cfg.baudrate = 2000000;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_BRR_OFFSET), 0x14U);
  cl_assert(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET) & STM32F4_USART_CR1_OVER8);

  // Past what 8x oversampling reaches, and below what the mantissa holds
  cfg.baudrate = 4000000;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), -ENOTSUP);
  cfg.baudrate = 300;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), -ENOTSUP);

  // The port keeps working, enabled
  cl_assert(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET) & STM32F4_USART_CR1_UE);
}

void test_uart_stm32__parity_and_flow_control(void) {
  struct pbl_uart_config cfg = {.baudrate = 115200, .parity = PBL_UART_PARITY_EVEN};

  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);
  cl_assert_equal_i(pbl_device_init(&dt_usart1.dev), 0);

  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET) & CR1_FRAME,
                    STM32F4_USART_CR1_M | STM32F4_USART_CR1_PCE);

  cfg.parity = PBL_UART_PARITY_ODD;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET) & CR1_FRAME, CR1_FRAME);

  // Only a port with RTS and CTS wired takes flow control
  cfg.parity = PBL_UART_PARITY_NONE;
  cfg.flow_ctrl = PBL_UART_FLOW_CTRL_RTS_CTS;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), -ENOTSUP);
  cl_assert_equal_i(pbl_uart_configure(&dt_usart1, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART1_BASE, STM32F4_USART_CR3_OFFSET), CR3_FLOW);
  cl_assert_equal_i(prv_usart(USART1_BASE, STM32F4_USART_CR1_OFFSET) & CR1_FRAME, 0U);

  cfg.flow_ctrl = PBL_UART_FLOW_CTRL_NONE;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart1, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART1_BASE, STM32F4_USART_CR3_OFFSET), 0U);
}

void test_uart_stm32__poll_out_waits_for_txe(void) {
  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);

  pbl_uart_poll_out(&dt_usart3, 'A');
  cl_assert_equal_i(s_num_tx, 1);

  // The byte is still in the transmit register: the next one waits for room, not for the line
  s_sr_reads = 0;
  s_txe_after = 5;
  s_tc_after = 0;
  pbl_uart_poll_out(&dt_usart3, 'B');
  cl_assert_equal_i(s_num_tx, 2);
  cl_assert_equal_i(s_sr_reads_at_tx, 5);
  cl_assert_equal_i(s_tx[0], 'A');
  cl_assert_equal_i(s_tx[1], 'B');

  // flush waits for TC, which comes after TXE
  s_sr_reads = 0;
  s_txe_after = 1;
  s_tc_after = 3;
  pbl_uart_flush(&dt_usart3);
  cl_assert_equal_i(s_sr_reads, 3);
}

void test_uart_stm32__configure_waits_for_the_last_byte(void) {
  const struct pbl_uart_config cfg = {.baudrate = 230400};

  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);

  pbl_uart_poll_out(&dt_usart3, 'A');
  s_sr_reads = 0;
  s_txe_after = 1;
  s_tc_after = 4;
  cl_assert_equal_i(pbl_uart_configure(&dt_usart3, &cfg), 0);
  cl_assert_equal_i(s_sr_reads, 4);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_BRR_OFFSET), 0x68U);
}

void test_uart_stm32__rx_delivers_bytes_and_counts_errors(void) {
  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);

  // Disabled: a byte waits in DR
  s_dr_reads = 0;
  prv_receive(&dt_usart3_inst, 'x', 0U);
  cl_assert_equal_i(s_dr_reads, 0);

  cl_assert_equal_i(pbl_uart_rx_enable(&dt_usart3, prv_rx, nullptr), 0);
  cl_assert(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET) & STM32F4_USART_CR1_RXNEIE);
  pbl_uart_stm32_irq_handler(&dt_usart3_inst);
  cl_assert_equal_i(s_num_rx, 1);
  cl_assert_equal_i(s_rx[0], 'x');

  prv_receive(&dt_usart3_inst, 'y', STM32F4_USART_SR_ORE);
  prv_receive(&dt_usart3_inst, 'z', STM32F4_USART_SR_FE);
  cl_assert_equal_i(s_num_rx, 3);
  cl_assert_equal_i(s_rx[1], 'y');
  cl_assert_equal_i(s_rx[2], 'z');
  cl_assert_equal_i(dt_usart3_stm32_state.overruns, 1U);
  cl_assert_equal_i(dt_usart3_stm32_state.framing_errors, 1U);
  // Reading DR cleared the flags
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_SR_OFFSET) & SR_ERRORS, 0U);

  // An overrun with no byte waiting is cleared and counted, and delivers nothing
  s_dr_reads = 0;
  *prv_fake_reg(USART3_BASE + STM32F4_USART_SR_OFFSET) |= STM32F4_USART_SR_ORE;
  pbl_uart_stm32_irq_handler(&dt_usart3_inst);
  cl_assert_equal_i(s_dr_reads, 1);
  cl_assert_equal_i(s_num_rx, 3);
  cl_assert_equal_i(dt_usart3_stm32_state.overruns, 2U);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_SR_OFFSET) & SR_ERRORS, 0U);

  // Nothing pending: DR is left alone
  pbl_uart_stm32_irq_handler(&dt_usart3_inst);
  cl_assert_equal_i(s_dr_reads, 1);
}

void test_uart_stm32__rx_disable_holds_bytes_back(void) {
  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);
  cl_assert_equal_i(pbl_uart_rx_enable(&dt_usart3, prv_rx, nullptr), 0);
  s_dr_reads = 0;

  // The callback stops reception: the next byte stays in the USART, which keeps receiving
  s_disable_in_cb = true;
  prv_receive(&dt_usart3_inst, '1', 0U);
  cl_assert_equal_i(s_num_rx, 1);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET) & STM32F4_USART_CR1_RXNEIE,
                    0U);
  cl_assert(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET) & STM32F4_USART_CR1_RE);

  prv_receive(&dt_usart3_inst, '2', 0U);
  cl_assert_equal_i(s_num_rx, 1);
  cl_assert_equal_i(s_dr_reads, 1);

  // Enabled again, the held byte comes
  s_disable_in_cb = false;
  cl_assert_equal_i(pbl_uart_rx_enable(&dt_usart3, prv_rx, nullptr), 0);
  pbl_uart_stm32_irq_handler(&dt_usart3_inst);
  cl_assert_equal_i(s_num_rx, 2);
  cl_assert_equal_i(s_rx[1], '2');
}

void test_uart_stm32__init_does_not_wait_for_a_byte_it_did_not_send(void) {
  const struct pbl_uart_config cfg = {.baudrate = 115200};

  // Left enabled by an earlier stage with TC clear, which nothing here will set
  *prv_fake_reg(USART3_BASE + STM32F4_USART_CR1_OFFSET) =
      STM32F4_USART_CR1_UE | STM32F4_USART_CR1_TE;
  *prv_fake_reg(USART3_BASE + STM32F4_USART_SR_OFFSET) = 0U;

  cl_assert_equal_i(pbl_uart_early_init(&dt_usart3, &cfg), 0);
  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET),
                    STM32F4_USART_CR1_UE | STM32F4_USART_CR1_TE | STM32F4_USART_CR1_RE);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_BRR_OFFSET), 0xd0U);
}

void test_uart_stm32__a_port_without_pins(void) {
  cl_assert_equal_i(pbl_device_init(&dt_usart2.dev), 0);

  cl_assert_equal_i(s_num_selected, 0);
  cl_assert_equal_i(prv_usart(USART2_BASE, STM32F4_USART_BRR_OFFSET), 0x68U);
  cl_assert(s_irq_enabled[38]);
}

void test_uart_stm32__early_console_then_device_init(void) {
  const struct pbl_uart_config cfg = {.baudrate = 230400};

  cl_assert_equal_i(pbl_uart_early_init(&dt_usart3, &cfg), 0);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET),
                    STM32F4_USART_CR1_UE | STM32F4_USART_CR1_TE);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_BRR_OFFSET), 0x68U);
  cl_assert(!s_irq_enabled[39]);

  pbl_uart_poll_out(&dt_usart3, 'e');
  cl_assert_equal_i(s_num_tx, 1);

  // The device init waits for the byte to leave and keeps the early line settings
  s_sr_reads = 0;
  s_txe_after = 1;
  s_tc_after = 2;
  cl_assert_equal_i(pbl_device_init(&dt_usart3.dev), 0);
  cl_assert(s_sr_reads >= 2);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_CR1_OFFSET),
                    STM32F4_USART_CR1_UE | STM32F4_USART_CR1_TE | STM32F4_USART_CR1_RE);
  cl_assert_equal_i(prv_usart(USART3_BASE, STM32F4_USART_BRR_OFFSET), 0x68U);
  cl_assert(s_irq_enabled[39]);
}
