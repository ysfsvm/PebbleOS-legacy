/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Spansion S29VS on the STM32F439 FMC (snowy).
//!
//! The chip can lock one contiguous sector range until it is reset ("Sector
//! Lock Range"). PRF is locked at init and again after every unprotect or
//! write_protect request, so it is never left writable: a hardware backstop
//! for the flash guard, which also covers stock shared PRF storage and MFG
//! info.

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#include <pbl/drivers/flash/flash_guard.h>
#include <pbl/drivers/flash/s29vs.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/sched.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/cmsis.h>
#include <pbl/system/status_codes.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <hardware/stm32f4_fmc.h>
#include <hardware/stm32f4_gpio.h>
#include <hardware/stm32f4_rcc.h>

#define MHZ_TO_HZ(mhz) ((mhz) * 1000000U)

typedef uint32_t FlashAddress;

//! The first 128 KiB has 32 KiB sectors, the rest 128 KiB ones.
#define BOTTOM_BOOT_REGION_END  0x20000U
#define BOTTOM_BOOT_SECTOR_SIZE 0x8000U
#define S29VS_SECTOR_SIZE       0x20000U
#define S29VS_SIZE              0x1000000U

//! This is the memory mapped region that's mapped to the parallel flash.
static const uintptr_t FMC_BANK_1_BASE_ADDRESS = 0x60000000;

//! FMC control registers
#define FMC_REG_BASE 0xA0000000U

//! This is the unit that we use for writing
static const uint32_t S29VS_PAGE_SIZE = 64;

//! Different commands we can send to the flash
typedef enum S29VSCommand {
  S29VSCommand_WriteBufferLoad = 0x25,
  S29VSCommand_BufferToFlash = 0x29,
  S29VSCommand_EraseResume = 0x30,
  S29VSCommand_SectorBlank = 0x33,
  S29VSCommand_SectorLock = 0x60,
  S29VSCommand_SectorLockRangeArg = 0x61,
  S29VSCommand_ReadStatusRegister = 0x70,
  S29VSCommand_ClearStatusRegister = 0x71,
  S29VSCommand_EraseSetup = 0x80,
  S29VSCommand_DeviceIDEntry = 0x90,
  S29VSCommand_EraseSuspend = 0xB0,
  S29VSCommand_ConfigureRegisterEntry = 0xD0,
  S29VSCommand_SoftwareReset = 0xF0
} S29VSCommand;

//! Arguments to the S29VSCommand_EraseSetup command
typedef enum S29VSCommandEraseAguments {
  S29VSCommandEraseAguments_ChipErase = 0x10,
  S29VSCommandEraseAguments_SectorErase = 0x30
} S29VSCommandEraseAguments;

//! The bitset stored in the status register, see prv_read_status_register
typedef enum S29VSStatusBit {
  S29VSStatusBit_BankStatus = (1 << 0),
  S29VSStatusBit_SectorLockStatus = (1 << 1),
  S29VSStatusBit_ProgramSuspended = (1 << 2),
  // Bit 3 is reserved
  S29VSStatusBit_ProgramStatus = (1 << 4),
  S29VSStatusBit_EraseStatus = (1 << 5),
  S29VSStatusBit_EraseSuspended = (1 << 6),
  S29VSStatusBit_DeviceReady = (1 << 7),
} S29VSStatusBit;
[[maybe_unused]] static const uint16_t SPANSION_MANUFACTURER_ID = 0x01;
[[maybe_unused]] static const uint16_t MACRONIX_MANUFACTURER_ID = 0xc2;
#define S29VS_GPIO_PORT(n) (0x40020000U + (n) * STM32F4_GPIO_PORT_SIZE)
#define S29VS_PORT_B       1U
#define S29VS_PORT_D       3U
#define S29VS_PORT_E       4U
#define S29VS_GPIOB        S29VS_GPIO_PORT(S29VS_PORT_B)
#define S29VS_GPIOD        S29VS_GPIO_PORT(S29VS_PORT_D)
#define S29VS_GPIOE        S29VS_GPIO_PORT(S29VS_PORT_E)

#define S29VS_PIN_RESET 2U
#define S29VS_AF_FMC    12U
#define S29VS_PINS_ALL  0xFFFFU

#define S29VS_SPEED_2MHZ   0U
#define S29VS_SPEED_100MHZ 3U

//! Push-pull, no pull
static void prv_gpio_init(uintptr_t port, uint32_t pins, uint32_t mode, uint32_t speed) {
  for (unsigned int pin = 0U; pin < 16U; pin++) {
    if ((pins & PBL_BIT(pin)) == 0U) {
      continue;
    }
    const uint32_t field = STM32F4_GPIO_FIELD2(pin);
    pbl_modifyl(field, PBL_FIELD_PREP(field, mode), port + STM32F4_GPIO_MODER_OFFSET);
    if (mode == STM32F4_GPIO_MODE_OUTPUT || mode == STM32F4_GPIO_MODE_AF) {
      pbl_modifyl(field, PBL_FIELD_PREP(field, speed), port + STM32F4_GPIO_OSPEEDR_OFFSET);
      pbl_modifyl(PBL_BIT(pin), 0U, port + STM32F4_GPIO_OTYPER_OFFSET);
    }
    pbl_modifyl(field, 0U, port + STM32F4_GPIO_PUPDR_OFFSET);
  }
}

static void prv_gpio_set_af(uintptr_t port, unsigned int pin, uint32_t af) {
  const uint32_t field = STM32F4_GPIO_FIELD4(pin);
  pbl_modifyl(field, PBL_FIELD_PREP(field, af), port + STM32F4_GPIO_AFR_OFFSET(pin));
}

static void prv_reset_line(bool high) {
  pbl_writel(
      high ? STM32F4_GPIO_BSRR_SET(S29VS_PIN_RESET) : STM32F4_GPIO_BSRR_RESET(S29VS_PIN_RESET),
      S29VS_GPIOD + STM32F4_GPIO_BSRR_OFFSET);
}

static void prv_issue_command_argument(FlashAddress sector_address, uint16_t cmd_arg);
static status_t flash_impl_erase_sector_begin(FlashAddress sector_addr);
static status_t flash_impl_set_burst_mode(bool burst_mode);
static void prv_issue_command(FlashAddress sector_address, S29VSCommand cmd);

