/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define TRACE_CLOCK_ERROR \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "= panic\n" \
  ""

#define TRACE_EQUAL_VALUES \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng DR 5a5a5a5a x2\n" \
  "rng DR 0f0f0f0f\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= ok 0f0f0f0f\n" \
  ""

#define TRACE_FIRST_VALUE_ZERO \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng DR 00000000\n" \
  "rng DR 12345678\n" \
  "rng DR 9abcdef0\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= ok 9abcdef0\n" \
  ""

#define TRACE_NEVER_READY \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= fail 00000000\n" \
  ""

#define TRACE_NORMAL \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng DR 1c0ffee1\n" \
  "rng DR 2badcafe\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= ok 2badcafe\n" \
  ""

#define TRACE_SEED_ERROR \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng DR 11111111\n" \
  "rng SR 04\n" \
  "rng CR 00\n" \
  "rng CR 04\n" \
  "rng DR 22222222\n" \
  "rng DR 33333333\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= ok 33333333\n" \
  ""

#define TRACE_SEED_ERROR_THREE_TIMES \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng DR 11111111\n" \
  "rng SR 04\n" \
  "rng CR 00\n" \
  "rng CR 04\n" \
  "rng DR 22222222\n" \
  "rng SR 04\n" \
  "rng CR 00\n" \
  "rng CR 04\n" \
  "rng DR 33333333\n" \
  "rng SR 04\n" \
  "rng CR 00\n" \
  "rng CR 04\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= fail 33333333\n" \
  ""

#define TRACE_STUCK \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng DR 66666666 x99999\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= fail 66666666\n" \
  ""

#define TRACE_TWO_CALLS \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng DR 01010101\n" \
  "rng DR 02020202\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= ok 02020202\n" \
  "= rand\n" \
  "clk rng on\n" \
  "rng CR 04\n" \
  "rng DR 03030303\n" \
  "rng DR 04040404\n" \
  "rng CR 00\n" \
  "clk rng off\n" \
  "= ok 04040404\n" \
  ""
