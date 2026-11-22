/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define TRACE_DATA_READY \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 28\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 28 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 28\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 a8 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c6 00 / 00 80\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a2 00 00 / 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a4 / 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 47 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d6 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 56 08 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d1 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 51 10 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "irq 23 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 0a / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "irq 23 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 0a\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 aa / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "work\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 9b 00 / 00 80\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 92 00 00 00 00 00 00 00 00 00 / 00 00 08 00 f8 00 20 00 01 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "work\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 9b 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "reports:\n" \
  "sample -250 250 1000 1000000000\n" \
  ""

#define TRACE_DOUBLE_TAP \
  "irq 23 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 0a / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "irq 23 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 0a\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 aa / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 63 c4 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c1 00 / 00 05\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 64 0a / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d5 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 55 10 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d0 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 50 10 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 28\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 29 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 29\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 a9 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "work\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 9c 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 9e 00 / 00 c0\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e b1 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "work\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 9c 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 9e 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e b1 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 aa\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 a0 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 a0\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d5 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 55 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d0 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 50 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 a9\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 a5 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "reports:\n" \
  "event 1 2 1\n" \
  ""

#define TRACE_FIFO \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c6 00 / 00 80\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a2 00 00 / 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a4 / 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 46 25 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c7 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e b0 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 47 40 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d6 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 56 04 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d1 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 51 40 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "irq 23 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 0a / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "irq 23 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 0a\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 aa / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "work\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a2 00 00 / 00 3c 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a4 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 / 00 01 0e 1b 28 35 42 4f 5c 69 76 83 90 9d aa b7 c4 d1 de eb f8 05 12 1f 2c 39 46 53 60 6d 7a 87 94 a1 ae bb c8 d5 e2 ef fc 09 16 23 30 3d 4a 57 64 71 7e 8b 98 a5 b2 bf cc d9 e6 f3 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "reports:\n" \
  "sample 1253 437 2068 999600000\n" \
  "sample 3700 2884 -3484 999640000\n" \
  "sample -1852 -2668 -1036 999680000\n" \
  "sample 563 -221 1378 999720000\n" \
  "sample 3010 2194 3825 999760000\n" \
  "sample -2542 -3358 -1727 999800000\n" \
  "sample -95 -911 688 999840000\n" \
  "sample 2319 1504 3135 999880000\n" \
  "sample -3233 3951 -2417 999920000\n" \
  "sample -786 -1601 29 999960000\n" \
  ""

#define TRACE_FIFO_STILL_FULL \
  "work\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a2 00 00 / 00 1b 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a4 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 / 00 a0 9f 9e 9d 9c 9b 9a 99 98 97 96 95 94 93 92 91 90 8f 8e 8d 8c 8b 8a 89 88 87 86 80 80 80\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a2 00 00 / 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a4 / 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a2 00 00 / 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a4 / 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a2 00 00 / 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a4 / 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c6 00 / 00 25\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a2 00 00 / 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 98 00 00 00 / 00 00 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x a4 / 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 46 0f / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c7 00 / 00 40\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "reports:\n" \
  "sample -3074 -3011 -3137 999840000\n" \
  "sample -3262 -3199 -3325 999880000\n" \
  "sample -3450 -3388 -3513 999920000\n" \
  "sample -3639 -3576 -3701 999960000\n" \
  "sample -3983 -3764 -3984 1000000000\n" \
  ""

#define TRACE_INIT \
  "clk apb2 12 on\n" \
  "rst apb2 12 on\n" \
  "rst apb2 12 off\n" \
  "clk apb2 12 off\n" \
  "clk apb2 14 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x ff 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 80 00 / 00 d1\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e b6 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x ff 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 41 05 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "SPI1 CR1 035c CR2 0000 CRCPR 0007 I2SCFGR 0000 I2SPR 0002\n" \
  "GPIOA MODER a800a900 OTYPER 00000000 OSPEEDR 0c00a800 PUPDR 64000000\n" \
  "GPIOA ODR 00000010 AFRL 55500000 AFRH 00000000\n" \
  "GPIOG MODER 00000000 PUPDR 00000000\n" \
  "EXTI IMR 00000000 RTSR 00000060 FTSR 00000000 SYSCFG EXTICR2 00000660\n" \
  "RCC APB2ENR 00004000 APB2RSTR 00000000\n" \
  ""

#define TRACE_INIT_ABSENT \
  "clk apb2 12 on\n" \
  "rst apb2 12 on\n" \
  "rst apb2 12 off\n" \
  "clk apb2 12 off\n" \
  "clk apb2 14 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x ff 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 80 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5000us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x ff 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 80 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5000us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x ff 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 80 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "SPI1 CR1 035c CR2 0000 CRCPR 0007 I2SCFGR 0000 I2SPR 0002\n" \
  "GPIOA MODER a800a900 OTYPER 00000000 OSPEEDR 0c00a800 PUPDR 64000000\n" \
  "GPIOA ODR 00000010 AFRL 55500000 AFRH 00000000\n" \
  "GPIOG MODER 00000000 PUPDR 00000000\n" \
  "EXTI IMR 00000000 RTSR 00000060 FTSR 00000000 SYSCFG EXTICR2 00000660\n" \
  "RCC APB2ENR 00004000 APB2RSTR 00000000\n" \
  ""

#define TRACE_PEEK \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 92 00 00 00 00 00 00 00 00 00 / 00 10 02 f0 fd 00 10 34 12 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  ""

#define TRACE_POWER_MODES \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 28\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 26 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 26\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 a6 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 a6\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 ac / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 ac\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 a5 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 10 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "SPI1 CR1 035c CR2 0000 CRCPR 0007 I2SCFGR 0000 I2SPR 0002\n" \
  "GPIOA MODER a800a900 OTYPER 00000000 OSPEEDR 0c00a800 PUPDR 64000000\n" \
  "GPIOA ODR 00000010 AFRL 55500000 AFRH 00000000\n" \
  "GPIOG MODER 00000000 PUPDR 00000000\n" \
  "EXTI IMR 00000000 RTSR 00000060 FTSR 00000000 SYSCFG EXTICR2 00000660\n" \
  "RCC APB2ENR 00004000 APB2RSTR 00000000\n" \
  ""

#define TRACE_SHAKE \
  "irq 23 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 0a / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "irq 23 on\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 0a\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 aa / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 6c 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d5 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 55 04 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d0 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 50 07 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 60 64 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 5f 03 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 54 0d / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 28\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 26 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 26\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 a6 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "work\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 11 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 10\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 9c 00 / 00 04\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 9e 00 / 00 0a\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e b1 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 7e 12 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "wait 5us\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 83 00 / 00 20\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 60 0f / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 60 a3 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 60 a3 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 aa\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 a0 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d3 00 / 00 a0\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 53 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d5 00 / 00 04\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 55 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x d0 00 / 00 07\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 50 00 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x c0 00 / 00 a6\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "clk apb2 12 on\n" \
  "cs 0\n" \
  "x 40 a5 / 00 00\n" \
  "cs 1\n" \
  "clk apb2 12 off\n" \
  "sleep 2ms\n" \
  "reports:\n" \
  "event 0 0 -1\n" \
  ""
