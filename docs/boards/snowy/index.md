# Pebble Time (snowy)

## Overview

:::{div} board-card

:Board: `snowy`
:Revisions: `dvt`, `s3`
:SoC: ST STM32F439
:Platform: Basalt
:Emulator: `snowy@dvt` with `CONFIG_SOC_STM32F4_EMULATED=y`
:::

Pebble Time (`dvt`) and Pebble Time Steel (`s3`) are rectangular color
watches from Pebble Technology, built around the ST STM32F439. This firmware
replaces only their normal firmware: the stock bootloader and stock recovery
firmware stay. Apps see the Basalt platform, a 144 × 168 color display.

The Pebble Time Round, board `spalding` (Chalk platform, 180 × 180 round
display), is electrically the same watch and shares this board's files; it
has its own section at the end of this page.

## Hardware

- **SoC**: ST STM32F439, an Arm Cortex-M4 with FPU, 1 MiB flash, 192 KiB
  SRAM and 64 KiB core-coupled RAM (CCM), run at 96 MHz from the 16 MHz HSI
- **Flash**: Spansion S29VS, 16 MiB parallel NOR on the FMC at `0x60000000`
- **Display**: 144 × 168 color, behind a Lattice iCE40LP FPGA framebuffer
  that the firmware programs over SPI
- **Backlight**: PWM driven
- **Bluetooth**: TI CC2564B (Bluetooth 4.1) controller over HCI UART; the
  firmware runs the NimBLE host, LE only
- **Accelerometer**: Bosch BMI160
- **Magnetometer**: NXP MAG3110
- **Ambient light**: phototransistor on an ADC input
- **Vibration**: motor on an H-bridge
- **Microphone**: PDM, read through the STM32 I2S interface
- **Power**: Maxim MAX14690 PMIC and battery charger

### Revisions

| Revision | Description |
| --- | --- |
| `dvt` | Pebble Time, as named by the stock firmware bundles |
| `s3` | Pebble Time Steel: its own battery capacity and low-power level |

### Devices

From `boards/snowy/snowy.dtsi`, `snowy.c`, `snowy.h` and the BMI160's SPI glue.

| Device | Bus and pins | Driver | Role |
| --- | --- | --- | --- |
| Debug console | USART3, TX PC10, RX PC11, 115200 baud | `uart/stm32` | `console_uart` |
| Emulator link | USART2, 230400 baud, emulator builds only | `uart/stm32` | `qemu_uart` |
| CC2564B | USART1, TX PA9, RX PA10, CTS PA11, RTS PA12; nSHUTDOWN PB12; 32 kHz clock on MCO1 (PA8) | `bluetooth/cc2564` | `bt_hci_uart` |
| iCE40LP display | SPI6, SCK PG13, MOSI PG14, CS PG8, CRESET PG15, CDONE PG9, BUSY PG10; rails PF3, PF2; TX DMA2 stream 5 channel 1; 16/32 MHz | `display/ice40lp` | `display` |
| Backlight | TIM12 CH1 on PB14, 256 Hz | `backlight/pwm` | `backlight` |
| S29VS | FMC bank 1 | `flash/s29vs` | `flash` |
| BMI160 | SPI1, SCK PA5, MISO PA6, MOSI PA7, CS PA4; INT PG5, PG6 | `imu/bmi160` | `accel` |
| MAG3110 | I2C1 (SCL PB6, SDA PB9, 400 kHz) at `0x0e`; data ready PF14 | `imu/mag3110` | `mag` |
| MAX14690 | I2C1 at `0x28`; interrupt PG7; MON on ADC channel 1 (PA1) | `pmic/max14690` | |
| Ambient light | ADC1 channel 2 (PA2), enabled by PA3 | `als/stm32_adc` | `als` |
| Vibe | TIM10 CH1 on PB8, 22 kHz; enable PF4 | `vibe/pwm` | `vibe` |
| Microphone | SPI2 as I2S, CK PB10, SD PB15; RX DMA1 stream 3 channel 0; powered from PMIC LDO3 | `audio/stm32_i2s_pdm` | `mic` |
| Buttons | Back PG4, Up PG3, Select PG1, Down PG2, active low | `input/gpio_keys`, from the tree | |
| Die temperature | internal sensor | `temp/stm32` | `die_temp` |
| RTC | internal, 32.768 kHz LSE in bypass mode | `rtc/stm32`, `timer/stm32_rtc` | `rtc` |

