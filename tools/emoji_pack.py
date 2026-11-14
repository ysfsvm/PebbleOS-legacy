#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 retake
# SPDX-License-Identifier: Apache-2.0
"""Build the emoji pack: the full colour emoji fonts as a resource pack file.

Boards whose resource bank holds only a subset of the colour emoji (snowy) declare an "emoji"
file store in their resource map: the GOTHIC_14/18/24/28_EMOJI_EXTENDED fonts, in that order.
With CONFIG_LEGACY_EMOJI_PACK the firmware draws an emoji its subset lacks from this file,
installed on the watch as "emoji" (PutBytes, object type file).

usage: emoji_pack.py OUTPUT [--fonts DIR]
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pbpack import ResourcePack  # noqa: E402

TOPDIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIZES = (14, 18, 24, 28)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output")
    parser.add_argument(
        "--fonts",
        default=os.path.join(TOPDIR, "resources", "normal", "base", "pbf"),
        help="directory with EMOJI_<size>~color.pbf",
    )
    args = parser.parse_args()

    pack = ResourcePack(is_system=False)
    for size in SIZES:
        with open(os.path.join(args.fonts, f"EMOJI_{size}~color.pbf"), "rb") as f:
            pack.add_resource(f.read())
    with open(args.output, "wb") as f:
        crc = pack.serialize(f)
    print(f"{args.output}: {len(SIZES)} fonts, {os.path.getsize(args.output)} bytes, crc {crc:#x}")


if __name__ == "__main__":
    main()
