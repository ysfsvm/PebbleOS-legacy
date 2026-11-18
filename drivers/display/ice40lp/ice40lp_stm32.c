/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Brian Silvestri */
/* SPDX-License-Identifier: Apache-2.0 */

//! Snowy and spalding display: an iCE40 FPGA framebuffer on an STM32F4 SPI.
//!
//! The FPGA is programmed at init with its bitstream, which is stored
//! deflated and inflated on the way out. It then takes frames: SCS
//! low, CMD_FRAME_BEGIN, then one line per panel line: a framebuffer column on
//! snowy, a row of the circular framebuffer on spalding (shifted by the
//! factory display offsets). BUSY is high while
//! the FPGA cannot take more data. Lines go out by DMA from two line buffers:
//! when one is done the next starts, from the DMA interrupt or, if BUSY is
//! high, from BUSY's falling edge, and the line after it is prepared in the
//! buffer just freed. STOP mode is blocked during a frame. The boot frame,
//! sent before the kernel runs, is polled.
//!
//! Nothing here asserts on a misbehaving FPGA: a crash loop before the
//! firmware is marked stable ends in the stock bootloader starting PRF.

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/board.h>
#include <pbl/drivers/display/ice40lp.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/gpio/stm32.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/timer.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/stm32/clock.h>
#include <pbl/soc/stm32/cmsis.h>
#include <pbl/soc/stm32/sleep.h>

#include <uzlib.h>
#ifdef CONFIG_DISPLAY_ICE40LP_ROUND
#include <applib/graphics/framebuffer.h>
#include <mfg/mfg_info.h>
#endif

#define CMD_FRAME_BEGIN 0x5

// Commands of the bootloader's FPGA configuration.
#define BOOT_CMD_DISPLAY_ON 3
#define BOOT_CMD_DRAW_SCENE 4
#define BOOT_SCENE_SPLASH   1

#ifdef CONFIG_DISPLAY_ICE40LP_ROUND
// The panel is scanned in rows, top to bottom.
#define DISP_LINES  CONFIG_PLATFORM_DISPLAY_HEIGHT
#define DISP_PIXELS CONFIG_PLATFORM_DISPLAY_WIDTH
#else
// The panel is scanned in columns: each line is one framebuffer column,
// bottom to top.
#define DISP_LINES  CONFIG_PLATFORM_DISPLAY_WIDTH
#define DISP_PIXELS CONFIG_PLATFORM_DISPLAY_HEIGHT
#endif

//! A full frame should take no longer than 33 ms.
#define BUSY_TIMEOUT_US 40000
//! Give up on a DMA frame that has not finished by then.
#define FRAME_TIMEOUT_MS 200U
//! Typical NVCM configuration time is 56 ms.
#define CDONE_TIMEOUT_US 100000
//! The bitstream's DEFLATE window (tools/deflate_raw.py default).
#define BITSTREAM_WINDOW 512
//! Configuration attempts, at boot and when the FPGA lost its configuration, as stock.
#define PROGRAM_ATTEMPTS 3
//! Between recovery attempts while the FPGA stays unconfigured.
#define REPROGRAM_INTERVAL_MS 10000U

//! Raw DEFLATE stream of the bitstream.
extern const uint8_t ice40lp_bitstream_start[];
extern const uint8_t ice40lp_bitstream_end[];

static const struct pbl_display *s_disp;
static bool s_programmed;
//! Watch worn rotated by 180 degrees: frames go out flipped both ways.
static bool s_rotated_180;
static struct pbl_timer s_frame_timer;
#ifdef CONFIG_DISPLAY_ICE40LP_ROUND
//! Factory measured offset of the panel, in pixels, read at the first frame.
static GPoint s_offset;
static bool s_offset_read;
#endif

// DMA frame state. Line n is prepared in s_lines[n & 1].
static uint8_t s_lines[2][DISP_PIXELS];
static const uint8_t *s_fb;
static volatile int s_sent;
static volatile bool s_frame_active;
static volatile bool s_waiting_busy;

static void prv_busy_isr(void *arg);

//! BUSY and CDONE falling edges
static struct pbl_irq_spec s_busy_irq;
static struct pbl_irq_spec s_cdone_irq;
//! BUSY interrupt in use, else polled
static bool s_busy_irq_ok;