## Flash layout

The stock bootloader occupies the first 16 KiB of internal flash; the
firmware is linked at `0x08004000` (`CONFIG_FLASH_OFFSET`), 1008 KiB long
(`CONFIG_FW_FLASH_SIZE`), and the bootloader copies staged updates there.
The external flash partitions are in `boards/snowy/snowy.dtsi`; its first
128 KiB has 32 KiB boot sectors, the rest 128 KiB sectors.

| Partition | Range | Owner |
| --- | --- | --- |
| `debug-db` (flash logs) | `0x000000-0x020000` | this firmware |
| `shared-prf-storage` | `0x020000-0x040000` | this firmware |
| `tzinfo` | `0x040000-0x060000` | this firmware |
| (unused) | `0x060000-0x0c0000` | |
| `stock-shared-prf-storage` | `0x0c0000-0x0e0000` | **stock**, read only |
| `mfg-info` (factory data, boot FPGA bitstream at `0x0f0000`) | `0x0e0000-0x100000` | **stock**, read only |
| `firmware-slot-1` (staged update) | `0x100000-0x200000` | shared with the stock bootloader |
| `safe-firmware` (stock recovery firmware) | `0x200000-0x300000` | **stock**, read only, locked in the chip |
| `system-resources-bank-0` | `0x300000-0x380000` | this firmware; the stock recovery can install into it |
| `system-resources-bank-1` | `0x380000-0x400000` | this firmware; the stock recovery can install into it |
| `filesystem` | `0x400000-0x1000000` | this firmware |

`snowy.c` asserts at build time that the stock offsets have not moved.
The flash guard (`CONFIG_FLASH_LAYOUT_SNOWY`) asserts instead of writing the
read-only partitions, and the S29VS driver locks the recovery firmware's
range in the chip at every boot.

## Shared with the stock firmware

The stock bootloader, the stock recovery firmware (PRF) and any stock
firmware installed again read state that outlives this firmware. Each item,
and the partition table above, keeps the stock layout and meaning, except
where [](#differences-from-the-stock-layout) says otherwise; changing one
needs every stock reader checked first.

- **RTC configuration.** Prescalers 127/255 (256 sub-second steps),
  24-hour format, shadow registers in use, LSE in bypass mode, UTC calendar
  with a two-digit year; `drivers/timer/stm32_rtc.c` restores it at boot.
  The recovery firmware derives milliseconds from the sub-second counter and
  asserts when time goes backwards across STOP mode: with another set-up it
  crash-loops on its watchdog. An earlier build of this port (31/1023,
  shadow registers bypassed) left a watch without a usable recovery that way.
- **RTC backup registers** (`include/pbl/system/rtc_registers.h`): DR0 boot
  bits, DR1 stuck buttons, DR2 bootloader version, DR4 factory-reset wipe
  marker (`CONFIG_LEGACY_FACTORY_RESET_KEEPS_FIRMWARE`; the stock firmware
  for this watch does not use DR4), DR5-DR10 reboot reason, DR11 erase in
  progress, DR12-DR16 time zone, DR17-DR18 compass calibration, DR19 last
  app slot. DR3 is not read or written.
- **Reboot reason** (`pebble/system/reboot_reason.c`,
  `CONFIG_LEGACY_REBOOT_REASON_STOCK_LAYOUT`): DR5 code and flags, DR6 the
  extra word, DR7-DR9 a stuck task's PC, LR and callback (a hard lockup's
  PC, LR and exception); for an event queue overflow DR7 task 0 and DR8-DR10
  the LR, the current and the dropped event. A stack overflow of a task
  outside 0-7 stores 3 in the task byte and the task in the second data
  byte. The recovery firmware fails an assert at every start, before it
  clears the reason, when the task word of an event queue reason or the
  task byte of a stack overflow is outside 0-7 and 9; the stock firmware
  accepts 0-8 and 10.
- **Boot bits, reboot reason, stuck buttons** (`pebble/system/bootbits.c`,
  `reboot_reason.c`): the bootloader picks the firmware or the recovery by
  the boot bits, and counts boots with each button held in DR1, which
  `soc/stm32/stm32f4/stuck_buttons.c` clears per button on release.
- **S29VS erase in progress.** DR11 in the stock layout (bit 31 in
  progress, bit 30 boot sector, address); `drivers/flash/s29vs.c` finishes
  an interrupted erase at the next boot.
