/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

//! H4 transport of the legacy watches (CONFIG_BT_HCI_UART_LEGACY), grown from Core's hci_uart.c:
//! controller boot and vendor set-up, the controller's low-power UART protocol, the pinned
//! resolvable private address, and recovery of a wedged controller.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/bluetooth/hci_uart.h>
#include <pbl/board.h>
#include <pbl/drivers/uart.h>
#include <pbl/drivers/uart/stm32_legacy.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/task_wdt/task_wdt.h>
#include <pbl/util/math.h>

#ifdef CONFIG_BT_CONTROLLER_CC2564
#include <pbl/bluetooth/le_crypto.h>
#include <pbl/drivers/bluetooth/cc2564.h>
#include <pbl/drivers/rtc.h>
#include <pbl/soc/stm32/sleep.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>
#ifdef CONFIG_SERVICE_SHARED_PRF_STORAGE_IMPORT_STOCK_V2
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>
#endif
#endif
#include <kernel/pebble_tasks.h>

// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/hci_common.h>
#include <nimble/transport.h>
#include <nimble/transport/hci_h4.h>
#include <nimble/transport_impl.h>
#include <os/endian.h>

// Controller reboots, stalls and forced sends belong in the flash log; NimBLE's own logging is
// too noisy.
PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define RX_RING_SIZE     1024U
#define EVT_POOL_POLL_MS 5U

static struct pbl_thread s_hci_thread;
PBL_THREAD_STACK_DEFINE(s_hci_task_stack, CONFIG_BT_HCI_THREAD_STACK_SIZE);
static PBL_SEM_DEFINE(s_rx_ready, 0, 1);
static PBL_SEM_DEFINE(s_acl_pool_avail, 0, 1);
static PBL_MUTEX_DEFINE(s_tx_mutex);
static struct hci_h4_sm s_hci_h4sm;

// Filled by the UART ISR, drained by the HCI task.
static uint8_t s_rx_ring[RX_RING_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;
static volatile bool s_rx_paused;

// Set once the controller can take HCI traffic (or never will).
static volatile enum {
  ControllerStatePending,
  ControllerStateReady,
  ControllerStateFailed,
} s_controller_state;

// The last command sent and the last command answered, for diagnostics.
static volatile uint16_t s_last_cmd;
static volatile uint16_t s_last_reply_cmd;
static volatile uint8_t s_last_reply_status;
static volatile uint32_t s_num_evts;
static volatile uint16_t s_last_fail_cmd;
static volatile uint8_t s_last_fail_status;
// ACL packets sent and completed as the controller reports them, and the
// length of the last command reply, to tell which check a host reset tripped.
static volatile uint32_t s_acl_tx;
static volatile uint32_t s_acl_completed;
static volatile uint8_t s_last_reply_len;
static char s_reset_snapshot[32];

static inline void prv_trace_append(const char *entry) {
}
static inline void prv_note_completed(uint16_t count) {
}
static inline void prv_note_smp(char dir, struct os_mbuf *om) {
}

static struct os_mbuf *prv_alloc_acl_from_ll(void) {
  struct os_mbuf *om;

  // A NULL return is a fatal framing error for the H4 state machine, so wait
  // for the host to free a buffer. The UART backpressures meanwhile.
  while ((om = ble_transport_alloc_acl_from_ll()) == NULL) {
    (void)pbl_sem_take(&s_acl_pool_avail, PBL_MSEC(100));
  }

  return om;
}

static os_error_t prv_acl_put_signal(struct os_mempool_ext *mpe, void *data, void *arg) {
  os_error_t err = os_memblock_put_from_cb(&mpe->mpe_mp, data);
  pbl_sem_give(&s_acl_pool_avail);
  return err;
}

//! The HCI task is waiting for the host to free an event buffer.
static volatile bool s_evt_waiting;

static void *prv_alloc_evt(int discardable) {
  void *buf;

  // The H4 state machine drops a discardable event without a buffer, but takes
  // NULL for any other as a fatal framing error: wait for the host to free one.
  while ((buf = ble_transport_alloc_evt(discardable)) == NULL && !discardable) {
    s_evt_waiting = true;
    pbl_thread_sleep(PBL_MSEC(EVT_POOL_POLL_MS));
  }
  s_evt_waiting = false;

  return buf;
}

static const struct hci_h4_allocators s_hci_h4_allocs_from_ll = {
  .acl = prv_alloc_acl_from_ll,
  .evt = prv_alloc_evt,
};

// Pinned resolvable private address wanted on the air in place of the identity address.
static uint8_t s_local_rpa[6];
static volatile bool s_local_rpa_set;
// Random address last sent, and the one the controller accepted (cleared by HCI Reset).
static uint8_t s_rnd_sent[6];
static uint8_t s_rnd_in_use[6];
static volatile bool s_rnd_in_use_valid;

#define HCI_OP_LE_SET_RANDOM_ADDR BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_SET_RAND_ADDR)
#define HCI_OP_RESET              BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND, BLE_HCI_OCF_CB_RESET)
#define LE_CONN_COMPLETE_LEN      19U
#define LE_ENH_CONN_COMPLETE_LEN  31U
// Offset of the connection interval: the enhanced event inserts the local and peer RPAs here.
#define LE_CONN_COMPLETE_ITVL_OFF 12U

static bool prv_addr_is_rpa(const uint8_t *addr) {
  return (addr[5] & 0xc0U) == 0x40U;
}

//! When the controller uses a resolvable private address set as its random address, it reports
//! no local RPA. Report that address as our local RPA, so NimBLE pairs with the address on the
//! air while keeping the identity address. The legacy LE Connection Complete becomes the enhanced
//! one; the enhanced one, sent when the controller resolved the peer, gets the field filled in.
static void prv_conn_complete_add_local_rpa(struct ble_hci_ev *ev) {
  if (!s_rnd_in_use_valid || !prv_addr_is_rpa(s_rnd_in_use) ||
      ev->opcode != BLE_HCI_EVCODE_LE_META) {
    return;
  }
  uint8_t *rpas = &ev->data[LE_CONN_COMPLETE_ITVL_OFF];
  if (ev->length == LE_CONN_COMPLETE_LEN && ev->data[0] == BLE_HCI_LE_SUBEV_CONN_COMPLETE) {
    const size_t tail_len = LE_CONN_COMPLETE_LEN - LE_CONN_COMPLETE_ITVL_OFF;
    memmove(rpas + 12U, rpas, tail_len);
    memset(rpas + 6U, 0, 6U);
    ev->data[0] = BLE_HCI_LE_SUBEV_ENH_CONN_COMPLETE;
    ev->length = LE_ENH_CONN_COMPLETE_LEN;
  } else if (ev->length != LE_ENH_CONN_COMPLETE_LEN ||
             ev->data[0] != BLE_HCI_LE_SUBEV_ENH_CONN_COMPLETE ||
             memcmp(rpas, (const uint8_t[6]){0}, 6U) != 0) {
    return;
  }
  memcpy(rpas, s_rnd_in_use, 6U);
}

static void prv_note_rnd_addr_reply(struct ble_hci_ev *ev) {
  if (ev->opcode != BLE_HCI_EVCODE_COMMAND_COMPLETE || ev->length < 4U ||
      (ev->data[1] | (ev->data[2] << 8)) != HCI_OP_LE_SET_RANDOM_ADDR || ev->data[3] != 0U) {
    return;
  }
  memcpy(s_rnd_in_use, s_rnd_sent, sizeof(s_rnd_in_use));
  s_rnd_in_use_valid = true;
}

#ifdef CONFIG_BT_CONTROLLER_CC2564
// The CC2564B is a Bluetooth 4.1 controller: no resolving list (LL privacy)
// and no privacy modes. Answer those commands here instead, and resolve
// peers' resolvable private addresses against the bonded IRKs, reporting a
// resolved connection as the enhanced LE Connection Complete a 4.2
// controller would send. NimBLE then finds the bond as usual.
#define H4_SM_IDLE       0U // HCI_H4_SM_W4_PKT_TYPE in hci_h4.c
#define FAKE_REPLIES_MAX 4U

static uint16_t s_fake_replies[FAKE_REPLIES_MAX];
static volatile uint32_t s_fake_head;
static volatile uint32_t s_fake_tail;

static bool prv_is_ll_privacy_cmd(uint16_t opcode) {
  switch (opcode) {
    case BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_ADD_RESOLV_LIST):
    case BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_RMV_RESOLV_LIST):
    case BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_CLR_RESOLV_LIST):
    case BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_SET_ADDR_RES_EN):
    case BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_SET_RPA_TMO):
    case BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_SET_PRIVACY_MODE):
      return true;
    default:
      return false;
  }
}