// puts gpios into or out of analog to save power when idle/in use respectively
static void prv_flash_idle_gpios(bool enable_gpios) {
  static bool gpios_idled = false;
  if (gpios_idled == enable_gpios) {
    return;
  }
  gpios_idled = enable_gpios;

  const uint32_t mode = enable_gpios ? STM32F4_GPIO_MODE_AF : STM32F4_GPIO_MODE_ANALOG;
  const uint32_t speed = enable_gpios ? S29VS_SPEED_100MHZ : S29VS_SPEED_2MHZ;

  // leave RESET_N and CE: they need to retain their state
  // Configure the rest as analog inputs to save as much power as possible
  // D2 - Reset - GPIO Reset line
  // D7 - FMC CE - FMC Chip Enable
  prv_gpio_init(S29VS_GPIOB, PBL_BIT(7), mode, speed);
  prv_gpio_init(S29VS_GPIOD, S29VS_PINS_ALL & ~PBL_BIT(2) & ~PBL_BIT(7), mode, speed);
  prv_gpio_init(S29VS_GPIOE, S29VS_PINS_ALL & ~PBL_BIT(0) & ~PBL_BIT(1), mode, speed);
}

static uint32_t s_num_flash_uses = 0;

// Mapped-resource users take and release the flash from any task, outside the flash mutex: the
// count and the clock it gates change together, with interrupts masked.
void s29vs_use(void) {
  pbl_irq_lock();
  if (s_num_flash_uses == 0) {
    soc_stm32_clock_set(SOC_STM32_AHB3, STM32F4_RCC_AHB3ENR_FMCEN, true);
    prv_flash_idle_gpios(true);
  }
  s_num_flash_uses++;
  pbl_irq_unlock();
}

void s29vs_release_many(uint32_t num_locks) {
  pbl_irq_lock();
  PBL_ASSERTN(s_num_flash_uses >= num_locks);
  s_num_flash_uses -= num_locks;
  if (s_num_flash_uses == 0) {
    soc_stm32_clock_set(SOC_STM32_AHB3, STM32F4_RCC_AHB3ENR_FMCEN, false);
  }
  pbl_irq_unlock();
}

static void flash_impl_release(void) {
  s29vs_release_many(1);
}

static uint16_t flash_s29vs_read_short(FlashAddress addr) {
  return pbl_readw(FMC_BANK_1_BASE_ADDRESS + addr);
}

static FlashAddress flash_impl_get_sector_base_address(FlashAddress addr) {
  if (addr < BOTTOM_BOOT_REGION_END) {
    return addr & ~(BOTTOM_BOOT_SECTOR_SIZE - 1);
  }

  return addr & ~(S29VS_SECTOR_SIZE - 1);
}

static uint8_t prv_read_status_register(FlashAddress sector_base_addr) {
#ifdef CONFIG_SOC_STM32F4_EMULATED
  // The emulator's CFI flash has no status register and takes the S29VS command for another: read
  // twice instead. While it programs or erases (for a while after the command, dropping writes
  // meanwhile) it answers with AMD status whose DQ6 toggles on every read; otherwise with the
  // array. Busy if it toggles, else ready without errors.
  const uint8_t status = flash_s29vs_read_short(sector_base_addr);
  const uint8_t again = flash_s29vs_read_short(sector_base_addr);
  return ((status ^ again) & 0x40U) ? 0U : S29VSStatusBit_DeviceReady;
#else
  prv_issue_command(sector_base_addr, S29VSCommand_ReadStatusRegister);
  return flash_s29vs_read_short(sector_base_addr);
#endif
}

static uint8_t prv_poll_for_ready(FlashAddress sector_base_addr) {
  // TODO: We should probably just assert if this takes too long
  uint8_t status;
  while (((status = prv_read_status_register(sector_base_addr)) & S29VSStatusBit_DeviceReady) ==
         0) {
    pbl_busy_wait_us(10);
  }

  return (status);
}

//! Issue the second part of a two-cycle command. This is not merged with the
//! prv_issue_command as not all commands have an argument.
//!
//! @param sector_address The address of the start of the sector to write the command to.
//! @param cmd_arg The command argument to write.
static void prv_issue_command_argument(FlashAddress sector_address, uint16_t cmd_arg) {
  // The offset in the sector we write the second part of commands to. Note that this is a 16-bit
  // word aligned address as opposed to a byte address.
  static const uint32_t COMMAND_ARGUMENT_ADDRESS = 0x2AA;

  pbl_writew(cmd_arg, FMC_BANK_1_BASE_ADDRESS + sector_address + COMMAND_ARGUMENT_ADDRESS * 2U);
}

//! @param sector_address The address of the start of the sector to write the command to.
//! @param cmd The command to write.
static void prv_issue_command(FlashAddress sector_address, S29VSCommand cmd) {
  // The offset in the sector we write the first part of commands to. Note that this is a 16-bit
  // word aligned address as opposed to a byte address.
  static const uint32_t COMMAND_ADDRESS = 0x555;

  pbl_writew(cmd, FMC_BANK_1_BASE_ADDRESS + sector_address + COMMAND_ADDRESS * 2U);
}

static void prv_software_reset(void) {
  prv_issue_command(0, S29VSCommand_SoftwareReset);
}

// Note: If this command has been executed at least once, all sectors are
// locked. They then must be unlocked before and relocked after each program
// operation (i.e write or erase). The chip only allows for one sector to be
// unlocked at any given time. For sector ranges which have been protected using
// the "Sector Lock Range Command", this function will have no effect.
static void prv_allow_write_if_sector_is_not_protected(bool lock, uint32_t sector_addr) {
  prv_issue_command(0, S29VSCommand_SectorLock);
  prv_issue_command_argument(0, S29VSCommand_SectorLock);

  int lock_flag = (lock ? 0 : 1) << 7; // set A6 to 0 to lock and 1 to unlock
  pbl_writew(S29VSCommand_SectorLock, FMC_BANK_1_BASE_ADDRESS + sector_addr + lock_flag);
}

[[maybe_unused]] static uint16_t prv_read_manufacturer_id(void) {
  // Issue the DeviceIDEntry command to change to the ID-CFI Address Map. This means that reading
  // from the bank will give us ID-CFI information instead of the normal flash contents. See
  // Table 11.2 (ID/CFI Data) for all the content you can read here. Reset the state afterwards to
  // return to the default address map.

  s29vs_use();
  prv_issue_command(0, S29VSCommand_DeviceIDEntry);
  uint16_t result = flash_s29vs_read_short(0x0);
  prv_software_reset();
  flash_impl_release();
  return result;
}