- **PMIC settings.** `drivers/pmic/max14690.c` writes the stock values,
  charger included (4.30 V, auto stop and restart).
- **Firmware checksum.** Staged firmware and the stock recovery image use
  the legacy STM32 CRC (`CONFIG_FIRMWARE_LEGACY_CRC`).

(differences-from-the-stock-layout)=
### Differences from the stock layout

The stock firmware and the recovery firmware see these differences when
they start after this firmware, and this firmware sees them when it starts
after one of them. They come from reading both sources; none was run on a
watch.

| State | Stock firmware and recovery | This firmware | Effect |
| --- | --- | --- | --- |
| Reboot reason | Stock firmware: six words, DR5-DR10. Recovery: seven, DR5-DR11, with a stuck mutex's LR and PC in DR10 and DR11, printed for a watchdog reason when DR10 is not zero | Six words, DR5-DR10, in the stock order; DR10 stays zero for a watchdog reason | A build without `CONFIG_LEGACY_REBOOT_REASON_STOCK_LAYOUT` keeps four words, DR5-DR8, and reads the detail of a watchdog, event queue or hard lockup reason written in the other order one word off |
| Reboot reason codes 26-29 | Dialog boot fault, Bluetooth core dump, core dump, core dump entry failed | Bluetooth core dump, core dump, core dump entry failed, hard lockup | The side that starts next names these four reasons wrongly |
| RTC alarm A | Not used | The clock event without `CONFIG_TIMER_STM32_RTC_WUT`; it stays enabled in the backup domain across a reset | None known |
| Calendar year | 2000-2037; a year outside is clamped and the calendar rewritten | 2000-2099 | None before 2038 |
| Calendar weekday | Zero | 1-7 | None known; the stock time conversion ignores it |
| Seconds read from the calendar | Truncated | Rounded to the nearest second | Up to one second apart |
| Filesystem page headers | 8 KiB pages, version 0x5001 | 8 KiB pages; version 0x5002 in every header written to erased flash (`CONFIG_FS_PFS_VERSION_2`), 0x5001 read as well. A filesystem with one page per 128 KiB sector, as earlier builds of this port wrote, is converted at the first boot (`CONFIG_FS_PFS_CONVERT_SECTOR_PAGES`), which takes minutes | The stock firmware, and a build of this port with sector-sized pages, format the filesystem when they start after this firmware |

After three failed starts before the firmware marks itself stable, when
the firmware asks, or with Back, Select and Up held for 5 s, the bootloader
switches to the recovery firmware and **erases the normal firmware**; a new
one must then be installed from a PC over Bluetooth Classic.

```{warning}
Never build the `prf` variant for this board, and never write the
bootloader, the recovery firmware or the factory data. Put-bytes refuses
a recovery image.
```

## Board description

Under `boards/snowy/` unless named otherwise:

- `defconfig` (and `defconfig.dvt`, `defconfig.s3`): the SoC
  (`CONFIG_SOC_STM32F439`, `soc/stm32`), the platform, the bus classes and
  the hardware drivers.
- `Kconfig`, `Kconfig.dvt`, `Kconfig.s3`: board and revision symbols.
  `BOARD_SNOWY` selects `BOARD_PARTITIONS_SNOWY` and `LEGACY_WATCH`, which
  opens the options of `Kconfig.legacy` that change shared code.
- `Kconfig.defconfig`: defaults set by the hardware or the stock firmware,
  e.g. `FW_FLASH_SIZE`, `QEMU_MACHINE`, the CC2564, the legacy CRC, the
  battery model.
- `pebble/boards/snowy.conf`: firmware settings, e.g. the watchdog,
  memory-mapped system resources, the pairing mode.
- `roles.yml`: each role names a C object with `symbol:` and the header
  that declares it (`snowy_devices.h`), not a devicetree node.
- `snowy.dts`, `snowy.dtsi`: the SoC tree (`dts/st/stm32f439.dtsi`), the
  buttons, and the NOR flash with its fixed partitions, from which the
  build generates the `PART_*` constants (`CONFIG_BOARD_PARTITIONS_DT`).

