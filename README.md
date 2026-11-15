<p align="center">
  <img src="docs/_static/images/logo.svg" alt="PebbleOS" width="320">
</p>

# PebbleOS for the Pebble Time

[Core Devices' PebbleOS](https://github.com/coredevices/PebbleOS) on the
Pebble Time (`snowy`), a watch Pebble Technology made in 2015 and Core's
firmware no longer supports. The port is a series of topic commits on top
of a Core release tag (today `v4.47.0`), kept so that Core's own boards
build to the same objects as without it.

It started as a curated fork of Brian Silvestri's
[PebbleOS-legacy-watches](https://github.com/bjsilvest/PebbleOS-legacy-watches);
most of the driver code is still his, and his files keep his copyright
line.

> **Not official.** Not made, endorsed or supported by Core Devices, Google
> or Pebble Technology. "Pebble" is a trademark of its owner and is used
> here only to say which watches these builds run on.
>
> **Use at your own risk.** No warranty (Apache 2.0, sections 7-8). A
> Pebble Time is sealed: there is no debug port to rescue it with. The
> builds replace only the normal firmware, never the stock bootloader, the
> stock recovery firmware or the factory data, so recovery mode stays
> available; getting a firmware back from recovery mode needs a PC with
> Bluetooth Classic, not a phone. Read the
> [recovery notes](https://github.com/ysfsvm/PebbleOS-legacy-firmware/blob/main/docs/RECOVERY.md)
> before installing anything.
>
> **AI assistance.** The port and this fork were made with LLM coding
> tools. Commits that used them carry an `Assisted-by:` trailer.

## Status

| Watch | Board | Status |
|---|---|---|
| Pebble Time | `snowy@dvt` | Runs on one retail watch: display, buttons, backlight, vibe, compass, microphone and dictation, charging, Bluetooth LE with passkey pairing, notifications, firmware updates, deep sleep. Battery life is not measured yet. |
| Pebble Time Steel | `snowy@s3` | Builds. Never run on a watch. |
| Pebble Time Round | `spalding` | Builds. Never run on a watch; its round display path has never executed. Do not install it on a watch you cannot afford to lose. |

The board page, [`docs/boards/snowy`](docs/boards/snowy/index.md), has the
hardware, the flash layout, what is shared with the stock firmware, the
feature table and the open issues.

Bluetooth is LE only, through NimBLE: Bluetooth Classic and the original
Pebble phone app are not supported.

The Pebble 2 (`silk`) port is parked on the branch `dev/rebase-v4.46` and
has not been carried to this base.

## Branches

| Branch | Content |
|---|---|
| `main` | The port on the current Core release. |
| `dev/<topic>` | Work in progress; rewritten freely. |
| `dev/rebase-v4.46`, `silk/v4.46` | The last state on Core `v4.46.0`, with the Pebble 2. |
| `probe/v4.47` | `main` plus the measurement build (`CONFIG_LEGACY_PERF_PROBE`): scripted key sequence, frame timing, sampling profiler. |

Tags named `archive/<name>` keep earlier branches for reference.

## Building

Follow Core's [getting started guide](https://pebbleos.readthedocs.io/latest/getting_started/index.html)
to set up the build, then:

```shell
pbl configure --board snowy@dvt -DCONFIG_RELEASE=y
pbl build
pbl bundle
```

The recovery firmware variant (`--variant=prf`) is not part of this port:
the watch keeps Pebble's.

## Testing and tools

The emulator build (`-DCONFIG_SOC_STM32F4_EMULATED=y`) runs Core's
integration tests behind the stock bootloader; the board page says how.
The tooling used for this port lives in the documentation repository,
[PebbleOS-legacy-firmware](https://github.com/ysfsvm/PebbleOS-legacy-firmware):
image registry and clean builds, emulator and firmware update tests, a
CC2564 stand-in for the emulator, the object-by-object comparison of Core's
boards, watch log tools, PC-side recovery, and the measurement protocol.
The same repository holds the known issues, the incident reports and the
roadmap.

The phone app is Core's, with the changes these watches need, in
[mobileapp-legacy](https://github.com/ysfsvm/mobileapp-legacy).

## Licenses

PebbleOS and the changes in this tree are under the Apache License 2.0
(`LICENSE`). Third-party components keep their own licenses, listed in
`third_party/sbom.yml`:

- the TI Bluetooth init script (`third_party/ti_bt_sp`) is shipped
  unmodified under TI's licence, for use only with TI devices;
- the STM32 Standard Peripheral Library and OpenPDMFilter come from the
  submodule `third_party/hal_stm32/stm32-sdk` under ST's licence;
- the display FPGA bitstreams of the Pebble Time and Time Round are the
  files of the PebbleOS source Google released.