//! Queue a successful Command Complete for @p opcode. The host waits for
//! each reply before sending the next command, so the queue stays short.
static bool prv_queue_fake_reply(uint16_t opcode) {
  bool queued = false;
  pbl_irq_lock();
  if (s_fake_head - s_fake_tail < FAKE_REPLIES_MAX) {
    s_fake_replies[s_fake_head % FAKE_REPLIES_MAX] = opcode;
    s_fake_head++;
    queued = true;
  }
  pbl_irq_unlock();
  pbl_sem_give(&s_rx_ready);
  return queued;
}

//! Only between packets, so a fake reply cannot split a real one.
static void prv_send_fake_replies(void) {
  while (s_fake_tail != s_fake_head && s_hci_h4sm.state == H4_SM_IDLE) {
    const uint16_t opcode = s_fake_replies[s_fake_tail % FAKE_REPLIES_MAX];
    s_fake_tail++;
    const uint8_t reply[] = {
      HCI_H4_EVT, BLE_HCI_EVCODE_COMMAND_COMPLETE, 4, 1, opcode & 0xffU, opcode >> 8U, 0,
    };
    hci_h4_sm_rx(&s_hci_h4sm, reply, sizeof(reply));
  }
}

struct ResolveContext {
  const uint8_t *rpa;
  ble_addr_t identity;
  bool found;
};

static int prv_resolve_cb(int obj_type, union ble_store_value *val, void *cookie) {
  struct ResolveContext *ctx = cookie;
  if (val->sec.irk_present && pbl_bt_le_irk_resolves(val->sec.irk, ctx->rpa)) {
    ctx->identity = val->sec.peer_addr;
    ctx->found = true;
    return 1;
  }
  return 0;
}

#ifdef CONFIG_SERVICE_SHARED_PRF_STORAGE_IMPORT_STOCK_V2
#define LE_LTK_REQ_LEN 13U

static volatile uint32_t s_legacy_ltk_used;
static volatile uint32_t s_legacy_ltk_mismatch;

