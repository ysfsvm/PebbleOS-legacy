#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 retake
# SPDX-License-Identifier: Apache-2.0
"""Make a standard H4 controller look like a TI CC2564B to the legacy transport.

usage: cc2564_shim.py LISTEN_PORT UPSTREAM_HOST:PORT [LOG]

Listens on 127.0.0.1:LISTEN_PORT for the watch's USART1 (QEMU: -serial tcp:...), connects to
the upstream controller (e.g. Bumble's virtual one) and passes HCI through, except what only
the CC2564B speaks: TI vendor commands (the init script, the baud rate change, sleep settings)
get a successful Command Complete here, and the eHCILL sleep protocol is answered here (a wake
request gets its acknowledgement; the controller never asks to sleep). Packets are forwarded
whole, so replies made here never split one from upstream.
"""

import socket
import struct
import sys
import time

LISTEN = int(sys.argv[1])
UP_HOST, UP_PORT = sys.argv[2].rsplit(":", 1)
LOG = open(sys.argv[3], "a") if len(sys.argv) > 3 else None

H4_CMD, H4_ACL, H4_EVT = 0x01, 0x02, 0x04
EHCILL_WAKE_UP_IND, EHCILL_WAKE_UP_ACK = 0x32, 0x33


def log(msg):
    if LOG:
        LOG.write(f"{time.monotonic():.3f} {msg}\n")
        LOG.flush()


class H4Reader:
    """Splits a byte stream into whole H4 packets (eHCILL bytes are packets of one)."""

    def __init__(self, upstream):
        self.buf = bytearray()
        self.upstream = upstream

    def feed(self, data):
        self.buf += data
        while self.buf:
            kind = self.buf[0]
            if 0x30 <= kind <= 0x33:
                n = 1
            elif kind == H4_CMD:
                if len(self.buf) < 4:
                    return
                n = 4 + self.buf[3]
            elif kind == H4_ACL:
                if len(self.buf) < 5:
                    return
                n = 5 + struct.unpack_from("<H", self.buf, 3)[0]
            elif kind == H4_EVT:
                if len(self.buf) < 3:
                    return
                n = 3 + self.buf[2]
            else:
                log(f"resync: dropping {kind:#04x}")
                del self.buf[0]
                continue
            if len(self.buf) < n:
                return
            pkt = bytes(self.buf[:n])
            del self.buf[:n]
            yield pkt


def main():
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", LISTEN))
    srv.listen(1)
    up = socket.create_connection((UP_HOST, int(UP_PORT)))
    log(f"upstream {UP_HOST}:{UP_PORT}")
    while True:
        watch, _ = srv.accept()
        log("watch connected")
        serve(watch, up)


def serve(watch, up):
    import select

    from_watch = H4Reader(True)
    from_up = H4Reader(False)
    while True:
        r, _, _ = select.select([watch, up], [], [])
        if watch in r:
            data = watch.recv(4096)
            if not data:
                log("watch closed")
                watch.close()
                return
            for pkt in list(from_watch.feed(data)):
                kind = pkt[0]
                if kind == EHCILL_WAKE_UP_IND:
                    watch.sendall(bytes([EHCILL_WAKE_UP_ACK]))
                    log("eHCILL wake -> ack")
                elif 0x30 <= kind <= 0x33:
                    log(f"eHCILL {kind:#04x} ignored")
                elif kind == H4_CMD and (pkt[2] >> 2) == 0x3F:
                    opcode = struct.unpack_from("<H", pkt, 1)[0]
                    watch.sendall(bytes([H4_EVT, 0x0E, 4, 1]) + struct.pack("<H", opcode) + b"\x00")
                    log(f"vendor {opcode:#06x} answered")
                else:
                    up.sendall(pkt)
        if up in r:
            data = up.recv(4096)
            if not data:
                log("upstream closed")
                sys.exit(1)
            for pkt in list(from_up.feed(data)):
                watch.sendall(pkt)


if __name__ == "__main__":
    main()
