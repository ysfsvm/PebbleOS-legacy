/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pbl/bluetooth/hci_uart.h>
#include <pbl/bluetooth/le_crypto.h>
#include <pbl/drivers/bluetooth/cc2564.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>

#include <clar.h>
#include <host/ble_hs.h>
#include <kernel/pebble_tasks.h>
#include <nimble/transport.h>
#include <nimble/transport/hci_h4.h>
#include <os/os_mbuf.h>

// Kernel
////////////////////////////////////////////////////////////////////////////////

#define SLEEP_COST_MS 10U

static uint64_t s_now_ms;
static uint32_t s_sleeps;

uint64_t pbl_uptime_ms(void) {
  return s_now_ms;
}

uint64_t pbl_uptime_ticks(void) {
  return s_now_ms * PBL_TICK_HZ / 1000U;
}

static uint32_t s_cycles;

uint32_t arch_cycle_get_32(void) {
  return s_cycles++;
}

bool pbl_kernel_is_started(void) {
  return true;
}

uint32_t pbl_cycles_per_sec(void) {
  return 1000000U;
}

void pbl_sleep_ms(uint32_t ms) {
  s_sleeps++;
  s_now_ms += SLEEP_COST_MS;
}

void pbl_thread_sleep(pbl_timeout_t timeout) {
  s_sleeps++;
  s_now_ms += SLEEP_COST_MS;
}

uint32_t pbl_ms_to_ticks(uint32_t ms) {
  return ms;
}

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  return 0;
}

void pbl_mutex_unlock(struct pbl_mutex *m) {
}

int pbl_sem_take(struct pbl_sem *s, pbl_timeout_t timeout) {
  return 0;
}

void pbl_sem_give(struct pbl_sem *s) {
}

int pbl_thread_create(struct pbl_thread *t, const struct pbl_thread_attr *attr) {
  return 0;
}

void pebble_task_register(PebbleTask task, struct pbl_thread *thread) {
}

void pbl_task_wdt_feed_self(void) {
}

void pbl_task_wdt_set_waiting(bool waiting) {
}

void soc_stm32_stop_block(void) {
}

void soc_stm32_stop_release(void) {
}

typedef const struct UARTDevice UARTDevice;

extern UARTDevice *const BOARD_BT_HCI_UART_DEV;

// NimBLE port
////////////////////////////////////////////////////////////////////////////////

struct ble_npl_event *npl_pebble_eventq_get(struct ble_npl_eventq *evq, ble_npl_time_t tmo) {
  return nullptr;
}

void npl_pebble_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
}

void npl_pebble_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
}

ble_npl_error_t npl_pebble_mutex_init(struct ble_npl_mutex *mu) {
  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout) {
  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_mutex_release(struct ble_npl_mutex *mu) {
  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_sem_init(struct ble_npl_sem *sem, uint16_t tokens) {
  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout) {
  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_sem_release(struct ble_npl_sem *sem) {
  return BLE_NPL_OK;
}

void npl_pebble_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq,
                             ble_npl_event_fn *ev_cb, void *ev_arg) {
}

ble_npl_error_t npl_pebble_callout_reset(struct ble_npl_callout *co, ble_npl_time_t ticks) {
  return BLE_NPL_OK;
}

void npl_pebble_callout_stop(struct ble_npl_callout *co) {
}

bool npl_pebble_callout_is_active(struct ble_npl_callout *co) {
  return false;
}

ble_npl_time_t npl_pebble_callout_get_ticks(struct ble_npl_callout *co) {
  return 0;
}

ble_npl_time_t npl_pebble_callout_remaining_ticks(struct ble_npl_callout *co, ble_npl_time_t now) {
  return 0;
}

ble_npl_error_t npl_pebble_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks) {
  *out_ticks = ms;
  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms) {
  *out_ms = ticks;
  return BLE_NPL_OK;
}

// NimBLE
////////////////////////////////////////////////////////////////////////////////

static uint32_t s_host_resets;
static int s_host_reset_reason;

void ble_hs_sched_reset(int reason) {
  s_host_resets++;
  s_host_reset_reason = reason;
}

void hci_h4_sm_init(struct hci_h4_sm *h4sm, const struct hci_h4_allocators *allocs,
                    hci_h4_frame_cb *frame_cb) {
}

int hci_h4_sm_rx(struct hci_h4_sm *h4sm, const uint8_t *buf, uint16_t len) {
  return len;
}

int os_mbuf_free_chain(struct os_mbuf *om) {
  return 0;
}

int ble_transport_to_hs_evt_impl(void *buf) {
  return 0;
}

int ble_transport_to_hs_acl_impl(struct os_mbuf *om) {
  return 0;
}

int ble_transport_to_hs_iso_impl(struct os_mbuf *om) {
  return 0;
}

void *ble_transport_alloc_evt(int discardable) {
  return nullptr;
}

struct os_mbuf *ble_transport_alloc_acl_from_ll(void) {
  return nullptr;
}

void ble_transport_free(void *buf) {
}

os_error_t os_memblock_put_from_cb(struct os_mempool *mp, void *block_addr) {
  return OS_OK;
}

int ble_store_iterate(int obj_type, ble_store_iterator_fn *callback, void *cookie) {
  return 0;
}

bool pbl_bt_le_irk_resolves(const uint8_t irk[16], const uint8_t rpa[6]) {
  return false;
}

int ble_transport_register_put_acl_from_ll_cb(os_mempool_put_fn *cb) {
  return 0;
}

#include "../../../third_party/nimble/transport/hci_uart_legacy.c"

// UART and controller
////////////////////////////////////////////////////////////////////////////////

UARTDevice *const BOARD_BT_HCI_UART_DEV = nullptr;

static bool s_tx_ready;
static uint32_t s_bytes_written;
static uint32_t s_boots;
static uint32_t s_stalls_reported;

bool uart_is_tx_ready(UARTDevice *dev) {
  return s_tx_ready;
}

void uart_write_byte(UARTDevice *dev, uint8_t data) {
  s_bytes_written++;
}

void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler handler) {
}