//! A bond imported from the stock firmware has its LTK regenerated but not
//! the Rand the phone holds (stock never stored it), so it is stored with
//! Rand 0. When the phone asks for that key, regenerate it from the phone's
//! EDIV and Rand as stock does (DIV = EDIV ^ dm(DHK, Rand), LTK =
//! d1(ER, DIV, 0), with stock's DHK, ER and dm) and, if it is the stored
//! one, stand in Rand 0 so NimBLE finds it. Otherwise leave the request
//! alone: NimBLE declines and the phone pairs again.
static void prv_ltk_req_imported_bond(struct ble_hci_ev *ev) {
  if (ev->opcode != BLE_HCI_EVCODE_LE_META || ev->length != LE_LTK_REQ_LEN ||
      ev->data[0] != BLE_HCI_LE_SUBEV_LT_KEY_REQ) {
    return;
  }
  uint64_t rand = 0;
  for (int i = 0; i < 8; i++) {
    rand |= (uint64_t)ev->data[3 + i] << (8 * i);
  }
  const uint16_t ediv = ev->data[11] | (ev->data[12] << 8);
  if (rand == 0U) {
    return;
  }

  struct pbl_bt_sm_pairing_info info;
  struct pbl_bt_sm_key dhk;
  struct pbl_bt_sm_key ir;
  if (!shared_prf_storage_get_ble_pairing_data(&info, nullptr, nullptr, nullptr) ||
      !info.is_local_encryption_info_valid || info.local_encryption_info.rand != 0U ||
      info.local_encryption_info.ediv != ediv ||
      !shared_prf_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION, &dhk) ||
      !shared_prf_storage_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY, &ir)) {
    return;
  }

  uint16_t y;
  uint8_t er[16];
  uint8_t ltk[16];
  if (!pbl_bt_le_dm_stock(dhk.data, rand, &y) || !pbl_bt_le_d1(ir.data, 3U, 0U, er) ||
      !pbl_bt_le_d1(er, (uint16_t)(ediv ^ y), 0U, ltk)) {
    return;
  }
  if (memcmp(ltk, info.local_encryption_info.ltk.data, sizeof(ltk)) != 0) {
    s_legacy_ltk_mismatch++;
    return;
  }
  memset(&ev->data[3], 0, 8U);
  s_legacy_ltk_used++;
}
#endif

static void prv_conn_complete_resolve_peer(struct ble_hci_ev *ev) {
  if (ev->opcode != BLE_HCI_EVCODE_LE_META || ev->length != LE_CONN_COMPLETE_LEN ||
      ev->data[0] != BLE_HCI_LE_SUBEV_CONN_COMPLETE || ev->data[1] != 0U ||
      ev->data[5] != BLE_ADDR_RANDOM || !prv_addr_is_rpa(&ev->data[6])) {
    return;
  }

  uint8_t rpa[6];
  memcpy(rpa, &ev->data[6], sizeof(rpa));
  struct ResolveContext ctx = {.rpa = rpa};
  ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC, prv_resolve_cb, &ctx);
  if (!ctx.found) {
    return;
  }

  uint8_t *rpas = &ev->data[LE_CONN_COMPLETE_ITVL_OFF];
  memmove(rpas + 12U, rpas, LE_CONN_COMPLETE_LEN - LE_CONN_COMPLETE_ITVL_OFF);
  memset(rpas, 0, 6U);
  memcpy(rpas + 6U, rpa, sizeof(rpa));
  memcpy(&ev->data[6], ctx.identity.val, sizeof(ctx.identity.val));
  ev->data[5] = (ctx.identity.type == BLE_ADDR_PUBLIC) ? BLE_ADDR_PUBLIC_ID : BLE_ADDR_RANDOM_ID;
  ev->data[0] = BLE_HCI_LE_SUBEV_ENH_CONN_COMPLETE;
  ev->length = LE_ENH_CONN_COMPLETE_LEN;
}
#endif

bool pbl_bt_hci_uart_local_addr_outdated(void) {
  if (!s_rnd_in_use_valid) {
    return s_local_rpa_set;
  }
  if (s_local_rpa_set) {
    return memcmp(s_rnd_in_use, s_local_rpa, sizeof(s_local_rpa)) != 0;
  }
  return prv_addr_is_rpa(s_rnd_in_use);
}

void pbl_bt_hci_uart_set_local_rpa(const uint8_t *addr) {
  pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
  s_local_rpa_set = (addr != NULL);
  if (addr != NULL) {
    memcpy(s_local_rpa, addr, sizeof(s_local_rpa));
  }
  pbl_mutex_unlock(&s_tx_mutex);
}

static void prv_note_evt(struct ble_hci_ev *ev) {
  s_num_evts++;
#ifdef CONFIG_BT_CONTROLLER_CC2564
  // First, so the local RPA lands in the enhanced event it may make.
  prv_conn_complete_resolve_peer(ev);
#ifdef CONFIG_SERVICE_SHARED_PRF_STORAGE_IMPORT_STOCK_V2
  prv_ltk_req_imported_bond(ev);
#endif
#endif
  prv_conn_complete_add_local_rpa(ev);
  prv_note_rnd_addr_reply(ev);
  if (ev->opcode == BLE_HCI_EVCODE_NUM_COMP_PKTS && ev->length >= 1U) {
    for (uint8_t i = 0; i < ev->data[0] && 1U + 4U * (i + 1U) <= ev->length; i++) {
      uint8_t *entry = &ev->data[1 + 4 * i];
      uint16_t count = entry[2] | (entry[3] << 8);
      s_acl_completed += count;
      prv_note_completed(count);
    }
    return;
  }
  if (ev->opcode == BLE_HCI_EVCODE_COMMAND_COMPLETE ||
      ev->opcode == BLE_HCI_EVCODE_COMMAND_STATUS) {
    s_last_reply_len = ev->length;
  }
  if (ev->opcode == BLE_HCI_EVCODE_COMMAND_COMPLETE && ev->length >= 4U) {
    s_last_reply_cmd = ev->data[1] | (ev->data[2] << 8);
    s_last_reply_status = ev->data[3];
  } else if (ev->opcode == BLE_HCI_EVCODE_COMMAND_STATUS && ev->length >= 4U) {
    s_last_reply_cmd = ev->data[2] | (ev->data[3] << 8);
    s_last_reply_status = ev->data[0];
  } else {
    return;
  }
  if (s_last_reply_status != 0U) {
    s_last_fail_cmd = s_last_reply_cmd;
    s_last_fail_status = s_last_reply_status;
  }
}