static uint16_t prv_read_configuration_register(void) {
  prv_issue_command(0, S29VSCommand_ConfigureRegisterEntry);
  uint16_t result = flash_s29vs_read_short(0x0);
  prv_software_reset();
  return result;
}

static void prv_write_configuration_register(uint16_t data) {
  // See section 5.8.1 of data sheet for command sequence
  prv_issue_command(0, S29VSCommand_ConfigureRegisterEntry);

  // Cycle 1: SA+Address 555h & Data 25h
  // Cycle 2: SA+Address 2AAh & Data 00h
  // Cycle 3: SA+Address X00h & PD
  // Cycle 4: SA+ Address 555h & Data 29h
  prv_issue_command(0, S29VSCommand_WriteBufferLoad);
  prv_issue_command_argument(0, 0);
  pbl_writew(data, FMC_BANK_1_BASE_ADDRESS);
  prv_issue_command(0, S29VSCommand_BufferToFlash);

  prv_software_reset();
}

// Use the "Sector Lock Range Command" (section 8.2 of data sheet) to block
// writes or erases to the PRF image residing on the flash. The only way to undo
// this is to issue a HW reset or pull power
static void prv_flash_protect_range(uint32_t start_sector, uint32_t end_sector) {
  PBL_ASSERTN(start_sector <= end_sector);

  s29vs_use();

  prv_issue_command(0, S29VSCommand_SectorLock);
  prv_issue_command_argument(0, S29VSCommand_SectorLock);

  start_sector = flash_impl_get_sector_base_address(start_sector);
  end_sector = flash_impl_get_sector_base_address(end_sector);

  pbl_writew(S29VSCommand_SectorLockRangeArg, FMC_BANK_1_BASE_ADDRESS + start_sector);
  pbl_writew(S29VSCommand_SectorLockRangeArg, FMC_BANK_1_BASE_ADDRESS + end_sector);

  flash_impl_release();
}

static void prv_hw_init(void) {
  soc_stm32_clock_set(SOC_STM32_AHB1,
                      STM32F4_RCC_AHB1ENR_GPIOEN(S29VS_PORT_B) |
                          STM32F4_RCC_AHB1ENR_GPIOEN(S29VS_PORT_D) |
                          STM32F4_RCC_AHB1ENR_GPIOEN(S29VS_PORT_E),
                      true);

  // Configure the reset pin (D2)
  prv_gpio_init(S29VS_GPIOD, PBL_BIT(S29VS_PIN_RESET), STM32F4_GPIO_MODE_OUTPUT,
                S29VS_SPEED_100MHZ);

  prv_reset_line(true);

  // Configure pins relating to the FMC peripheral (30 pins!)

  // B7 - FMC AVD - FMC Address Valid aka Latch
  // D0-D1, D8-D15, E2-15 - FMC A, AD - FMC Address and Address/Data lines
  // D2 - Reset - GPIO Reset line
  // D3 - FMC CLK
  // D4 - FMC OE - FMC Output Enable
  // D5 - FMC WE - FMC Write Enable
  // D6 - FMC RDY - FMC Ready line
  // D7 - FMC CE - FMC Chip Enable

  prv_gpio_set_af(S29VS_GPIOB, 7, S29VS_AF_FMC);
  prv_gpio_init(S29VS_GPIOB, PBL_BIT(7), STM32F4_GPIO_MODE_AF, S29VS_SPEED_100MHZ);

  for (unsigned int pin = 0; pin < 16; ++pin) {
    if (pin == S29VS_PIN_RESET) {
      continue;
    }
    prv_gpio_set_af(S29VS_GPIOD, pin, S29VS_AF_FMC);
  }
  prv_gpio_init(S29VS_GPIOD, S29VS_PINS_ALL & ~PBL_BIT(S29VS_PIN_RESET), STM32F4_GPIO_MODE_AF,
                S29VS_SPEED_100MHZ);

  for (unsigned int pin = 2; pin < 16; ++pin) {
    prv_gpio_set_af(S29VS_GPIOE, pin, S29VS_AF_FMC);
  }
  prv_gpio_init(S29VS_GPIOE, S29VS_PINS_ALL & ~PBL_BIT(0) & ~PBL_BIT(1), STM32F4_GPIO_MODE_AF,
                S29VS_SPEED_100MHZ);

  // We have configured the pins, lets perform a full HW reset to put the chip
  // in a good state
  prv_reset_line(false);
  pbl_busy_wait_us(10); // only needs to be 50ns according to data sheet
  prv_reset_line(true);
  pbl_busy_wait_us(30); // need 200ns + 10us before CE can be pulled low

  flash_impl_set_burst_mode(false);
}

static void prv_flash_reset(void) {
  s_num_flash_uses = 0;
  s29vs_use();

  prv_hw_init();

  flash_impl_release();
}

//! Reset the chip (dropping any lock range) and lock PRF again.
static void prv_reset_and_lock_prf(void) {
  prv_flash_reset();
  prv_flash_protect_range(
      PART_SAFE_FIRMWARE_OFFSET,
      (PART_SAFE_FIRMWARE_OFFSET + PART_SAFE_FIRMWARE_SIZE) - SECTOR_SIZE_BYTES);
}

static status_t flash_impl_init(bool coredump_mode) {
  // Don't need to do anything to enable coredump mode.

  prv_reset_and_lock_prf();
  return S_SUCCESS;
}

static status_t flash_impl_get_erase_status(void) {
  s29vs_use();
  uint8_t status = prv_read_status_register(0);
  flash_impl_release();

  if ((status & S29VSStatusBit_DeviceReady) == 0)
    return E_BUSY;
  if ((status & S29VSStatusBit_EraseSuspended) != 0)
    return E_AGAIN;
  if ((status & S29VSStatusBit_EraseStatus) != 0)
    return E_ERROR;
  return S_SUCCESS;
}

[[maybe_unused]] static status_t flash_impl_erase_subsector_begin(FlashAddress subsector_addr) {
  return flash_impl_erase_sector_begin(subsector_addr);
}