The tree follows the Linux bindings of the STM32F4. The RCC clock
controller, the GPIO ports with their pin interrupts and the buttons come
from it, and their drivers use the registers directly. The UARTs, I2C, SPI,
DMA and timers are still defined in C with the drivers' macros, on the ST
Standard Peripheral Library (`third_party/hal_stm32`), and the display,
Bluetooth and accelerometer take their pins from `snowy.h`. Moving them
into the devicetree is planned work.

### Clock event

The clock source is the RTC calendar plus its 256 Hz sub-second counter
(`CONFIG_TIMER_STM32_RTC`). RTC alarm A raises the clock events, at least
three steps (11.7 ms) ahead (`CONFIG_TIMER_STM32_RTC_ALARM`, the default).
The wakeup timer (`CONFIG_TIMER_STM32_RTC_WUT`) is off: the kernel re-arms
it on most ticks with interrupts locked for about 60 µs, and with it the
CC2564 lost init script reply bytes when Bluetooth restarted on the watch.

## Building

The tree's CI builds in `ghcr.io/coredevices/pebbleos-docker:v13`; the ST
library is the submodule `third_party/hal_stm32/stm32-sdk`.

Watch image, Pebble Time (`snowy@s3` for the Steel):

```shell
pbl configure --board snowy@dvt -DCONFIG_RELEASE=y
pbl build
pbl bundle
```

`CONFIG_SOC_STM32F4_EMULATED` builds for QEMU's `pebble-snowy-bb` machine
(`pebble-s4-bb` for spalding), working around what it does not model (S29VS
status register, RTC alarm sub-seconds, RTC initialization mode). Clock
events then come from SysTick, or from the wakeup timer with
`CONFIG_TIMER_STM32_RTC_WUT=y`, and the emulator link on USART2 is added.
Never set it for a watch. Emulator image and integration tests:

```shell
pbl configure --board snowy@dvt -DCONFIG_SOC_STM32F4_EMULATED=y
pbl build
PEBBLE_QEMU_BIN=/path/to/qemu-system-arm pbl itest system ui notifications \
    --bt-hci <controller>
```

The harness boots the build behind the stock bootloader, taken from git
history (`tests/integration/harness/device/legacy_images.py`): a shallow
clone cannot run it. QEMU needs the `pebble-snowy-bb` machine and a fourth
serial port on USART1. The CC2564 driver expects a controller that answers
TI's init script: the runs recorded below called the harness through
pytest with a controller stand-in that is not in this tree.

### Memory

Release build of `snowy@dvt` at the tip of the port:

| Region | Used | Share |
| --- | --- | --- |
| Flash (firmware) | 934 891 B | 90.57 % |
| Kernel SRAM | 76 032 B | 88.39 % |
| CCM | 30 288 B | 46.22 % |
| System resources | 519 017 of 524 288 B | 98.99 % |

CCM holds the thread stacks, the HCI receive ring, the NimBLE host's data,
the PDM filter tables (`CONFIG_AUDIO_STM32_I2S_PDM_TABLES_CCM`) and the
kernel heap (the remaining 35 248 B). The NimBLE pools
(`CONFIG_NIMBLE_POOLS_SRAM`), the interrupt stack and all DMA buffers are in
SRAM: DMA cannot reach CCM. The emulator build uses 969 011 B of flash and
81 152 B of kernel SRAM.

## Emoji

The system resources fill their flash region (98.99 %), so the firmware
carries the color emoji fonts only as a subset of 136 emoji. With
`CONFIG_LEGACY_EMOJI_PACK` (on by default for this board) the watch draws
every other emoji from a file named `emoji` on its filesystem: a resource
pack with the four full emoji fonts (about 360 KB). Without the file the
watch draws the subset, as before; an emoji outside it shows as a box.

Every bundle is built with its pack (`emoji_<board>.pbpack` in the build
directory, made by `tools/emoji_pack.py`) and carries it: the bundle's
manifest lists it under `files`, with the name it takes on the watch. A
phone app that knows the entry installs the file after the firmware update
(PutBytes, object type file); one that does not ignores it and the update
works as usual. The file survives firmware updates and is removed by a
factory reset. From a PC it is installed over the developer connection
with `python -m plt.emoji_pack` from the port's tools.

## Status

Checked on one retail Pebble Time (`snowy@dvt`) with this tree on Core
v4.47.0, November 2026. The Time Steel (`snowy@s3`) and the Time Round
(`spalding`) build and have never run on a watch. "Emulator" means the
`system`, `ui` and `notifications` integration tests: 14 passed with each
clock event variant of the emulated SoC.