static int prv_hci_frame_cb(uint8_t pkt_type, void *data) {
  switch (pkt_type) {
    case HCI_H4_EVT:
      prv_note_evt(data);
      return ble_transport_to_hs_evt(data);
    case HCI_H4_ACL:
      prv_note_smp('<', data);
      return ble_transport_to_hs_acl(data);
    default:
      pbl_panic();
  }

  return -1;
}

static void prv_rx_irq_handler(UARTDevice *dev, uint8_t data, const UARTRXErrorFlags *err_flags) {
  uint32_t head = s_rx_head;
  uint32_t next = (head + 1U) % RX_RING_SIZE;

  // The byte has already left the UART, so pause while there is still room
  // for it; the rest waits in the UART.
  s_rx_ring[head] = data;
  s_rx_head = next;

  if ((next + 1U) % RX_RING_SIZE == s_rx_tail) {
    s_rx_paused = true;
    uart_set_rx_interrupt_enabled(dev, false);
  }

  pbl_sem_give(&s_rx_ready);
}

static void prv_uart_start(void) {
  // A controller driver that boots the controller has set the UART up already.
  uart_set_rx_interrupt_handler(BOARD_BT_HCI_UART_DEV, prv_rx_irq_handler);
  uart_set_rx_interrupt_enabled(BOARD_BT_HCI_UART_DEV, true);
  s_controller_state = ControllerStateReady;
}

#ifdef CONFIG_BT_CC2564_SLEEP
// TI's eHCILL (SWRA288): the controller asks to sleep after it has been idle
// (GO_TO_SLEEP_IND); we hold our RTS high, so it keeps what it has, arm a
// wake interrupt on CTS, acknowledge and let STOP in. Either side wakes the
// other: the controller pulses its RTS (our CTS) and sends WAKE_UP_IND once
// we release RTS, or we send WAKE_UP_IND and wait for WAKE_UP_ACK. One STOP
// block is held whenever we are not asleep.
#define HCILL_GO_TO_SLEEP_IND 0x30U
#define HCILL_GO_TO_SLEEP_ACK 0x31U
#define HCILL_WAKE_UP_IND     0x32U
#define HCILL_WAKE_UP_ACK     0x33U
#define HCILL_WAKE_TIMEOUT_MS 100U
#define HCILL_WAKE_ATTEMPTS   5
//! A CTS edge with no WAKE_UP_IND this long after it was not a wake pulse.
#define HCILL_CTRL_WAKE_TIMEOUT_MS 50U

static volatile enum {
  HcillAwake,
  HcillAsleep,
  HcillWaking,     // we sent WAKE_UP_IND
  HcillCtrlWaking, // the controller pulsed CTS: waiting for its WAKE_UP_IND
} s_hcill = HcillAwake;
static PBL_SEM_DEFINE(s_hcill_awake, 0, 1);
static volatile uint32_t s_hcill_sleeps;
static volatile uint32_t s_hcill_ctrl_wakes;
static volatile uint32_t s_hcill_host_wakes;
static volatile uint32_t s_hcill_wake_retries;
static volatile uint32_t s_hcill_false_wakes;
static volatile uint64_t s_hcill_ctrl_wake_ticks;

//! With s_tx_mutex held.
static void prv_hcill_send(uint8_t msg) {
  cc2564_send_hcill(msg);
}

static void prv_hcill_cts_wake_isr(void *arg) {
  if (s_hcill != HcillAsleep) {
    return;
  }
  cc2564_cts_wake_disable();
  soc_stm32_stop_block();
  s_hcill = HcillCtrlWaking;
  s_hcill_ctrl_wake_ticks = pbl_uptime_ticks();
  s_hcill_ctrl_wakes++;
  // Let the controller send its WAKE_UP_IND.
  cc2564_hold_rts(false);
  pbl_sem_give(&s_rx_ready);
}

static void prv_hcill_set_awake(void) {
  s_hcill = HcillAwake;
  pbl_sem_give(&s_hcill_awake);
}