static status_t flash_impl_erase_sector_begin(FlashAddress sector_addr) {
  status_t result = E_UNKNOWN;

  // FIXME: We should just assert that the address is already aligned. If
  // someone is depending on this behaviour without already knowing the range
  // that's being erased they're going to have a bad time. This will probably
  // cause some client fallout though, so tackle this later.
  sector_addr = flash_impl_get_sector_base_address(sector_addr);

  s29vs_use();
  prv_issue_command(sector_addr, S29VSCommand_ClearStatusRegister);

  // Some sanity checks
  {
    status_t error = S_SUCCESS;
    const uint8_t sr = prv_read_status_register(sector_addr);
    if ((sr & S29VSStatusBit_DeviceReady) == 0) {
      // Another operation is already in progress.
      error = E_BUSY;
    } else if (sr & S29VSStatusBit_EraseSuspended) {
      // Cannot program while another program operation is suspended.
      error = E_INVALID_OPERATION;
    }
    if (FAILED(error)) {
      result = error;
      goto done;
    }
  }

  prv_allow_write_if_sector_is_not_protected(false, sector_addr);

  prv_issue_command(sector_addr, S29VSCommand_EraseSetup);
  prv_issue_command_argument(sector_addr, S29VSCommandEraseAguments_SectorErase);
  prv_allow_write_if_sector_is_not_protected(true, sector_addr);

  // Check the status register to make sure that the erase has started.
  const uint8_t sr = prv_read_status_register(sector_addr);
  if ((sr & S29VSStatusBit_DeviceReady) == 0) {
    // Program or erase operation in progress. Is it in the current bank?
    result = ((sr & S29VSStatusBit_BankStatus) == 0) ? S_SUCCESS : E_BUSY;
  } else {
    // Operation hasn't started. Something is wrong.
    if (sr & S29VSStatusBit_SectorLockStatus) {
      // Sector is write-protected.
      result = E_INVALID_OPERATION;
    } else if (sr & S29VSStatusBit_EraseStatus) {
      // Erase failed for some reason.
      result = E_ERROR;
    } else {
      // The erase has either completed in the time between starting the erase
      // and polling the status register, or the erase was never started. The
      // former case could be due to a context switch at the worst time and
      // subsequent task starvation, or being run in QEMU. The latter could be
      // due to a software bug or hardware failure. It would be possible to tell
      // the two situations apart by performing a blank check, but that takes
      // more time than a nonblocking erase should require. Let the upper layers
      // verify that the erase succeeded if they care about it.
      result = S_SUCCESS;
    }
  }

done:
  flash_impl_release();
  return result;
}

static status_t flash_impl_erase_suspend(FlashAddress sector_addr) {
  status_t status = E_INTERNAL;
  sector_addr = flash_impl_get_sector_base_address(sector_addr);
  s29vs_use();
  const uint8_t sr = prv_read_status_register(sector_addr);
  // Is an operation in progress?
  if ((sr & S29VSStatusBit_DeviceReady) != 0) {
    // No erase in progress to suspend. Maybe the erase completed before this
    // call.
    status = S_NO_ACTION_REQUIRED;
  } else if ((sr & S29VSStatusBit_BankStatus) != 0) {
    // Operation is in a different bank than the given address.
    status = E_INVALID_ARGUMENT;
  } else {
    // All clear.
    prv_issue_command(sector_addr, S29VSCommand_EraseSuspend);
    if (prv_poll_for_ready(sector_addr) & S29VSStatusBit_EraseSuspended) {
      status = S_SUCCESS;
    } else {
      // The erase must have completed between the status register read and
      // the EraseSuspend command.
      status = S_NO_ACTION_REQUIRED;
    }
  }
  flash_impl_release();
  return status;
}

static status_t flash_impl_erase_resume(FlashAddress sector_addr) {
  status_t status = E_INTERNAL;
  sector_addr = flash_impl_get_sector_base_address(sector_addr);
  s29vs_use();
  uint8_t sr = prv_read_status_register(sector_addr);
  if ((sr & S29VSStatusBit_DeviceReady) != 0 && (sr & S29VSStatusBit_EraseSuspended) != 0) {
    prv_issue_command(sector_addr, S29VSCommand_EraseResume);
    status = S_SUCCESS;
  } else {
    // Device busy or no suspended erase to resume.
    status = E_INVALID_OPERATION;
  }
  flash_impl_release();
  return status;
}

// A chip erase would take PRF, MFG info and the boot FPGA bitstream with it.
static void prv_read_words_pio(uint16_t *buffer, uintptr_t flash_data_region, uint32_t num_words) {
  for (uint32_t words_read = 0; words_read < num_words; words_read++) {
    buffer[words_read] = pbl_readw(flash_data_region + words_read * 2U);
  }
}

// Currently this implementation reads halfwords at a time (16-bits). Burst
// length is currently 1 for synchronous reads. This can be optimized in future
// to do larger burst sizes and/or unrolling larger transfer sizes into 32-bit
// reads.
static status_t flash_impl_read_sync(void *buffer_ptr, FlashAddress start_addr,
                                     size_t buffer_size) {
  uint8_t *buffer = buffer_ptr;
  s29vs_use();

  uint32_t flash_data_addr = (FMC_BANK_1_BASE_ADDRESS + start_addr);
  bool odd_start_addr = ((start_addr % 2) == 1);
  uint32_t bytes_read = 0;

  uint16_t *buff_ptr = (uint16_t *)&buffer[bytes_read];
  if (odd_start_addr) {
    // read first byte into a temporary buffer but read from source on aligned word boundary
    uint16_t temp_buffer = pbl_readw(flash_data_addr - 1);
    buffer[bytes_read++] = (uint8_t)((temp_buffer >> 8) & 0xFF);
  }

  // At this point, flash_data_addr is now halfword aligned
  buff_ptr = (uint16_t *)&buffer[bytes_read];
  bool odd_buff_addr = ((((uintptr_t)buff_ptr) % 2) == 1);
  if (buffer_size - bytes_read >= 2) {
    // if at least one halfword to read
    if (!odd_buff_addr) {
      // Both flash_data_addr and buffer are aligned
      uint32_t num_words = (buffer_size - bytes_read) / 2;
      prv_read_words_pio(buff_ptr, flash_data_addr + bytes_read, num_words);
      bytes_read += num_words * 2;
    } else {
      // Not aligned - read into temporary buffer and copy over
      const uintptr_t flash_data_region = flash_data_addr + bytes_read;
      uint32_t num_words = (buffer_size - bytes_read) / 2;
      for (uint32_t words_read = 0; words_read < num_words; words_read++) {
        uint16_t temp_buffer = pbl_readw(flash_data_region + words_read * 2U);
        buffer[bytes_read++] = (uint8_t)(temp_buffer & 0xFF);
        buffer[bytes_read++] = (uint8_t)((temp_buffer >> 8) & 0xFF);
      }
    }
  }

  buff_ptr = (uint16_t *)&buffer[bytes_read];
  // See if there are any remaining bytes left - at this point - flash_data_addr is still halfword
  // aligned
  if (buffer_size - bytes_read == 1) {
    uint16_t temp_buffer = pbl_readw(flash_data_addr + bytes_read);
    buffer[bytes_read++] = (uint8_t)(temp_buffer & 0xFF);
  } else if (buffer_size - bytes_read != 0) {
    // Should not reach here
    PBL_LOG_DBG("Invalid data length read");
  }

  flash_impl_release();

  return S_SUCCESS;
}

