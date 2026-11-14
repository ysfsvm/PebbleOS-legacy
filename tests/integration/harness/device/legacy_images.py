# SPDX-FileCopyrightText: 2026 Brian Silvestri
# SPDX-License-Identifier: Apache-2.0

"""Flash images for QEMU's models of the legacy watches (pebble-snowy-bb,
pebble-s4-bb), which boot the stock bootloader."""

import os
import subprocess
from pathlib import Path

from harness.errors import HarnessError

# The stock bootloaders are taken from git history and only ever used here.
BOARDS = {
    "snowy": {
        "protected": (
            ("PRF", 0x200000, 0x300000),
            ("stock shared PRF storage", 0x0C0000, 0x0E0000),
            ("MFG info", 0x0E0000, 0x100000),
        ),
        "bootloader": "c6288702d^:bin/boot/boot_snowy_dvt@1478015115.hex",
        "flash_size": 0x1000000,
        "resources_begin": 0x300000,
    },
}
BOARDS["spalding"] = dict(
    BOARDS["snowy"], bootloader="c6288702d^:bin/boot/boot_spalding@1478015115.hex"
)


def build_stock_images(board, build_dir, tmp):
    """Stock bootloader + firmware, and an external flash with resources."""
    from intelhex import IntelHex

    boot_hex = os.path.join(tmp, "boot.hex")
    with open(boot_hex, "wb") as f:
        f.write(subprocess.check_output(["git", "show", board["bootloader"]]))

    boot = IntelHex(boot_hex)
    fw = IntelHex(os.path.join(build_dir, "pebbleos.hex"))
    fw.start_addr = None
    if not boot.maxaddr() < fw.minaddr():
        raise HarnessError("firmware overlaps the bootloader")

    micro = IntelHex()
    micro.merge(boot)
    micro.merge(fw, overlap="error")
    micro.padding = 0xFF
    end = ((micro.maxaddr() + 511) // 512) * 512
    micro_bin = os.path.join(tmp, "micro.bin")
    micro.tobinfile(micro_bin, start=0x08000000, end=end - 1)

    spi = bytearray(b"\xff" * board["flash_size"])
    res = Path(build_dir, "system_resources.pbpack").read_bytes()
    begin = board["resources_begin"]
    spi[begin : begin + len(res)] = res
    return micro_bin, spi
