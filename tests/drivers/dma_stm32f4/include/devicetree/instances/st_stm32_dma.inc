/* SPDX-FileCopyrightText: 2026 retake */
/* SPDX-License-Identifier: Apache-2.0 */

/* DMA2 of the STM32F4, at its address, with a clock of the test's controller */
PBL_DMA_STM32F4_DEFINE(dt_dma2, "dma2", 0x40026400, PBL_CLK(&s_rcc, 0), 56, 57, 58, 59, 60, 68, 69,
                       70);