static int flash_impl_write_page_begin(const void *vp_buffer, const FlashAddress start_addr,
                                       size_t len) {
  if (!len) {
    return E_INVALID_ARGUMENT;
  }
  const uint8_t *buffer = vp_buffer;
  // Flash write transactions can only write one page at a time, where each
  // page is 64 bytes in size. Split up our transactions into pages and then
  // write one page.
  const uint32_t offset_in_page = start_addr % S29VS_PAGE_SIZE;
  const uint32_t bytes_in_page = MIN(S29VS_PAGE_SIZE - offset_in_page, len);

  // We're only allowed to write whole 16-bit words during a write operation.
  // Therefore we'll need to pad out our write if it's not perfectly aligned at
  // the start or the end.
  int num_shorts = bytes_in_page / 2;

  // 4 cases
  // Perfectly aligned - No additional writes
  // Unaligned start, even length - Need to pad both ends
  // Unaligned start, odd length - Pad the start
  // Aligned start, odd length - Pad the end
  if (start_addr & 0x1 || bytes_in_page & 0x1) {
    ++num_shorts;
  }

  const FlashAddress sector_addr = flash_impl_get_sector_base_address(start_addr);

  s29vs_use();
  prv_issue_command(sector_addr, S29VSCommand_ClearStatusRegister);

  // Some sanity checks
  {
    status_t error = S_SUCCESS;
    const uint8_t sr = prv_read_status_register(sector_addr);
    if ((sr & S29VSStatusBit_DeviceReady) == 0) {
      // Another operation is already in progress.
      error = E_BUSY;
    } else if (sr & S29VSStatusBit_ProgramSuspended) {
      // Cannot program while another program operation is suspended.
      error = E_INVALID_OPERATION;
    }
    if (FAILED(error)) {
      flash_impl_release();
      return error;
    }
  }

  prv_allow_write_if_sector_is_not_protected(false, sector_addr);
  prv_issue_command(sector_addr, S29VSCommand_WriteBufferLoad);
  prv_issue_command_argument(sector_addr, num_shorts - 1);

  // We're now ready to write the words. Subsequent writes to the sector will
  // actually write the data through to the write buffer.

  uintptr_t flash_write_dest = FMC_BANK_1_BASE_ADDRESS + (start_addr & ~0x1);
  uint32_t bytes_remaining = bytes_in_page;

  // Handle leading byte
  if (start_addr & 0x1) {
    // Handle a buffer with an unaligned start. Write 0xff for the first byte
    // since flash can only flip ones to zeros, and no data will be lost.
    const uint16_t first_short_value = 0xFF | ((*buffer) << 8);
    pbl_writew(first_short_value, flash_write_dest);

    // Now for the rest of the function let's pretend this never happened.
    flash_write_dest += 2U;
    ++buffer;
    --bytes_remaining;
  }

  // Handle body words
  for (; bytes_remaining >= 2; bytes_remaining -= 2, buffer += 2) {
    uint16_t buffer_word;
    memcpy(&buffer_word, buffer, sizeof buffer_word);
    pbl_writew(buffer_word, flash_write_dest);
    flash_write_dest += 2U;
  }

  // Handle trailing byte if present. This will be present if we started out
  // aligned and we wrote an odd number of bytes or if we started out unaligned
  // and wrote an even number of bytes.
  if (bytes_remaining) {
    // We need to write only a single byte, but we're only allowed to write
    // words. If we write a single byte followed by 0xFFFF, we won't modify the
    // second byte as bits are only allowed to be written from 1 -> 0. 1s will
    // stay 1s, and 0s will stay 0s.
    const uint16_t trailing_short_value = *buffer | 0xFF00;
    pbl_writew(trailing_short_value, flash_write_dest);
  }

  // Buffer writing is complete, issue the buffer to flash command to actually
  // commit the changes to memory.
  prv_issue_command(sector_addr, S29VSCommand_BufferToFlash);

  // Check the status register to make sure that the write has started.
  status_t result = E_UNKNOWN;
  const uint8_t sr = prv_read_status_register(sector_addr);
  if ((sr & S29VSStatusBit_DeviceReady) == 0) {
    // Program or erase operation in progress. Is it in the current bank?
    result = ((sr & S29VSStatusBit_BankStatus) == 0) ? S_SUCCESS : E_BUSY;
  } else {
    // Operation hasn't started. Something is wrong.
    if (sr & S29VSStatusBit_SectorLockStatus) {
      // Sector is write-protected.
      result = E_INVALID_OPERATION;
    } else if (sr & S29VSStatusBit_ProgramStatus) {
      // Programming failed for some reason.
      result = E_ERROR;
    } else {
      // The flash never appeared to go busy and there is no error. Either the
      // flash write completed between the write command and the status register
      // read (inopportune context switch or running in QEMU), or the write
      // never started. It's possible to tell them apart by validating that the
      // data was actually written to flash, but that adds even more complexity
      // to this function. Let the upper layers verify that the write succeeded
      // if they are concerned about reliability.
      result = S_SUCCESS;
    }
  }

  prv_allow_write_if_sector_is_not_protected(true, sector_addr);
  flash_impl_release();
  return FAILED(result) ? result : (int)bytes_in_page;
}

static status_t flash_impl_get_write_status(void) {
  s29vs_use();
  const uint8_t status = prv_read_status_register(0);
  flash_impl_release();

  if ((status & S29VSStatusBit_DeviceReady) == 0)
    return E_BUSY;
  if ((status & S29VSStatusBit_ProgramSuspended) != 0)
    return E_AGAIN;
  if ((status & S29VSStatusBit_ProgramStatus) != 0)
    return E_ERROR;
  return S_SUCCESS;
}

static status_t flash_impl_enter_low_power_mode(void) {
  prv_flash_idle_gpios(false);
  return S_SUCCESS;
}