//! In the HCI task, for an eHCILL message where a packet type would be.
static void prv_hcill_rx(uint8_t msg) {
  switch (msg) {
    case HCILL_GO_TO_SLEEP_IND:
      if (s_hcill != HcillAwake) {
        // Waking up: one queued before our WAKE_UP_IND, to be ignored.
        return;
      }
      pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
      cc2564_hold_rts(true);
      pbl_irq_lock();
      s_hcill = HcillAsleep;
      pbl_irq_unlock();
      cc2564_cts_wake_enable(prv_hcill_cts_wake_isr);
      prv_hcill_send(HCILL_GO_TO_SLEEP_ACK);
      pbl_mutex_unlock(&s_tx_mutex);
      s_hcill_sleeps++;
      // A wake pulse may already have taken the block back.
      soc_stm32_stop_release();
      return;
    case HCILL_WAKE_UP_IND:
      // If both sides woke at once, this is the acknowledgment of ours, but
      // the controller still waits for one of its own (Linux hci_ll does the
      // same): without it, it never sends again.
      pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
      pbl_irq_lock();
      if (s_hcill == HcillAsleep) {
        // Missed the pulse.
        cc2564_cts_wake_disable();
        soc_stm32_stop_block();
        cc2564_hold_rts(false);
      }
      pbl_irq_unlock();
      prv_hcill_send(HCILL_WAKE_UP_ACK);
      prv_hcill_set_awake();
      pbl_mutex_unlock(&s_tx_mutex);
      return;
    case HCILL_WAKE_UP_ACK:
      if (s_hcill == HcillWaking) {
        prv_hcill_set_awake();
      }
      return;
    default:
      return;
  }
}

//! Back to sleep after an incomplete wake, with interrupts locked.
static void prv_hcill_sleep_locked(void) {
  s_hcill = HcillAsleep;
  cc2564_cts_wake_enable(prv_hcill_cts_wake_isr);
  cc2564_hold_rts(true);
}

//! In the HCI task: a CTS edge that no WAKE_UP_IND followed was not the
//! controller waking us (e.g. its RTS settling as it went to sleep). Go back
//! to sleep.
static void prv_hcill_check_false_wake(void) {
  if (s_hcill != HcillCtrlWaking || (pbl_uptime_ticks() - s_hcill_ctrl_wake_ticks) <
                                        (HCILL_CTRL_WAKE_TIMEOUT_MS * PBL_TICK_HZ) / 1000U) {
    return;
  }
  pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
  bool back_to_sleep = false;
  pbl_irq_lock();
  if (s_hcill == HcillCtrlWaking) {
    prv_hcill_sleep_locked();
    back_to_sleep = true;
  }
  pbl_irq_unlock();
  if (back_to_sleep) {
    s_hcill_false_wakes++;
    soc_stm32_stop_release();
  }
  pbl_mutex_unlock(&s_tx_mutex);
}

static bool prv_is_hcill(uint8_t c) {
  return c >= HCILL_GO_TO_SLEEP_IND && c <= HCILL_WAKE_UP_ACK;
}

//! Take s_tx_mutex with the controller awake, waking it if needed.
//! @return false if it would not wake; the mutex is then not held.
static bool prv_hcill_lock_awake(void) {
  for (int attempt = 0; attempt < HCILL_WAKE_ATTEMPTS; attempt++) {
    pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
    if (s_hcill == HcillAwake) {
      return true;
    }

    // Whatever the state, send WAKE_UP_IND: if the controller is waking us
    // at the same time, its WAKE_UP_IND then counts as the acknowledgment.
    pbl_irq_lock();
    if (s_hcill == HcillAsleep) {
      cc2564_cts_wake_disable();
      soc_stm32_stop_block();
      s_hcill_host_wakes++;
    } else {
      s_hcill_wake_retries++;
    }
    s_hcill = HcillWaking;
    pbl_irq_unlock();
    (void)pbl_sem_take(&s_hcill_awake, PBL_NO_WAIT);
    cc2564_hold_rts(false);
    prv_hcill_send(HCILL_WAKE_UP_IND);
    // The HCI task needs the mutex to answer the controller.
    pbl_mutex_unlock(&s_tx_mutex);

    (void)pbl_sem_take(&s_hcill_awake, PBL_MSEC(HCILL_WAKE_TIMEOUT_MS));
  }

  // Left waking, the controller's next wake pulse would be ignored and every
  // packet dropped, with the host still holding its links. Go back to sleep
  // and reset the host: it drops the links, so advertising starts again, and
  // its HCI Reset wakes the controller anew.
  pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
  bool back_to_sleep = false;
  pbl_irq_lock();
  if (s_hcill == HcillWaking) {
    prv_hcill_sleep_locked();
    back_to_sleep = true;
  }
  pbl_irq_unlock();
  if (back_to_sleep) {
    soc_stm32_stop_release();
  }
  pbl_mutex_unlock(&s_tx_mutex);

  static uint32_t s_wake_failures;
  s_wake_failures++;
  if (s_wake_failures <= 3U || (s_wake_failures % 256U) == 0U) {
    PBL_LOG_ERR("CC2564 did not wake up (%lu)", (unsigned long)s_wake_failures);
  }
  ble_hs_sched_reset(BLE_HS_ECONTROLLER);
#ifdef CONFIG_BT_CONTROLLER_CC2564
  // A controller that does not wake is wedged: power cycle it.
  pbl_bt_hci_uart_controller_unresponsive();
#endif
  return false;
}
#endif

#ifdef CONFIG_BT_CONTROLLER_CC2564
// Power cycles after the controller stopped answering, at most this many per boot; then it is
// left off rather than the host resetting forever.
#define MAX_CONTROLLER_REBOOTS 3U

// The host times out a command after 2 s. One sent while the controller
// rebooted was dropped, so its timeout says nothing about the new controller.
#define CONTROLLER_REBOOT_GRACE_MS 3000U

static volatile bool s_reboot_requested;
//! The controller stopped taking bytes: power cycle it although it is no longer Ready.
static volatile bool s_tx_stall;
static uint32_t s_reboots;
static uint64_t s_rebooted_at;