| Feature | On the watch | Emulator |
| --- | --- | --- |
| Display | Runs; frame timing measured | Yes |
| Buttons | Run; debounce set from the code for about 40 ms from press to event, not measured | Yes |
| Backlight | Runs, capped at 80 % of full scale; PWM levels not checked one by one | No |
| Vibe | Runs; patterns not checked one by one | No |
| Accelerometer | No recorded check | No |
| Compass | Calibrates and follows the heading | No |
| Ambient light | Lux table measured on a Pebble Time (v4.46 port) | No |
| Battery and charging | Reading and charging work; charge cycle and battery life not measured | No |
| Microphone and dictation | Dictation works at room temperature | No |
| Bluetooth LE | Pairs with a passkey, reconnects, restarts after being switched off and on | With a controller stand-in |
| Firmware update | Side-loaded over Bluetooth LE, installed by the stock bootloader | v4.47 to v4.47, v4.46 to v4.47 and back |
| Recovery | Firmware to stock recovery and back from a PC, three times (v4.46 port) | No |
| Deep sleep (STOP) | 94 to 96 % of an idle hour in STOP; current not measured | No |
| Certification screen | Shows the stock regulatory IDs and the ratings from the FCC filing | No |

## Open issues

- **Wakeup timer as the clock event.** With
  `CONFIG_TIMER_STM32_RTC_WUT=y` the CC2564 often fails to start after
  Bluetooth is switched off and on (reply bytes lost to UART overruns, one
  byte per interrupt), and Settings > Bluetooth then stays on "Enabling".
  The default, alarm A, has not shown this.
- **Clock source reading back.** On the watch, a calendar read one step
  behind the previous one made the kernel add 2^32 ticks (a jump of 48
  days). The driver now never returns a smaller value and logs the raw
  registers when it happens; why the read goes back is not known, and
  there is no unit test for it.
- **UI frame time.** An app frame takes about 34 ms to draw against a
  33 ms target, plus about 24 ms to send to the display. Of the draw time,
  about 8 % is the 64-bit divisions of `integer_sqrt` (round line caps)
  and 8 to 15 % heap allocation in the launcher's glances; both are the
  same code as on Core's boards.
- **spalding.** Its round display path (`DISPLAY_ICE40LP_ROUND`: rows of
  the packed round framebuffer, shifted by the factory display offsets)
  builds and links, but has never run.
- **Board description.** Only the flash partitions are in the devicetree;
  the devices are still described by the board's C tables. The tree is not
  validated against dtschema, and the root compatible `pebble,snowy` may
  need its vendor prefix registered.
- **Board values.** Some `Kconfig.defconfig` defaults are firmware options
  that may belong in `snowy.conf`.

## spalding

`boards/spalding` (no revisions, `pbl configure --board spalding`)
includes `snowy/snowy.c`, `snowy.h` and `snowy.dtsi`, where
`CONFIG_BOARD_SPALDING` picks the round framebuffer mask, the BMI160 and
MAG3110 axes, a push-pull display rail and the spalding FPGA bitstream. Its
`Kconfig.defconfig` sets the stock battery model (44 h, low power at 7 %,
warnings at 34 % and 20 %) and `APP_GLANCE_ON_DEMAND`. A release build
uses 939 979 B of flash (91.07 %), 70 400 B of kernel SRAM (81.85 %),
37 456 B of CCM (57.15 %) and 522 697 B of resources (99.70 %).

## Licensing

- **STM32 Standard Peripheral Library and OpenPDMFilter**
  (`third_party/hal_stm32/stm32-sdk`): `LicenseRef-ST-SLA0044 AND
  Apache-2.0` in `third_party/sbom.yml`.
- **TI service pack** (`third_party/ti_bt_sp/TIInit_6.7.16.bts`): the
  CC256xB init script, unmodified, under the Texas Instruments Text File
  License (`third_party/ti_bt_sp/LICENSE`): binary redistribution without
  modification, with TI devices only.
- **iCE40 bitstreams** (`drivers/display/ice40lp/*_framebuffer.fpga`):
  binaries embedded in the firmware, byte for byte the files
  `platform/snowy/Snowy_LP1K_framebuffer.fpga` and
  `platform/snowy/Spalding_UL1K_framebuffer.fpga` of the PebbleOS source
  release this repository's history starts from.