static status_t flash_impl_exit_low_power_mode(void) {
  // it's ok to access s_num_flash_uses here directly, as only caller enter_stop_mode() is called
  // only while interrupts are disabled
  prv_flash_idle_gpios(s_num_flash_uses > 0);
  return S_SUCCESS;
}

//! BCR1 and BTR1
struct fmc_nor_config {
  uint32_t bcr;
  uint32_t btr;
};

#define FMC_BCR1  (FMC_REG_BASE + STM32F4_FMC_BCR_OFFSET(0))
#define FMC_BTR1  (FMC_REG_BASE + STM32F4_FMC_BTR_OFFSET(0))
#define FMC_BWTR1 (FMC_REG_BASE + STM32F4_FMC_BWTR_OFFSET(0))

#define FMC_BCR_CONFIG_MASK                                                       \
  (STM32F4_FMC_BCR_MBKEN | STM32F4_FMC_BCR_MUXEN | STM32F4_FMC_BCR_MTYP_MASK |    \
   STM32F4_FMC_BCR_MWID_MASK | STM32F4_FMC_BCR_FACCEN | STM32F4_FMC_BCR_BURSTEN | \
   STM32F4_FMC_BCR_WAITPOL | STM32F4_FMC_BCR_WRAPMOD | STM32F4_FMC_BCR_WAITCFG |  \
   STM32F4_FMC_BCR_WREN | STM32F4_FMC_BCR_WAITEN | STM32F4_FMC_BCR_EXTMOD |       \
   STM32F4_FMC_BCR_ASYNCWAIT | STM32F4_FMC_BCR_CBURSTRW | STM32F4_FMC_BCR_CCLKEN)

// BCR bits kept on disable
#define FMC_BCR_DISABLE_KEEP (PBL_GENMASK(19, 0) & ~STM32F4_FMC_BCR_MBKEN)

static void prv_switch_flash_mode(const struct fmc_nor_config *config) {
  pbl_writel(pbl_readl(FMC_BCR1) & FMC_BCR_DISABLE_KEEP, FMC_BCR1);

  pbl_writel((pbl_readl(FMC_BCR1) & ~FMC_BCR_CONFIG_MASK) | config->bcr, FMC_BCR1);
  pbl_writel(pbl_readl(FMC_BCR1) | STM32F4_FMC_BCR_FACCEN, FMC_BCR1);
  pbl_writel(config->btr, FMC_BTR1);
  pbl_writel(STM32F4_FMC_BWTR_RESET, FMC_BWTR1);

  pbl_writel(pbl_readl(FMC_BCR1) | STM32F4_FMC_BCR_MBKEN, FMC_BCR1);
}

static uint32_t prv_fmc_btr(uint32_t addset, uint32_t addhld, uint32_t datast, uint32_t busturn,
                            uint32_t clkdiv, uint32_t datlat) {
  return PBL_FIELD_PREP(STM32F4_FMC_BTR_ADDSET_MASK, addset) |
         PBL_FIELD_PREP(STM32F4_FMC_BTR_ADDHLD_MASK, addhld) |
         PBL_FIELD_PREP(STM32F4_FMC_BTR_DATAST_MASK, datast) |
         PBL_FIELD_PREP(STM32F4_FMC_BTR_BUSTURN_MASK, busturn) |
         PBL_FIELD_PREP(STM32F4_FMC_BTR_CLKDIV_MASK, clkdiv) |
         PBL_FIELD_PREP(STM32F4_FMC_BTR_DATLAT_MASK, datlat);
}

static uint16_t prv_get_num_wait_cycles(uint32_t flash_clock_freq) {
  // wait_cycle table based on frequency (table 7.1)
  // NOTE: 27MHZ frequency skipped due to data latency being 4 smaller than the wait_cycle
  uint32_t wait_cycle[] = {40000000, 54000000, 66000000, 80000000, 95000000, 104000000, 120000000};
  // find number wait states based on table
  uint32_t wait_state;
  for (wait_state = 4; wait_state < (ARRAY_LENGTH(wait_cycle) + 4); wait_state++) {
    if (flash_clock_freq < wait_cycle[wait_state - 4]) {
      break;
    }
  }
  return wait_state;
}

