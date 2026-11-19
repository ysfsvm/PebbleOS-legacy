# Task watchdog

`subsys/task_wdt/` is the software watchdog that catches stuck threads and
turns them into core dumps, enabled by `CONFIG_TASK_WDT`. Its public API is
`include/pbl/task_wdt/task_wdt.h`.
It is a fixed pool of channels, each with its own timeout and optional expiry
callback, backing a single hardware watchdog (`include/pbl/drivers/watchdog.h`).

## Model

- A **channel** watches one thread. `pbl_task_wdt_add()` binds it to the calling
  thread (or an explicit one) with a timeout (the system threads use
  `CONFIG_TASK_WDT_TIMEOUT_MS`); the thread, or something acting
  on its behalf, must call `pbl_task_wdt_feed()`, `pbl_task_wdt_feed_self()` or
  `pbl_task_wdt_feed_thread()` before the timeout runs out. `pbl_task_wdt_feed_all()`
  exists for long, lock-holding operations such as flash erases.
- A dedicated **watchdog thread** at the highest priority wakes every
  `CONFIG_TASK_WDT_CHECK_PERIOD_MS`, compares every channel against the
  kernel uptime, and feeds the hardware watchdog when it decides not to
  reset. Its sleep also bounds how long the SoC may stay in deep sleep, so
  the hardware watchdog is kept alive without SoC-specific timers. A thread
  spinning with interrupts disabled, or an interrupt handler that never
  returns, starves the watchdog thread too; the hardware watchdog is the
  backstop for those, and the hard lockup detector below turns most of them
  into core dumps.
- When a channel **expires**, the watchdog thread logs the stuck thread's
  name, saved PC and LR, and calls the channel callback. The callback may
  try to unblock the thread and returns a pointer naming the work it was
  doing. It runs again on every check while the channel stays expired. The
  stall is then reported with `pbl_task_wdt_stalled()`, which the firmware
  overrides to store it in the reboot reason (`RebootReasonCode_Watchdog`,
  with the fed and active channel masks in `data8`, and the work in
  `stuck_task_callback`).
- After `CONFIG_TASK_WDT_GRACE_MS` of a channel staying expired, counted
  from the check that first found it expired, `pbl_task_wdt_expired()`
  resets the system: it panics by default, and the firmware overrides it
  with `reset_due_to_software_failure()`, so a core dump is written. That
  check never resets, so the callback always gets the grace period to
  recover the thread, and the next check runs once the grace period is over
  instead of a full check period later. Without `CONFIG_WATCHDOG` it logs
  instead and keeps running. A channel fed again within the grace period is
  reported with `pbl_task_wdt_recovered()`, with which the firmware clears
  the reboot reason, and logs the recovery.
- A thread waiting for work (`pbl_task_wdt_set_waiting()`) does not expire
  while it is blocked. Once woken it must run within its timeout, counted
  from the check that first finds it runnable, so a waiting thread starved
  by a busier, higher-priority one still expires.
- `pbl_task_wdt_add_workq()` watches a work queue's thread: it waits while the
  queue is idle and its channel is fed after each item, so each item gets the
  whole timeout.
- `pbl_task_wdt_suspend()` keeps every channel fed for a bounded time (or until
  `pbl_task_wdt_resume()`), for phases such as boot or a long settings-file
  compaction where stalls are expected.

## Hard lockup detector

With `CONFIG_TASK_WDT_HARDLOCKUP`, a timer of the architecture that the
kernel's interrupt lock does not mask counts the time the CPU runs, outside
idle waits and deep sleep, since the watchdog thread last checked its
channels. The thread starts the detector and feeds it with
`pbl_task_wdt_hardlockup_feed()` on every check, also when it does not feed
the hardware watchdog (for a debugger, or a driver that is not up), and
every hardware watchdog driver reports its own feeds, so the feeds of long
flash erases count too.

Past `CONFIG_TASK_WDT_HARDLOCKUP_TIMEOUT_MS` (never sooner, at most an
eighth later), the detector calls `pbl_task_wdt_hardlockup()` with the
interrupted code's PC, LR and exception number. The timeout lies between
the check period and `CONFIG_TASK_WDT_HARDLOCKUP_WATCHDOG_MIN_MS`, the
shortest time the hardware watchdog may take: on the STM32F4, 5.5 s for a
nominal 8.2 s, as the IWDG runs from the LSI (17 to 47 kHz). The default
report writes a core dump. The firmware overrides it to store the lockup in
the reboot reason (`RebootReasonCode_HardLockup`), which the next boot
logs, unless an earlier reason other than a task watchdog stall is stored
(an assert that then hung), and to set `BOOT_BIT_SOFTWARE_FAILURE_OCCURRED`,
as its other crash paths do: the bootloader counts the reset as a failed
boot.

It finds an interrupt handler that never returns or that is taken again
and again, and a thread spinning with the kernel's interrupts locked. It
does not see code spinning with every interrupt masked, nor a system that
idles or sleeps and never wakes up: its timer stops there, and the hardware
watchdog still resets those without a dump.

A dump is also not certain for a lockup that starts after the CPU idled or
slept since the last check. The hardware watchdog counts that time and the
detector does not, so the report comes up to a check period later, counted
from the last feed: check period plus timeout plus an eighth, 8.06 s with
the STM32F4 defaults. That is before the IWDG's nominal 8.2 s, but not
before its shortest time, and an IWDG on a faster LSI resets first.

An architecture provides the timer by selecting
`ARCH_HAS_HARDLOCKUP_DETECTOR` and implementing `pbl/task_wdt/arch.h`. On
Cortex-M it is SysTick at the most urgent priority, so the detector is not
available where SysTick is the system timer (the QEMU boards). SysTick
counts CPU cycles and is capped at 2^24 of them, so its period is a
sixteenth of the timeout or less: it interrupts the CPU that often while it
runs. It is stopped around the idle wait, which it would otherwise end
every period.

## Users

- `pebble/main.c` starts the watchdog, suspends it for the first 30 s of
  boot, watches the high priority system work queue with
  `pbl_task_wdt_add_workq()`, and adds the KernelMain channel, which the
  launcher event loop feeds.
- `pebble/process_management/app_throttle.c` watches the background system work
  queue. Its callback throttles the app thread to the idle priority for a
  short time when that queue is ready to run but starved.
- `subsys/bluetooth/init.c` owns the NimBLE host channel, waiting while the
  host has no events to run.
- Flash, filesystem and console code feeds the calling thread's channel from
  loops that legitimately run for seconds.

## Tests

`tests/subsys/task_wdt/` runs the subsystem on the POSIX kernel port with
virtual time; see {doc}`../development/tests/unit` for how to run it.
`tests/subsys/task_wdt_hardlockup/` runs the hard lockup detector's counting
against a stand-in for the architecture's timer.

The QEMU boards carry a real hardware watchdog (the CMSDK APB watchdog,
driven by `drivers/watchdog/qemu.c`), so both paths can be tried in
the emulator: `wdt stall main` ends in a core dump, and `wdt stall irq`
ends in a hardware reset that the next boot reports as a watchdog reset.