//! Stop talking to the controller: writers drop their bytes, eHCILL and the RX path start
//! over. Gives back the STOP block held while it is awake.
static void prv_controller_detach(void) {
  pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
  s_controller_state = ControllerStatePending;
  pbl_mutex_unlock(&s_tx_mutex);

  uart_set_rx_interrupt_enabled(BOARD_BT_HCI_UART_DEV, false);

  bool asleep = false;
#ifdef CONFIG_BT_CC2564_SLEEP
  pbl_irq_lock();
  asleep = (s_hcill == HcillAsleep);
  s_hcill = HcillAwake;
  pbl_irq_unlock();
  cc2564_cts_wake_disable();
  cc2564_hold_rts(false);
#endif
  if (!asleep) {
    soc_stm32_stop_release();
  }

  pbl_irq_lock();
  s_rx_head = 0U;
  s_rx_tail = 0U;
  s_rx_paused = false;
  pbl_irq_unlock();
  // A packet cut short would otherwise prefix the next one.
  hci_h4_sm_init(&s_hci_h4sm, &s_hci_h4_allocs_from_ll, prv_hci_frame_cb);
}

//! On the HCI task: power cycle a controller that stopped answering, through its boot
//! sequence. The host then resyncs with an HCI Reset.
static void prv_controller_reboot(void) {
  s_reboot_requested = false;
  const bool stalled = s_tx_stall;
  s_tx_stall = false;
  if (s_controller_state != ControllerStateReady && !stalled) {
    return;
  }
  // The grace period is for host command timeouts the last reboot caused, not for a stall.
  if (!stalled && s_reboots > 0U &&
      (pbl_uptime_ticks() - s_rebooted_at) < (CONTROLLER_REBOOT_GRACE_MS * PBL_TICK_HZ) / 1000U) {
    return;
  }

  prv_controller_detach();
  if (s_reboots >= MAX_CONTROLLER_REBOOTS) {
    cc2564_shutdown();
    s_controller_state = ControllerStateFailed;
    PBL_LOG_ERR("CC2564 stopped answering again after %lu power cycles: left off",
                (unsigned long)s_reboots);
    return;
  }
  s_reboots++;
  PBL_LOG_WRN("CC2564 stopped answering: power cycling it (%lu)", (unsigned long)s_reboots);

  // Bounded by the boot attempts.
  pbl_task_wdt_set_waiting(true);
  const bool booted = cc2564_boot();
  pbl_task_wdt_set_waiting(false);
  // The host timed out on commands dropped meanwhile: those were this reboot.
  s_reboot_requested = false;

  if (booted) {
    prv_uart_start();
    s_rebooted_at = pbl_uptime_ticks();
#ifdef CONFIG_BT_CC2564_SLEEP
    if (cc2564_take_sleep_ind()) {
      prv_hcill_rx(HCILL_GO_TO_SLEEP_IND);
    }
#endif
  } else {
    // cc2564_boot() left the chip off and gave its STOP block back.
    s_controller_state = ControllerStateFailed;
  }
}
#endif

#ifdef CONFIG_LEGACY_BT_CONTROLLER_OFF_WHEN_STOPPED
static volatile bool s_power_off_requested;
static volatile bool s_power_on_requested;
static volatile bool s_powered_off;

//! On the HCI task: hold the controller in shutdown while Bluetooth is off (deep sleep still
//! draws tens of microamps), and boot it again, init script included, when it comes back on.
static void prv_controller_power(void) {
  if (s_power_off_requested) {
    s_power_off_requested = false;
    if (!s_powered_off) {
      // A controller that failed to boot is off already, its STOP block given back; one that
      // stalled is still awake.
      if (s_controller_state == ControllerStateReady || s_tx_stall) {
        s_tx_stall = false;
        prv_controller_detach();
      }
      cc2564_shutdown();
      s_controller_state = ControllerStatePending;
      s_powered_off = true;
    }
  }
  if (s_power_on_requested) {
    // Failed: a boot failed (here or earlier) and left the chip off, its STOP block given back.
    const bool boot = s_powered_off || s_controller_state == ControllerStateFailed;
    if (boot) {
      // Pending before the request is cleared, so the host start waits for this boot instead
      // of taking the last one's failure.
      s_controller_state = ControllerStatePending;
    }
    s_power_on_requested = false;
    if (boot) {
      if (s_tx_stall) {
        // Stalled since the reboot check above: still awake, holding its STOP block.
        s_tx_stall = false;
        prv_controller_detach();
      }
      s_powered_off = false;
      pbl_task_wdt_set_waiting(true);
      const bool booted = cc2564_boot();
      pbl_task_wdt_set_waiting(false);
      if (booted) {
        prv_uart_start();
#ifdef CONFIG_BT_CC2564_SLEEP
        if (cc2564_take_sleep_ind()) {
          prv_hcill_rx(HCILL_GO_TO_SLEEP_IND);
        }
#endif
      } else {
        s_controller_state = ControllerStateFailed;
      }
    }
  }
}

void pbl_bt_hci_uart_controller_power(bool on) {
  if (on) {
    s_power_on_requested = true;
  } else {
    s_power_off_requested = true;
  }
  pbl_sem_give(&s_rx_ready);
  if (on) {
    // The host start waits for the controller with pbl_bt_hci_uart_wait_ready().
    return;
  }
  for (uint32_t waited_ms = 0U; !s_powered_off && waited_ms < 1000U; waited_ms += 10U) {
    pbl_thread_sleep(PBL_MSEC(10));
  }
}
#endif