static int prv_request_irq(const struct pbl_gpio *gpio, pbl_intc_handler_t handler,
                           struct pbl_irq_spec *irq) {
  int ret;

  ret = pbl_gpio_to_irq(gpio, PBL_IRQ_TYPE_EDGE_FALLING, irq);
  if (ret != 0) {
    return ret;
  }
  ret = pbl_device_init(&irq->intc->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_intc_request(irq, handler, nullptr);
}

static uint16_t prv_baud_prescaler(uint32_t hz) {
  RCC_ClocksTypeDef clocks;
  RCC_GetClocksFreq(&clocks);

  // SPI6 is on APB2. Pick the smallest divider not above the target.
  static const uint16_t prescalers[] = {
    SPI_BaudRatePrescaler_2,   SPI_BaudRatePrescaler_4,   SPI_BaudRatePrescaler_8,
    SPI_BaudRatePrescaler_16,  SPI_BaudRatePrescaler_32,  SPI_BaudRatePrescaler_64,
    SPI_BaudRatePrescaler_128, SPI_BaudRatePrescaler_256,
  };
  for (unsigned i = 0; i < sizeof(prescalers) / sizeof(prescalers[0]); i++) {
    if ((clocks.PCLK2_Frequency >> (i + 1)) <= hz) {
      return prescalers[i];
    }
  }
  return SPI_BaudRatePrescaler_256;
}

static void prv_spi_configure(uint32_t hz) {
  SPI_TypeDef *spi = BOARD_CONFIG_DISPLAY.spi;
  SPI_InitTypeDef spi_cfg;

  SPI_Cmd(spi, DISABLE);
  SPI_StructInit(&spi_cfg);
  spi_cfg.SPI_Direction = SPI_Direction_1Line_Tx;
  spi_cfg.SPI_Mode = SPI_Mode_Master;
  spi_cfg.SPI_DataSize = SPI_DataSize_8b;
  spi_cfg.SPI_CPOL = SPI_CPOL_High;
  spi_cfg.SPI_CPHA = SPI_CPHA_2Edge;
  spi_cfg.SPI_NSS = SPI_NSS_Soft;
  spi_cfg.SPI_BaudRatePrescaler = prv_baud_prescaler(hz);
  spi_cfg.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_Init(spi, &spi_cfg);
  SPI_Cmd(spi, ENABLE);
}

static void prv_spi_write(const uint8_t *buf, size_t len) {
  SPI_TypeDef *spi = BOARD_CONFIG_DISPLAY.spi;

  for (size_t i = 0; i < len; i++) {
    while (!(spi->SR & SPI_SR_TXE)) {
    }
    spi->DR = buf[i];
  }
  while (!(spi->SR & SPI_SR_TXE)) {
  }
  while (spi->SR & SPI_SR_BSY) {
  }
}

static bool prv_busy(void) {
  return pbl_gpio_get(&BOARD_CONFIG_DISPLAY.busy) > 0;
}

//! @return false if BUSY did not clear in time.
static bool prv_wait_not_busy(void) {
  // A task sleeps between polls, as the stock firmware did, instead of spinning for up to
  // 40 ms; before the kernel runs, and from interrupts, it spins.
  const bool can_sleep = pbl_kernel_is_running() && !pbl_in_isr() && !pbl_irq_is_locked();
  for (int us = 0; us < BUSY_TIMEOUT_US; us += can_sleep ? 1000 : 10) {
    if (!prv_busy()) {
      return true;
    }
    if (can_sleep) {
      pbl_sleep_ms(1);
    } else {
      pbl_busy_wait_us(10);
    }
  }
  return false;
}

#ifdef CONFIG_DISPLAY_ICE40LP_STM32_BUSY_EDGE
//! Given by BUSY's falling edge while a task waits for a frame.
static PBL_SEM_DEFINE(s_busy_fell, 0, 1);
static volatile bool s_waiting_frame;

//! Like prv_wait_not_busy(), but a task sleeps until BUSY's falling edge instead of polling.
static bool prv_wait_not_busy_edge(void) {
  if (s_busy_irq_ok && prv_busy() && pbl_kernel_is_running() && !pbl_in_isr() &&
      !pbl_irq_is_locked()) {
    (void)pbl_sem_take(&s_busy_fell, PBL_NO_WAIT);
    // STOP mode would delay the wake-up.
    soc_stm32_stop_block();
    s_waiting_frame = true;
    (void)pbl_intc_enable(&s_busy_irq);
    // Arm before looking again, so the falling edge cannot be missed.
    if (prv_busy()) {
      (void)pbl_sem_take(&s_busy_fell, PBL_MSEC(BUSY_TIMEOUT_US / 1000));
    }
    s_waiting_frame = false;
    (void)pbl_intc_disable(&s_busy_irq);
    soc_stm32_stop_release();
  }
  // The level decides: after a timeout or a glitch this polls as before.
  return prv_wait_not_busy();
}
#endif

//! Inflate the bitstream to the FPGA, a line buffer at a time; they are not
//! in use yet. @return false if it did not inflate to the end.
static bool prv_write_bitstream(void) {
  struct {
    TINF_DATA d;
    uint8_t dict[BITSTREAM_WINDOW];
  } *s = pbl_malloc(sizeof(*s));
  if (!s) {
    PBL_LOG_ERR("No memory to inflate the FPGA bitstream");
    return false;
  }

  uzlib_uncompress_init(&s->d, s->dict, sizeof(s->dict));
  s->d.source = ice40lp_bitstream_start;
  s->d.source_limit = ice40lp_bitstream_end;
  s->d.source_read_cb = nullptr;
  int res;
  do {
    uint8_t *chunk = s_lines[0];
    s->d.dest_start = chunk;
    s->d.dest = chunk;
    s->d.dest_limit = chunk + sizeof(s_lines[0]);
    res = uzlib_uncompress(&s->d);
    prv_spi_write(chunk, s->d.dest - chunk);
  } while (res == TINF_OK);
  pbl_free(s);

  if (res != TINF_DONE) {
    PBL_LOG_ERR("FPGA bitstream inflate failed: %d", res);
    return false;
  }
  return true;
}

static bool prv_program(void) {
  const struct pbl_gpio *creset = &BOARD_CONFIG_DISPLAY.creset;
  const struct pbl_gpio *scs = &BOARD_CONFIG_DISPLAY.scs;

  prv_spi_configure(BOARD_CONFIG_DISPLAY.base_spi_frequency);

  // SCS low while CRESET rises selects SPI slave configuration.
  pbl_gpio_set(scs, true);
  pbl_gpio_set(creset, false);
  // CRESET must stay low for 200 ns.
  pbl_busy_wait_us(10);
  pbl_gpio_set(creset, true);
  // The iCE40 needs 800 us of housekeeping after reset.
  pbl_busy_wait_us(1000);

  const bool written = prv_write_bitstream();

  // SCS high so these clocks are not taken as commands; 49+ clocks finish
  // the configuration.
  pbl_gpio_set(scs, false);
  static const uint8_t zeros[9];
  prv_spi_write(zeros, sizeof(zeros));

  bool done = false;
  for (int us = 0; us < CDONE_TIMEOUT_US; us += 100) {
    if (pbl_gpio_get(&BOARD_CONFIG_DISPLAY.cdone) > 0) {
      done = true;
      break;
    }
    pbl_busy_wait_us(100);
  }
  if (!done) {
    PBL_LOG_ERR("FPGA CDONE not high after programming");
  }

  prv_spi_configure(BOARD_CONFIG_DISPLAY.fast_spi_frequency);
  return written && done;
}

//! Set when CDONE fell: the FPGA was reset (e.g. by ESD) and came back, if at all, with the
//! bootloader's configuration from its NVCM, which raises CDONE again.
static volatile bool s_config_lost;

static bool prv_program_retry(void) {
  for (int attempt = 1; attempt <= PROGRAM_ATTEMPTS; attempt++) {
    if (prv_program()) {
      // Programming pulled CDONE low itself.
      s_config_lost = false;
      return true;
    }
    PBL_LOG_WRN("FPGA configuration failed (try %d)", attempt);
  }
  return false;
}

static bool prv_configured(void) {
  return pbl_gpio_get(&BOARD_CONFIG_DISPLAY.cdone) > 0;
}

static void prv_cdone_isr(void *arg) {
  s_config_lost = true;
}

//! In KernelMain, between frames: configure the FPGA again if it lost its configuration (CDONE
//! low, e.g. after ESD), as the stock firmware did, or never got it. Rate-limited, as it takes a
//! while and may keep failing.
static void prv_recover(void) {
  static uint64_t s_last_attempt;
  static bool s_attempted;

  if (s_programmed && prv_configured() && !s_config_lost) {
    return;
  }
  const uint64_t now = pbl_uptime_ticks();
  if (s_attempted && (now - s_last_attempt) < (REPROGRAM_INTERVAL_MS * PBL_TICK_HZ) / 1000U) {
    return;
  }
  s_attempted = true;
  s_last_attempt = now;
  PBL_LOG_WRN("FPGA lost its configuration (%d%d): configuring it again", s_programmed,
              s_config_lost);
  s_programmed = prv_program_retry();
}

static void prv_power_enable(void) {
  // 1 ms between rails; the PMIC brings up VLCD (3.2 V) first.
  pbl_busy_wait_us(2000);
  pbl_gpio_set(&BOARD_CONFIG_DISPLAY.rail_6v6, true);
  pbl_busy_wait_us(2000);
  pbl_gpio_set(&BOARD_CONFIG_DISPLAY.rail_4v5, true);
}

static void prv_mco1_enable(void) {
  pbl_gpio_stm32_af(&BOARD_MCO1, GPIO_AF_MCO, 0, GPIO_Speed_2MHz);
  RCC_MCO1Config(RCC_MCO1Source_LSE, RCC_MCO1Div_1);
}

//! Has the bootloader's FPGA configuration redraw its logo and turn the
//! panel on, as the stock firmware did; the panel keeps it until our first
//! frame.
static void prv_show_boot_splash(void) {
  const struct pbl_gpio *scs = &BOARD_CONFIG_DISPLAY.scs;
  const uint8_t draw_splash[] = {BOOT_CMD_DRAW_SCENE, BOOT_SCENE_SPLASH};
  const uint8_t display_on = BOOT_CMD_DISPLAY_ON;

  prv_spi_configure(BOARD_CONFIG_DISPLAY.base_spi_frequency);

  prv_wait_not_busy();
  pbl_gpio_set(scs, true);
  prv_spi_write(draw_splash, sizeof(draw_splash));
  pbl_gpio_set(scs, false);

  // Don't turn the panel on before the logo is drawn.
  if (!prv_wait_not_busy()) {
    return;
  }
  pbl_gpio_set(scs, true);
  prv_spi_write(&display_on, 1);
  pbl_gpio_set(scs, false);
}

//! Polled all-black frame; the line buffers must not be in use.
static void prv_send_black_frame(void) {
  uint8_t *line = s_lines[0];
  memset(line, 0, DISP_PIXELS);
  const struct pbl_gpio *scs = &BOARD_CONFIG_DISPLAY.scs;
  pbl_gpio_set(scs, true);
  pbl_busy_wait_us(5);
  const uint8_t cmd = CMD_FRAME_BEGIN;
  prv_spi_write(&cmd, 1);
  for (int i = 0; i < DISP_LINES; i++) {
    if (!prv_wait_not_busy()) {
      break;
    }
    prv_spi_write(line, DISP_PIXELS);
  }
  pbl_gpio_set(scs, false);
}

static void prv_frame_timeout(struct pbl_timer *timer);

static int prv_init(const struct pbl_display *disp) {
  const BoardConfigIce40Display *cfg = &BOARD_CONFIG_DISPLAY;

  s_disp = disp;
  pbl_timer_init(&s_frame_timer, prv_frame_timeout);

  prv_mco1_enable();

  soc_stm32_clock_set(SOC_STM32_APB2, cfg->spi_rcc, true);
  pbl_gpio_stm32_af(&cfg->sclk, cfg->spi_af, 0, GPIO_Speed_50MHz);
  pbl_gpio_stm32_af(&cfg->mosi, cfg->spi_af, 0, GPIO_Speed_50MHz);
  pbl_gpio_configure(&cfg->scs, PBL_GPIO_OUTPUT_INACTIVE);
  pbl_gpio_configure(&cfg->creset, PBL_GPIO_OUTPUT_ACTIVE);
  pbl_gpio_configure(&cfg->cdone, PBL_GPIO_INPUT);
  pbl_gpio_configure(&cfg->busy, PBL_GPIO_INPUT);
  // Keep the rails as the bootloader left them: dropping them under its logo
  // brings back a stale inverted image.
  pbl_gpio_configure(&cfg->rail_6v6, PBL_GPIO_OUTPUT);
  pbl_gpio_configure(&cfg->rail_4v5, PBL_GPIO_OUTPUT);

  int ret = prv_request_irq(&cfg->busy, prv_busy_isr, &s_busy_irq);
  if (ret == 0) {
    s_busy_irq_ok = true;
  } else {
    PBL_LOG_ERR("No BUSY interrupt (%d): polling BUSY", ret);
  }

  ret = pbl_device_init(&cfg->tx_dma.ctrl->dev);
  if (ret != 0) {
    return ret;
  }
  const struct pbl_dma_slave_config dma_cfg = {
    .dst_addr = (uintptr_t)&cfg->spi->DR,
    .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
  };
  ret = pbl_dma_slave_config(&cfg->tx_dma, &dma_cfg);
  if (ret != 0) {
    return ret;
  }

  prv_show_boot_splash();
  s_programmed = prv_program_retry();
  prv_power_enable();

  // As the stock firmware: a falling CDONE means the configuration was lost.
  ret = prv_request_irq(&cfg->cdone, prv_cdone_isr, &s_cdone_irq);
  if (ret == 0) {
    ret = pbl_intc_enable(&s_cdone_irq);
  }
  if (ret != 0) {
    PBL_LOG_ERR("No CDONE interrupt (%d)", ret);
  }

  return 0;
}

#ifdef CONFIG_DISPLAY_ICE40LP_ROUND
static const GBitmapDataRowInfoInternal *prv_row_info(int y) {
  return &g_gbitmap_legacy_3x_data_row_infos[y];
}

//! Pixel x of a circular framebuffer row; black outside the circle.
static uint8_t prv_round_pixel(const uint8_t *row, const GBitmapDataRowInfoInternal *info, int x) {
  if (s_rotated_180) {
    x = CONFIG_PLATFORM_DISPLAY_WIDTH - 1 - x;
  }
  return (x >= info->min_x && x <= info->max_x) ? row[x] : 0;
}
#endif

#ifdef CONFIG_DISPLAY_ICE40LP_STM32_FAST_LINE
// -O3 moves the rotation test out of the loop; -O2 compiles it like -Os.
#define LINE_OPTIMIZE PBL_OPTIMIZE("O3")
#else
#define LINE_OPTIMIZE
#endif

//! One panel line: the MSBs of every pixel's color channels, then the LSBs.
static LINE_OPTIMIZE void prv_get_line(const uint8_t *fb, int index, uint8_t *line) {
#ifdef CONFIG_DISPLAY_ICE40LP_ROUND
  int y = index - s_offset.y;
  if (y < 0 || y >= CONFIG_PLATFORM_DISPLAY_HEIGHT) {
    memset(line, 0, DISP_PIXELS);
    return;
  }
  if (s_rotated_180) {
    y = CONFIG_PLATFORM_DISPLAY_HEIGHT - 1 - y;
  }
  const GBitmapDataRowInfoInternal *info = prv_row_info(y);
  const uint8_t *row = fb + info->offset;
  for (int dst = 0; dst < DISP_PIXELS; dst += 2) {
    const int x = dst - s_offset.x;
    const uint8_t odd = prv_round_pixel(row, info, x);
    const uint8_t even = prv_round_pixel(row, info, x + 1);
    line[dst / 2] = (odd & 0b00101010) | ((even & 0b00101010) >> 1);
    line[dst / 2 + DISP_PIXELS / 2] = ((odd & 0b00010101) << 1) | (even & 0b00010101);
  }
#else
  for (int src = 0; src < DISP_PIXELS; src += 2) {
    uint8_t even;
    uint8_t odd;
    if (s_rotated_180) {
      const int col = CONFIG_PLATFORM_DISPLAY_WIDTH - 1 - index;
      even = fb[CONFIG_PLATFORM_DISPLAY_WIDTH * (src + 1) + col];
      odd = fb[CONFIG_PLATFORM_DISPLAY_WIDTH * src + col];
    } else {
      even = fb[CONFIG_PLATFORM_DISPLAY_WIDTH * (CONFIG_PLATFORM_DISPLAY_HEIGHT - 2 - src) + index];
      odd = fb[CONFIG_PLATFORM_DISPLAY_WIDTH * (CONFIG_PLATFORM_DISPLAY_HEIGHT - 1 - src) + index];
    }
    line[src / 2] = (odd & 0b00101010) | ((even & 0b00101010) >> 1);
    line[src / 2 + DISP_PIXELS / 2] = ((odd & 0b00010101) << 1) | (even & 0b00010101);
  }
#endif
}

//! With interrupts locked or from an interrupt.
static void prv_end_frame(void) {
  const BoardConfigIce40Display *cfg = &BOARD_CONFIG_DISPLAY;

  if (s_busy_irq_ok) {
    (void)pbl_intc_disable(&s_busy_irq);
  }
  s_waiting_busy = false;
  // The last byte may still be shifting out.
  while (!(cfg->spi->SR & SPI_SR_TXE)) {
  }
  while (cfg->spi->SR & SPI_SR_BSY) {
  }
  SPI_I2S_DMACmd(cfg->spi, SPI_I2S_DMAReq_Tx, DISABLE);
  pbl_gpio_set(&cfg->scs, false);
  s_frame_active = false;
  soc_stm32_stop_release();
  pbl_timer_stop(&s_frame_timer);
}

//! End the frame on a DMA error, from an interrupt.
static void prv_fail_frame(int status) {
  prv_end_frame();
  PBL_LOG_WRN("Display DMA failed at line %d: %d", s_sent, status);
  pbl_display_complete(s_disp, status);
}

static void prv_dma_done(void *arg, int status);

//! Start line s_sent and prepare the one after it.
static int prv_send_next_line(void) {
  const int line = s_sent;
  int ret;

  s_sent = line + 1;
  ret = pbl_dma_prep_slave_single(&BOARD_CONFIG_DISPLAY.tx_dma, s_lines[line & 1], DISP_PIXELS,
                                  PBL_DMA_MEM_TO_DEV, prv_dma_done, nullptr);
  if (ret != 0) {
    return ret;
  }
  if (s_sent < DISP_LINES) {
    prv_get_line(s_fb, s_sent, s_lines[s_sent & 1]);
  }

  return 0;
}

static void prv_busy_isr(void *arg) {
#ifdef CONFIG_DISPLAY_ICE40LP_STM32_BUSY_EDGE
  if (s_waiting_frame) {
    s_waiting_frame = false;
    (void)pbl_intc_disable(&s_busy_irq);
    pbl_sem_give(&s_busy_fell);
    return;
  }
#endif
  if (!s_waiting_busy) {
    return;
  }
  s_waiting_busy = false;
  (void)pbl_intc_disable(&s_busy_irq);
  int ret = prv_send_next_line();
  if (ret != 0) {
    prv_fail_frame(ret);
  }
}

static void prv_dma_done(void *arg, int status) {
  int ret;

  if (!s_frame_active) {
    return;
  }
  if (status != 0) {
    prv_fail_frame(status);
    return;
  }
  if (s_sent >= DISP_LINES) {
    prv_end_frame();
    pbl_display_complete(s_disp, 0);
    return;
  }
  if (prv_busy() && !s_busy_irq_ok) {
    if (!prv_wait_not_busy()) {
      return;
    }
  } else if (prv_busy()) {
    // Arm before looking again, so the falling edge cannot be missed.
    s_waiting_busy = true;
    (void)pbl_intc_enable(&s_busy_irq);
    if (prv_busy()) {
      return;
    }
    s_waiting_busy = false;
    (void)pbl_intc_disable(&s_busy_irq);
  }
  ret = prv_send_next_line();
  if (ret != 0) {
    prv_fail_frame(ret);
  }
}

//! In KernelMain.
static void prv_start_dma_frame(const uint8_t *fb) {
  const BoardConfigIce40Display *cfg = &BOARD_CONFIG_DISPLAY;
  int ret;

#ifdef CONFIG_DISPLAY_ICE40LP_STM32_BUSY_EDGE
  if (!prv_wait_not_busy_edge()) {
#else
  if (!prv_wait_not_busy()) {
#endif
    // A new frame (SCS falling edge) resets the FPGA's frame logic. Stuck busy: configure it
    // again, as the stock firmware did.
    PBL_LOG_WRN("Display busy before frame: configuring the FPGA again");
    s_programmed = prv_program_retry();
  }

  s_fb = fb;
  s_sent = 0;
  prv_get_line(fb, 0, s_lines[0]);

  soc_stm32_stop_block();
  pbl_gpio_set(&cfg->scs, true);
  pbl_busy_wait_us(5);
  const uint8_t cmd = CMD_FRAME_BEGIN;
  prv_spi_write(&cmd, 1);
  SPI_I2S_DMACmd(cfg->spi, SPI_I2S_DMAReq_Tx, ENABLE);

  pbl_timer_start(&s_frame_timer, PBL_MSEC(FRAME_TIMEOUT_MS));
  pbl_irq_lock();
  s_frame_active = true;
  ret = prv_send_next_line();
  if (ret != 0) {
    prv_end_frame();
  }
  pbl_irq_unlock();

  if (ret != 0) {
    PBL_LOG_WRN("Display DMA did not start: %d", ret);
    pbl_display_complete(s_disp, ret);
  }
}

//! Stuck (e.g. BUSY never fell): end the frame rather than hang the UI.
static void prv_frame_timeout(struct pbl_timer *timer) {
  bool ended = false;

  pbl_irq_lock();
  if (s_frame_active) {
    (void)pbl_dma_terminate(&BOARD_CONFIG_DISPLAY.tx_dma);
    prv_end_frame();
    ended = true;
  }
  pbl_irq_unlock();

  if (ended) {
    PBL_LOG_WRN("Display frame timed out at line %d", s_sent);
    pbl_display_complete(s_disp, -ETIMEDOUT);
  }
}

static int prv_clear(const struct pbl_display *disp) {
  if (!s_programmed) {
    return 0;
  }

  // One black frame, as on the stock firmware; the PMIC cuts the rails later.
  prv_send_black_frame();

  return 0;
}

//! The rails are left to the PMIC power-off, as on the stock firmware.
static int prv_enable(const struct pbl_display *disp) {
  return 0;
}

static int prv_disable(const struct pbl_display *disp) {
  return 0;
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  s_rotated_180 = rotated;

  return 0;
}

static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  prv_recover();
  if (!s_programmed) {
    pbl_display_complete(disp, 0);
    return 0;
  }

#ifdef CONFIG_DISPLAY_ICE40LP_ROUND
  if (!s_offset_read) {
    s_offset = mfg_info_get_disp_offsets();
    s_offset_read = true;
    PBL_LOG_INFO("MFG display offsets (%d,%d)", s_offset.x, s_offset.y);
  }
  // Rows sit at their row info offsets; the panel takes whole frames.
  prv_start_dma_frame(buf->data - prv_row_info(0)->offset);
#else
  prv_start_dma_frame(buf->data);
#endif

  return 0;
}

const struct pbl_display_ops pbl_display_ice40lp_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
  .clear = prv_clear,
};

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

//! Reset the FPGA, as ESD would: it loses its configuration until the driver notices.
static int prv_cmd_fpga_reset(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_gpio_set(&BOARD_CONFIG_DISPLAY.creset, false);
  pbl_busy_wait_us(10);
  pbl_gpio_set(&BOARD_CONFIG_DISPLAY.creset, true);
  pbl_shell_print(sh, "FPGA reset, CDONE %d", prv_configured());
  return 0;
}

PBL_SHELL_CMD_REGISTER(fpga_reset, nullptr, "Reset the display FPGA (tests its recovery)",
                       prv_cmd_fpga_reset);
#endif