static status_t flash_impl_set_burst_mode(bool burst_mode) {
  const uint32_t MAX_FREQ = MHZ_TO_HZ(108); // max frequency of the flash 108MHZ
  const uint32_t TAVDP_MIN = 60;            // min addr setup time in tenths of ns
  const uint32_t TADVO_MIN = 40;            // min addr hold time in tenths

  const uint32_t SETUP_STEP = MHZ_TO_HZ(16); // for data setup equation

  const uint16_t WAIT_STATE_MASK = 0x7800; // mask for wait state binary for sync burst

  s29vs_use();

  // get system clock tick speed
  RCC_ClocksTypeDef clocks;
  RCC_GetClocksFreq(&clocks);
  uint32_t h_clock = clocks.HCLK_Frequency;                      // frequency in hertz
  uint32_t time_per_cycle = ((uint64_t)(10000000000)) / h_clock; // period in 1/10th ns

  // time between address write and address latch (AVD high)
  // tAAVDS on datasheet, min 4 ns
  //
  // AVD low time
  // tAVDP on datasheet, min 6 ns
  const uint32_t addset = (TAVDP_MIN / time_per_cycle) + 1; // give setup of min 6ns

  // time between AVD high (address is available) and OE low (memory can write)
  // tAVDO on the datasheet, min 4 ns
  const uint32_t addhld = (TADVO_MIN / time_per_cycle) + 1; // gives hold of min 4ns

  // time between OE low (memory can write) and valid data being available
  // FIXME: optimize this equation
  // current linear equation has slope of 1 cycle/SETUP_STEP, with initial value 1
  // setupTime based on h_clock frequency
  // equation derived from existing working values; 5 at 64Mhz, 8 at 128 Mhz
  // the data was then interpolated into a line, with a padded value of 1
  const uint32_t datast = (h_clock / SETUP_STEP) + 1;

  // Time between chip selects
  // not on the datasheet, picked a random safe number
  // FIXME: at high bus frequencies, more than one cycle may be needed
  const uint32_t busturn = 1; // TODO: actually ok? See back-to-back Read/Write Cycle

  // Async multiplexed 16-bit NOR
  struct fmc_nor_config config = {
    .bcr = STM32F4_FMC_BCR_MUXEN |
           PBL_FIELD_PREP(STM32F4_FMC_BCR_MTYP_MASK, STM32F4_FMC_BCR_MTYP_NOR) |
           PBL_FIELD_PREP(STM32F4_FMC_BCR_MWID_MASK, STM32F4_FMC_BCR_MWID_16) |
           STM32F4_FMC_BCR_WREN | STM32F4_FMC_BCR_WAITEN,
    // CLKDIV and DATLAT are not used for async NOR
    .btr = prv_fmc_btr(addset, addhld, datast, busturn, 15, 15),
  };

  // configure the peripheral before we try to read from it
  prv_switch_flash_mode(&config);

  uint16_t configuration_register = prv_read_configuration_register();
  // clear bits that are about to be set
  configuration_register &= 0x0278; // clear bits [15:10], [8:7], [2:0]

  // add one. This way, if (h_clock < MAX_FREQ), only divide by one (use h_clock as is)
  // else divide by whatever is needed to be under MAX_FREQ
  uint32_t clk_division = (h_clock / (MAX_FREQ + 1)) + 1;

  // Update necessary parameters for synchronous modes
  if (burst_mode) {
    config.bcr |= STM32F4_FMC_BCR_BURSTEN | STM32F4_FMC_BCR_WAITCFG;

    // nor_timing_init.FMC_DataSetupTime = 1;           // FIXME: originally set to 1 for 64Mhz
    // but sync burst was not working at this value;
    // commented out so the DataSetupTime for ASYNC (up above) is used instead
    // this is to ensure sync_burst works with dynamic changes to h_clk frequency

    uint16_t wait_state = prv_get_num_wait_cycles(h_clock / clk_division);
    // CLKDIV divides h_clock if h_clock > 108MHZ; testing shows that a difference of 4 needs to
    // be maintained between wait_state and latency
    config.btr = prv_fmc_btr(addset, addhld, datast, busturn, clk_division, wait_state - 4);

    // Set bits according to value needed - see Table 7.11 in data sheet
    // [15]    Device Read Mode                0b0     Synchronous Read Mode
    // [14:11] Programmable Read Wait States   0bXXXX  N wait cycles, wait states set to (N - 2)
    // [10]    RDY Polarity                    0b1     RDY signal is active high (default)
    // [8]     RDY Timing                      0b0     RDY active once cycle before data (default)
    // [7]     Output Drive Strength           0b0     Full Drive=Current Driver Strength (default)
    // [2:0]   Burst Length                    0b000   Continuous (default)
    configuration_register |= 0x400 | (((wait_state - 2) << 11) & (WAIT_STATE_MASK));
  } else {
    // Set bits according to value needed - see Table 7.11 in data sheet
    // [15]    Device Read Mode                0b1     Asynchronous Read Mode
    // [14:11] Programmable Read Wait States   0b1011  13 wait cycles (default)
    // [10]    RDY Polarity                    0b1     RDY signal is active high (default)
    // [8]     RDY Timing                      0b1     RDY active with data (default)
    // [7]     Output Drive Strength           0b0     Full Drive=Current Driver Strength (default)
    // [2:0]   Burst Length                    0b000   Continuous (default)
    configuration_register |= 0xDD00;
  }

  prv_write_configuration_register(configuration_register);

  prv_switch_flash_mode(&config);

  prv_poll_for_ready(0);
  flash_impl_release();

  return S_SUCCESS;
}

// Flash class driver
///////////////////////////////////////////////////////////////////////////////

//! Sector of the erase in progress, for suspend and resume.
static FlashAddress s_erase_addr;

// First try plus the three retries the stock firmware made.
#define ERASE_ATTEMPTS 4U

// Longest wait for one erase when finishing an interrupted one at boot.
#define ERASE_RESUME_TIMEOUT_MS 10000U

#ifdef CONFIG_SOC_STM32F4
#include <pbl/system/rtc_registers.h>

#include <stm32f4xx_rtc.h>

extern void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value);
extern uint32_t RTC_ReadBackupRegister(uint32_t reg_id);

// The sector being erased, kept in an RTC backup register across resets as the stock firmware
// did, so a reset in the middle of an erase (watchdog, power loss) finishes it at the next boot
// instead of leaving a half-erased sector behind.
// The stock firmware's layout, so its recovery firmware (which shares the register) reads it
// the same way: bit 31 erase in progress, bit 30 subsector, the address below.
#define ERASE_PROGRESS_IN_PROGRESS 0x80000000U
#define ERASE_PROGRESS_SUBSECTOR   0x40000000U
#define ERASE_PROGRESS_ADDR_MASK   0x3FFFFFFFU

static void prv_erase_progress_set(FlashAddress addr) {
  uint32_t reg = ERASE_PROGRESS_IN_PROGRESS | (addr & ERASE_PROGRESS_ADDR_MASK);
  if (addr < BOTTOM_BOOT_REGION_END) {
    reg |= ERASE_PROGRESS_SUBSECTOR;
  }
  RTC_WriteBackupRegister(RTC_BKP_FLASH_ERASE_PROGRESS, reg);
}

static void prv_erase_progress_clear(void) {
  RTC_WriteBackupRegister(RTC_BKP_FLASH_ERASE_PROGRESS, 0U);
}

//! Finish an erase that a reset interrupted, as the stock firmware did: the sector is erased
//! again whatever it holds now, and the marker is cleared only afterwards so that a reset
//! during the repair repeats it.
static void prv_erase_progress_resume(void) {
  const uint32_t reg = RTC_ReadBackupRegister(RTC_BKP_FLASH_ERASE_PROGRESS);
  if ((reg & ERASE_PROGRESS_IN_PROGRESS) == 0U) {
    return;
  }
  const FlashAddress addr = flash_impl_get_sector_base_address(reg & ERASE_PROGRESS_ADDR_MASK);
  const uint32_t size = addr < BOTTOM_BOOT_REGION_END ? BOTTOM_BOOT_SECTOR_SIZE : S29VS_SECTOR_SIZE;
  if (addr >= S29VS_SIZE || flash_guard_is_protected(addr, size)) {
    PBL_LOG_WRN("Ignoring the interrupted erase of 0x%" PRIx32, addr);
    prv_erase_progress_clear();
    return;
  }

  PBL_LOG_WRN("Erase of 0x%" PRIx32 " was interrupted; finishing it", addr);
  // An interrupted sector can take much longer to erase, or fail the first time.
  for (unsigned int attempt = 0U; attempt < ERASE_ATTEMPTS; attempt++) {
    status_t status = flash_impl_erase_sector_begin(addr);
    if (PASSED(status)) {
      uint32_t ms = 0U;
      while ((status = flash_impl_get_erase_status()) == E_BUSY && ms++ < ERASE_RESUME_TIMEOUT_MS) {
        pbl_busy_wait_us(1000);
        watchdog_feed();
      }
    }
    if (PASSED(status)) {
      break;
    }
    PBL_LOG_ERR("Could not finish the erase of 0x%" PRIx32 " (%" PRId32 ")", addr, status);
  }
  prv_erase_progress_clear();
}
#else
static inline void prv_erase_progress_set(FlashAddress addr) {
  (void)addr;
}
static inline void prv_erase_progress_clear(void) {
}
static inline void prv_erase_progress_resume(void) {
}
#endif