static void prv_hci_task_main(void *unused) {
#ifdef CONFIG_BT_CONTROLLER_CC2564
  // Loading TI's init script takes a second or two, so it happens here rather
  // than in ble_transport_ll_init(). A failure leaves Bluetooth off instead of
  // crashing, since a crash loop would end in the bootloader starting PRF.
  if (cc2564_boot()) {
    prv_uart_start();
#ifdef CONFIG_BT_CC2564_SLEEP
    if (cc2564_take_sleep_ind()) {
      prv_hcill_rx(HCILL_GO_TO_SLEEP_IND);
    }
#endif
  } else {
    s_controller_state = ControllerStateFailed;
  }
#endif

  while (true) {
#ifdef CONFIG_BT_CC2564_SLEEP
    (void)pbl_sem_take(&s_rx_ready, (s_hcill == HcillCtrlWaking)
                                        ? PBL_MSEC(HCILL_CTRL_WAKE_TIMEOUT_MS)
                                        : PBL_FOREVER);
    prv_hcill_check_false_wake();
#else
    pbl_sem_take(&s_rx_ready, PBL_FOREVER);
#endif
#ifdef CONFIG_BT_CONTROLLER_CC2564
    if (s_reboot_requested) {
      prv_controller_reboot();
    }
#endif
#ifdef CONFIG_LEGACY_BT_CONTROLLER_OFF_WHEN_STOPPED
    prv_controller_power();
#endif

    while (s_rx_tail != s_rx_head) {
      uint32_t tail = s_rx_tail;
      uint32_t head = s_rx_head;
      uint32_t len = (head > tail) ? (head - tail) : (RX_RING_SIZE - tail);

#ifdef CONFIG_BT_CC2564_SLEEP
      // eHCILL messages come where a packet type would: pick them out.
      int consumed = 0;
      while ((uint32_t)consumed < len) {
        const uint8_t c = s_rx_ring[tail + consumed];
        if (s_hci_h4sm.state == H4_SM_IDLE && prv_is_hcill(c)) {
          consumed++;
          prv_hcill_rx(c);
          continue;
        }
        const int n = hci_h4_sm_rx(&s_hci_h4sm, &s_rx_ring[tail + consumed], 1);
        if (n <= 0) {
          PBL_LOG_ERR("hci_h4_sm_rx returned %d", n);
        }
        consumed++;
      }
#else
      int consumed = hci_h4_sm_rx(&s_hci_h4sm, &s_rx_ring[tail], len);
      if (consumed <= 0) {
        PBL_LOG_ERR("hci_h4_sm_rx returned %d", consumed);
        consumed = len;
      }
#endif
      s_rx_tail = (tail + consumed) % RX_RING_SIZE;

      if (s_rx_paused) {
        s_rx_paused = false;
        uart_set_rx_interrupt_enabled(BOARD_BT_HCI_UART_DEV, true);
      }
    }

#ifdef CONFIG_BT_CONTROLLER_CC2564
    prv_send_fake_replies();
#endif
  }
}

#ifdef CONFIG_BT_CONTROLLER_CC2564
//! Give up on a controller that holds CTS off this long: it would otherwise
//! hang the writer until the watchdog resets the watch.
#define TX_STALL_TIMEOUT_MS 2000U

//! A byte leaves in ~11 us at 921600 baud, so the transmit register is busy
//! for most of every write. Spin about three byte times before sleeping: a
//! 1 ms sleep per byte held BLE sends, which run on KernelMain, to ~1 KB/s and
//! stalled the UI. Keep it short: the HCI RX thread has KernelMain's priority
//! and cannot drain the controller while KernelMain spins.
#define TX_SPIN_US 30U

//! @return false if the controller did not take the byte in time.
static bool prv_write_byte_bounded(uint8_t byte) {
  const uint32_t spin_start = pbl_cycle_get_32();
  const uint32_t spin_cycles = TX_SPIN_US * (pbl_cycles_per_sec() / 1000000U);
  while (!uart_is_tx_ready(BOARD_BT_HCI_UART_DEV) &&
         (pbl_cycle_get_32() - spin_start) < spin_cycles) {
  }
  // Still busy: the controller holds CTS off, e.g. while it wakes up.
  if (PBL_POLL_SLEEP_MS(uart_is_tx_ready(BOARD_BT_HCI_UART_DEV), 1U, TX_STALL_TIMEOUT_MS) != 0) {
    return false;
  }
  uart_write_byte(BOARD_BT_HCI_UART_DEV, byte);
  return true;
}

static void prv_tx_stalled(void) {
  // Writers drop their bytes from here on; the HCI task power cycles the controller. The stall
  // is flagged first, so a power-off in between still detaches the awake controller.
  s_tx_stall = true;
  s_controller_state = ControllerStateFailed;
  cc2564_report_stall(s_rx_paused, s_evt_waiting);
  pbl_bt_hci_uart_controller_unresponsive();
}
#endif

static void prv_write(const uint8_t *data, size_t len) {
  if (s_controller_state != ControllerStateReady) {
    return;
  }

#ifdef CONFIG_BT_CONTROLLER_CC2564
  for (size_t i = 0U; i < len; i++) {
    if (!prv_write_byte_bounded(data[i])) {
      prv_tx_stalled();
      return;
    }
  }
#else
  for (size_t i = 0U; i < len; i++) {
    uart_write_byte(BOARD_BT_HCI_UART_DEV, data[i]);
  }
#endif
}