void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled) {
}

bool cc2564_boot(void) {
  s_boots++;
  return true;
}

void cc2564_report_stall(bool rx_paused, bool evt_waiting) {
  s_stalls_reported++;
}

void cc2564_shutdown(void) {
}

bool cc2564_take_sleep_ind(void) {
  return false;
}

void cc2564_hold_rts(bool hold) {
}

void cc2564_cts_wake_enable(pbl_intc_handler_t cb) {
}

void cc2564_cts_wake_disable(void) {
}

void cc2564_send_hcill(uint8_t msg) {
}

// Tests
////////////////////////////////////////////////////////////////////////////////

static uint8_t s_payload[4] = {1, 2, 3, 4};
static struct os_mbuf s_om;

static void prv_send_acl(void) {
  s_om = (struct os_mbuf){.om_data = s_payload, .om_len = sizeof(s_payload)};
  (void)ble_transport_to_ll_acl_impl(&s_om);
}

void test_hci_uart_legacy__initialize(void) {
  s_now_ms = 0U;
  s_sleeps = 0U;
  s_tx_ready = true;
  s_bytes_written = 0U;
  s_boots = 0U;
  s_stalls_reported = 0U;
  s_host_resets = 0U;
  s_host_reset_reason = 0;
  s_reboots = 0U;
  s_tx_stall = false;
  s_reboot_requested = false;
  s_hcill = HcillAwake;
  s_rx_head = 0U;
  s_rx_tail = 0U;
#ifdef CONFIG_BT_CC2564_DIAG
  s_rx_bytes = 0U;
  s_rx_overruns = 0U;
  s_rx_framing_errors = 0U;
  s_rx_noise_errors = 0U;
#endif
  prv_uart_start();
}

void test_hci_uart_legacy__sends_while_cts_is_on(void) {
  prv_send_acl();

  cl_assert_equal_i(s_bytes_written, 1 + sizeof(s_payload));
  cl_assert_equal_i(s_stalls_reported, 0);
}

void test_hci_uart_legacy__stall_gives_up_after_two_seconds(void) {
  s_tx_ready = false;

  prv_send_acl();

  cl_assert_equal_i(s_stalls_reported, 1);
  cl_assert(s_now_ms >= TX_STALL_TIMEOUT_MS);
  cl_assert(s_now_ms <= TX_STALL_TIMEOUT_MS + 2U * SLEEP_COST_MS);
  cl_assert(s_reboot_requested);
}

void test_hci_uart_legacy__stall_power_cycles_and_resets_host(void) {
  s_tx_ready = false;
  prv_send_acl();
  s_tx_ready = true;

  prv_controller_reboot();

  cl_assert_equal_i(s_boots, 1);
  cl_assert_equal_i(s_controller_state, ControllerStateReady);
  cl_assert_equal_i(s_host_resets, 1);
  cl_assert_equal_i(s_host_reset_reason, BLE_HS_ECONTROLLER);
}

void test_hci_uart_legacy__stall_after_last_power_cycle_resets_host(void) {
  s_reboots = MAX_CONTROLLER_REBOOTS;
  s_tx_ready = false;
  prv_send_acl();

  prv_controller_reboot();

  cl_assert_equal_i(s_boots, 0);
  cl_assert_equal_i(s_controller_state, ControllerStateFailed);
  cl_assert_equal_i(s_host_resets, 1);
}

void test_hci_uart_legacy__host_timeout_power_cycle_leaves_host_alone(void) {
  pbl_bt_hci_uart_controller_unresponsive();

  prv_controller_reboot();

  cl_assert_equal_i(s_boots, 1);
  cl_assert_equal_i(s_host_resets, 0);
}

void test_hci_uart_legacy__rx_bytes_are_stored_whatever_the_error_flags(void) {
  const UARTRXErrorFlags clean = {0};
  const UARTRXErrorFlags errors = {
    .overrun_error = true,
    .framing_error = true,
    .noise_detected = true,
  };

  prv_rx_irq_handler(BOARD_BT_HCI_UART_DEV, 0x04, &clean);
  prv_rx_irq_handler(BOARD_BT_HCI_UART_DEV, 0x0e, &errors);
  prv_rx_irq_handler(BOARD_BT_HCI_UART_DEV, 0x30, &clean);

  cl_assert_equal_i(s_rx_head, 3);
  cl_assert_equal_i(s_rx_tail, 0);
  cl_assert_equal_i(s_rx_ring[0], 0x04);
  cl_assert_equal_i(s_rx_ring[1], 0x0e);
  cl_assert_equal_i(s_rx_ring[2], 0x30);
  cl_assert(!s_rx_paused);
#ifdef CONFIG_BT_CC2564_DIAG
  char diag[PBL_BT_HCI_UART_DIAG_LEN];
  pbl_bt_hci_uart_copy_diag(diag, sizeof(diag));
  cl_assert_equal_s(diag, "rx3 o1 f1 n1 ev0 cp0 A z0 c0 h0 r0 f0 i0/0 s0/0");
#endif
}