static int prv_errno(status_t status) {
  if (PASSED(status)) {
    return 0;
  }
  switch (status) {
    case E_BUSY:
      return -EBUSY;
    case E_AGAIN:
      return -EAGAIN;
    case E_INVALID_OPERATION:
      return -EACCES;
    case E_INVALID_ARGUMENT:
      return -EINVAL;
    default:
      return -EIO;
  }
}

static int prv_init(const struct pbl_flash *flash) {
  const status_t status = flash_impl_init(pbl_flash_in_coredump(flash));
  if (PASSED(status) && !pbl_flash_in_coredump(flash)) {
    prv_erase_progress_resume();
  }
  return prv_errno(status);
}

static int prv_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  return prv_errno(flash_impl_read_sync(buf, addr, len));
}

static int prv_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  const uint8_t *data = buf;

  if (flash_guard_is_protected(addr, len)) {
    return -EACCES;
  }

  while (len > 0U) {
    const int written = flash_impl_write_page_begin(data, addr, len);
    if (written < 0) {
      return prv_errno(written);
    }

    status_t status;
    while ((status = flash_impl_get_write_status()) == E_BUSY) {
    }
    if (FAILED(status)) {
      return prv_errno(status);
    }

    data += written;
    addr += (uint32_t)written;
    len -= (size_t)written;
  }

  return 0;
}

//! Wait for the erase in progress to end. Up to a few seconds per sector (typically 0.35-0.6 s):
//! sleep between polls, as the stock firmware did, unless the scheduler cannot run (boot, core
//! dump, interrupts locked).
static status_t prv_erase_wait(void) {
  const bool can_sleep = pbl_kernel_is_running() && !pbl_in_isr() && !pbl_irq_is_locked();
  status_t status;
  while ((status = flash_impl_get_erase_status()) == E_BUSY) {
    if (can_sleep) {
      pbl_sleep_ms(2);
    }
  }
  return status;
}

//! Erase the 32 KiB boot sectors in [addr, addr + size), polled.
static int prv_erase_boot_sectors(uint32_t addr, uint32_t size) {
  for (uint32_t sector = addr; sector < addr + size; sector += BOTTOM_BOOT_SECTOR_SIZE) {
    status_t status;
    // A failed erase is tried again, as the stock firmware did.
    for (unsigned int attempt = 0U; attempt < ERASE_ATTEMPTS; attempt++) {
      status = flash_impl_erase_sector_begin(sector);
      if (FAILED(status)) {
        break;
      }
      s_erase_addr = sector;
      prv_erase_progress_set(sector);
      status = prv_erase_wait();
      prv_erase_progress_clear();
      if (status != E_ERROR) {
        break;
      }
      PBL_LOG_WRN("Erase of 0x%" PRIx32 " failed, attempt %u", sector, attempt + 1U);
    }
    if (FAILED(status)) {
      return prv_errno(status);
    }
  }
  return 1;
}

static int prv_erase(const struct pbl_flash *flash, uint32_t addr, uint32_t size) {
  if (flash_guard_is_protected(addr, size)) {
    return -EACCES;
  }

  if (addr < BOTTOM_BOOT_REGION_END) {
    return prv_erase_boot_sectors(addr, size);
  }
  if (size != S29VS_SECTOR_SIZE) {
    return -EINVAL;
  }

  // A core dump leaves the marker alone: it may name the sector whose erase the crash cut
  // short, which the next boot has to finish.
  const bool track = !pbl_flash_in_coredump(flash);

  s_erase_addr = addr;
  if (track) {
    prv_erase_progress_set(addr);
  }
  const status_t status = flash_impl_erase_sector_begin(addr);
  if (FAILED(status) && track) {
    prv_erase_progress_clear();
  }
  return prv_errno(status);
}

static int prv_erase_status(const struct pbl_flash *flash) {
  const status_t status = flash_impl_get_erase_status();
  if (status != E_BUSY && status != E_AGAIN && !pbl_flash_in_coredump(flash)) {
    prv_erase_progress_clear();
  }
  return prv_errno(status);
}

static int prv_erase_suspend(const struct pbl_flash *flash) {
  const status_t status = flash_impl_erase_suspend(s_erase_addr);
  return (status == S_NO_ACTION_REQUIRED) ? 1 : prv_errno(status);
}

static int prv_erase_resume(const struct pbl_flash *flash) {
  return prv_errno(flash_impl_erase_resume(s_erase_addr));
}

static void prv_power_down(const struct pbl_flash *flash, pbl_flash_pm_fn_t fn, void *arg) {
  (void)flash_impl_enter_low_power_mode();
  fn(arg);
  (void)flash_impl_exit_low_power_mode();
}

static const struct pbl_flash_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
  .erase_status = prv_erase_status,
  .erase_suspend = prv_erase_suspend,
  .erase_resume = prv_erase_resume,
  .power_down = prv_power_down,
};

static const struct pbl_flash_geometry s_geometry = {
  .size = S29VS_SIZE,
  .page_size = S29VS_PAGE_SIZE,
  .sector_size = S29VS_SECTOR_SIZE,
  // Only the boot sectors erase in 32 KiB: callers erase 128 KiB elsewhere.
  .subsector_size = BOTTOM_BOOT_SECTOR_SIZE,
  .sector_erase_ms = 800,
  .subsector_erase_ms = 400,
};

PBL_FLASH_STATE_DEFINE(pbl_flash_s29vs);
const struct pbl_flash pbl_flash_s29vs = PBL_FLASH_INIT(pbl_flash_s29vs, "flash", nullptr, nullptr,
                                                        &s_ops, &s_geometry, nullptr, nullptr);
PBL_DEVICE_REGISTER(pbl_flash_s29vs, &pbl_flash_s29vs.dev);