static void prv_write_mbuf(uint8_t type, struct os_mbuf *om) {
  if (type == HCI_H4_ACL) {
    s_acl_tx++;
    prv_note_smp('>', om);
  }
#ifdef CONFIG_BT_CC2564_SLEEP
  if (!prv_hcill_lock_awake()) {
    os_mbuf_free_chain(om);
    return;
  }
#else
  pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
#endif
  prv_write(&type, 1U);
  for (struct os_mbuf *x = om; x != NULL; x = SLIST_NEXT(x, om_next)) {
    prv_write(x->om_data, x->om_len);
  }
  pbl_mutex_unlock(&s_tx_mutex);

  os_mbuf_free_chain(om);
}

void ble_transport_ll_init(void) {
  hci_h4_sm_init(&s_hci_h4sm, &s_hci_h4_allocs_from_ll, prv_hci_frame_cb);
  ble_transport_register_put_acl_from_ll_cb(prv_acl_put_signal);

  struct pbl_thread_attr attr = {
    .name = "NimbleHCI",
    .entry = prv_hci_task_main,
    .prio = CONFIG_BT_HCI_THREAD_PRIO,
    .privileged = true,
    .stack = s_hci_task_stack,
    .stack_size = sizeof(s_hci_task_stack),
  };

  PBL_ASSERTN(pbl_thread_create(&s_hci_thread, &attr) == 0);
  pebble_task_register(PebbleTask_BTHCI, &s_hci_thread);
}

//! The controller is booting, or a power change for it is still waiting for the HCI task.
static bool prv_controller_settling(void) {
#ifdef CONFIG_LEGACY_BT_CONTROLLER_OFF_WHEN_STOPPED
  if (s_power_on_requested || s_power_off_requested) {
    return true;
  }
#endif
  return s_controller_state == ControllerStatePending;
}

bool pbl_bt_hci_uart_wait_ready(uint32_t timeout_ms) {
  const uint32_t step_ms = 50U;

  for (uint32_t waited = 0U; prv_controller_settling(); waited += step_ms) {
    if (waited >= timeout_ms) {
      return false;
    }
    pbl_task_wdt_feed_self();
    pbl_thread_sleep(PBL_MSEC(step_ms));
  }

  return s_controller_state == ControllerStateReady;
}

void pbl_bt_hci_uart_snapshot(void) {
  // Keep the first reset since boot: later ones follow from it.
  if (s_reset_snapshot[0] != '\0') {
    return;
  }
  sniprintf(s_reset_snapshot, sizeof(s_reset_snapshot), "%04x>%04x/%u a%lu n%lu",
            (unsigned)s_last_cmd, (unsigned)s_last_reply_cmd, (unsigned)s_last_reply_len,
            (unsigned long)s_acl_tx, (unsigned long)s_acl_completed);
}

uint32_t pbl_bt_hci_uart_acl_clamped(void) {
  return 0U;
}

void pbl_bt_hci_uart_copy_rx_stats(char *buf, size_t len) {
#ifdef CONFIG_BT_CC2564_SLEEP
  static const char states[] = {'A', 'S', 'W', 'C'};
  sniprintf(buf, len, "%c z%lu c%lu h%lu r%lu f%lu", states[s_hcill], (unsigned long)s_hcill_sleeps,
            (unsigned long)s_hcill_ctrl_wakes, (unsigned long)s_hcill_host_wakes,
            (unsigned long)s_hcill_wake_retries, (unsigned long)s_hcill_false_wakes);
#else
  buf[0] = '\0';
#endif
}

uint16_t pbl_bt_hci_uart_le_acl_len_reported(void) {
  return 0U;
}

void pbl_bt_hci_uart_controller_unresponsive(void) {
  s_reboot_requested = true;
  pbl_sem_give(&s_rx_ready);
}

const char *pbl_bt_hci_uart_reset_snapshot(void) {
  return s_reset_snapshot;
}

void pbl_bt_hci_uart_copy_status(char *buf, size_t len) {
  sniprintf(buf, len, "%04x:%02x !%04x:%02x", (unsigned)s_last_reply_cmd,
            (unsigned)s_last_reply_status, (unsigned)s_last_fail_cmd, (unsigned)s_last_fail_status);
}

int ble_transport_to_ll_cmd_impl(void *buf) {
  struct ble_hci_cmd *cmd = buf;
  uint8_t type = HCI_H4_CMD;

  s_last_cmd = le16toh(cmd->opcode);

#ifdef CONFIG_BT_CONTROLLER_CC2564
  if (prv_is_ll_privacy_cmd(s_last_cmd) && prv_queue_fake_reply(s_last_cmd)) {
    ble_transport_free(buf);
    return 0;
  }
#endif

#ifdef CONFIG_BT_CC2564_SLEEP
  if (!prv_hcill_lock_awake()) {
    ble_transport_free(buf);
    return 0;
  }
#else
  pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
#endif
  if (s_last_cmd == HCI_OP_LE_SET_RANDOM_ADDR && cmd->length == sizeof(s_local_rpa)) {
    if (s_local_rpa_set) {
      memcpy(cmd->data, s_local_rpa, sizeof(s_local_rpa));
    }
    memcpy(s_rnd_sent, cmd->data, sizeof(s_rnd_sent));
  } else if (s_last_cmd == HCI_OP_RESET) {
    s_rnd_in_use_valid = false;
  }
  prv_write(&type, 1U);
  prv_write(buf, sizeof(*cmd) + cmd->length);
  pbl_mutex_unlock(&s_tx_mutex);

  ble_transport_free(buf);

  return 0;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  prv_write_mbuf(HCI_H4_ACL, om);
  return 0;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  prv_write_mbuf(HCI_H4_ISO, om);
  return 0;
}
