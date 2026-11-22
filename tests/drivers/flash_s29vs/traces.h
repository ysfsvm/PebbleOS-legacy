/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define TRACE_EMPTY ""

#define TRACE_ERASE_BOOT \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0071\n"      \
  "W 000aaa 0070\n"      \
  "R 000000 0080\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 000080 0060\n"      \
  "W 000aaa 0080\n"      \
  "W 000554 0030\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 000000 0060\n"      \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "dr11 = c0000000\n"    \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "sleep 2ms\n"          \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "sleep 2ms\n"          \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0080\n"      \
  "clk 2 00000001 off\n" \
  "dr11 = 00000000\n"    \
  "clk 2 00000001 on\n"  \
  "W 008aaa 0071\n"      \
  "W 008aaa 0070\n"      \
  "R 008000 0080\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 008080 0060\n"      \
  "W 008aaa 0080\n"      \
  "W 008554 0030\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 008000 0060\n"      \
  "W 008aaa 0070\n"      \
  "R 008000 0000\n"      \
  "clk 2 00000001 off\n" \
  "dr11 = c0008000\n"    \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "sleep 2ms\n"          \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "sleep 2ms\n"          \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0080\n"      \
  "clk 2 00000001 off\n" \
  "dr11 = 00000000\n"    \
  "clk 2 00000001 on\n"  \
  "W 010aaa 0071\n"      \
  "W 010aaa 0070\n"      \
  "R 010000 0080\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 010080 0060\n"      \
  "W 010aaa 0080\n"      \
  "W 010554 0030\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 010000 0060\n"      \
  "W 010aaa 0070\n"      \
  "R 010000 0000\n"      \
  "clk 2 00000001 off\n" \
  "dr11 = c0010000\n"    \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "sleep 2ms\n"          \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "sleep 2ms\n"          \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0080\n"      \
  "clk 2 00000001 off\n" \
  "dr11 = 00000000\n"    \
  "clk 2 00000001 on\n"  \
  "W 018aaa 0071\n"      \
  "W 018aaa 0070\n"      \
  "R 018000 0080\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 018080 0060\n"      \
  "W 018aaa 0080\n"      \
  "W 018554 0030\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 018000 0060\n"      \
  "W 018aaa 0070\n"      \
  "R 018000 0000\n"      \
  "clk 2 00000001 off\n" \
  "dr11 = c0018000\n"    \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "sleep 2ms\n"          \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0000\n"      \
  "clk 2 00000001 off\n" \
  "sleep 2ms\n"          \
  "clk 2 00000001 on\n"  \
  "W 000aaa 0070\n"      \
  "R 000000 0080\n"      \
  "clk 2 00000001 off\n" \
  "dr11 = 00000000\n"    \
  ""

#define TRACE_ERASE_BOOT_ONE \
  "clk 2 00000001 on\n"      \
  "W 008aaa 0071\n"          \
  "W 008aaa 0070\n"          \
  "R 008000 0080\n"          \
  "W 000aaa 0060\n"          \
  "W 000554 0060\n"          \
  "W 008080 0060\n"          \
  "W 008aaa 0080\n"          \
  "W 008554 0030\n"          \
  "W 000aaa 0060\n"          \
  "W 000554 0060\n"          \
  "W 008000 0060\n"          \
  "W 008aaa 0070\n"          \
  "R 008000 0000\n"          \
  "clk 2 00000001 off\n"     \
  "dr11 = c0008000\n"        \
  "clk 2 00000001 on\n"      \
  "W 000aaa 0070\n"          \
  "R 000000 0000\n"          \
  "clk 2 00000001 off\n"     \
  "sleep 2ms\n"              \
  "clk 2 00000001 on\n"      \
  "W 000aaa 0070\n"          \
  "R 000000 0000\n"          \
  "clk 2 00000001 off\n"     \
  "sleep 2ms\n"              \
  "clk 2 00000001 on\n"      \
  "W 000aaa 0070\n"          \
  "R 000000 0080\n"          \
  "clk 2 00000001 off\n"     \
  "dr11 = 00000000\n"        \
  ""

#define TRACE_ERASE_ERROR \
  "dr11 = 80400000\n"     \
  "clk 2 00000001 on\n"   \
  "W 400aaa 0071\n"       \
  "W 400aaa 0070\n"       \
  "R 400000 0080\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 400080 0060\n"       \
  "W 400aaa 0080\n"       \
  "W 400554 0030\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 400000 0060\n"       \
  "W 400aaa 0070\n"       \
  "R 400000 0000\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0001\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0001\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 00a0\n"       \
  "clk 2 00000001 off\n"  \
  "dr11 = 00000000\n"     \
  ""

#define TRACE_ERASE_RETRY \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0071\n"       \
  "W 000aaa 0070\n"       \
  "R 000000 0080\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 000080 0060\n"       \
  "W 000aaa 0080\n"       \
  "W 000554 0030\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 000000 0060\n"       \
  "W 000aaa 0070\n"       \
  "R 000000 0000\n"       \
  "clk 2 00000001 off\n"  \
  "dr11 = c0000000\n"     \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0000\n"       \
  "clk 2 00000001 off\n"  \
  "sleep 2ms\n"           \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0000\n"       \
  "clk 2 00000001 off\n"  \
  "sleep 2ms\n"           \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 00a0\n"       \
  "clk 2 00000001 off\n"  \
  "dr11 = 00000000\n"     \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0071\n"       \
  "W 000aaa 0070\n"       \
  "R 000000 0080\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 000080 0060\n"       \
  "W 000aaa 0080\n"       \
  "W 000554 0030\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 000000 0060\n"       \
  "W 000aaa 0070\n"       \
  "R 000000 0000\n"       \
  "clk 2 00000001 off\n"  \
  "dr11 = c0000000\n"     \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0000\n"       \
  "clk 2 00000001 off\n"  \
  "sleep 2ms\n"           \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0000\n"       \
  "clk 2 00000001 off\n"  \
  "sleep 2ms\n"           \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0080\n"       \
  "clk 2 00000001 off\n"  \
  "dr11 = 00000000\n"     \
  ""

#define TRACE_ERASE_SECTOR \
  "dr11 = 80400000\n"      \
  "clk 2 00000001 on\n"    \
  "W 400aaa 0071\n"        \
  "W 400aaa 0070\n"        \
  "R 400000 0080\n"        \
  "W 000aaa 0060\n"        \
  "W 000554 0060\n"        \
  "W 400080 0060\n"        \
  "W 400aaa 0080\n"        \
  "W 400554 0030\n"        \
  "W 000aaa 0060\n"        \
  "W 000554 0060\n"        \
  "W 400000 0060\n"        \
  "W 400aaa 0070\n"        \
  "R 400000 0000\n"        \
  "clk 2 00000001 off\n"   \
  "clk 2 00000001 on\n"    \
  "W 000aaa 0070\n"        \
  "R 000000 0001\n"        \
  "clk 2 00000001 off\n"   \
  "clk 2 00000001 on\n"    \
  "W 000aaa 0070\n"        \
  "R 000000 0001\n"        \
  "clk 2 00000001 off\n"   \
  "clk 2 00000001 on\n"    \
  "W 000aaa 0070\n"        \
  "R 000000 0080\n"        \
  "clk 2 00000001 off\n"   \
  "dr11 = 00000000\n"      \
  ""

#define TRACE_ERASE_SUSPEND \
  "dr11 = 80400000\n"       \
  "clk 2 00000001 on\n"     \
  "W 400aaa 0071\n"         \
  "W 400aaa 0070\n"         \
  "R 400000 0080\n"         \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 400080 0060\n"         \
  "W 400aaa 0080\n"         \
  "W 400554 0030\n"         \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 400000 0060\n"         \
  "W 400aaa 0070\n"         \
  "R 400000 0000\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 0001\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 400aaa 0070\n"         \
  "R 400000 0000\n"         \
  "W 400aaa 00b0\n"         \
  "W 400aaa 0070\n"         \
  "R 400000 00c0\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 00c0\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "R 600000 1009\n"         \
  "R 600002 1e17\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 400aaa 0070\n"         \
  "R 400000 00c0\n"         \
  "W 400aaa 0030\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 0001\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 0001\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 0001\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 0080\n"         \
  "clk 2 00000001 off\n"    \
  "dr11 = 00000000\n"       \
  ""

#define TRACE_ERASE_SUSPEND_LATE \
  "dr11 = 80400000\n"            \
  "clk 2 00000001 on\n"          \
  "W 400aaa 0071\n"              \
  "W 400aaa 0070\n"              \
  "R 400000 0080\n"              \
  "W 000aaa 0060\n"              \
  "W 000554 0060\n"              \
  "W 400080 0060\n"              \
  "W 400aaa 0080\n"              \
  "W 400554 0030\n"              \
  "W 000aaa 0060\n"              \
  "W 000554 0060\n"              \
  "W 400000 0060\n"              \
  "W 400aaa 0070\n"              \
  "R 400000 0000\n"              \
  "clk 2 00000001 off\n"         \
  "clk 2 00000001 on\n"          \
  "W 400aaa 0070\n"              \
  "R 400000 0080\n"              \
  "clk 2 00000001 off\n"         \
  "clk 2 00000001 on\n"          \
  "W 400aaa 0070\n"              \
  "R 400000 0080\n"              \
  "clk 2 00000001 off\n"         \
  ""

#define TRACE_INIT                                                         \
  "clk 2 00000001 on\n"                                                    \
  "clk 0 0000001a on\n"                                                    \
  "rst 1\n"                                                                \
  "rst 0\n"                                                                \
  "wait 10us\n"                                                            \
  "rst 1\n"                                                                \
  "wait 30us\n"                                                            \
  "fmc bank off\n"                                                         \
  "fmc bank on\n"                                                          \
  "W 000aaa 00d0\n"                                                        \
  "R 000000 df48\n"                                                        \
  "W 000aaa 00f0\n"                                                        \
  "W 000aaa 00d0\n"                                                        \
  "W 000aaa 0025\n"                                                        \
  "W 000554 0000\n"                                                        \
  "W 000000 df48\n"                                                        \
  "W 000aaa 0029\n"                                                        \
  "W 000aaa 00f0\n"                                                        \
  "fmc bank off\n"                                                         \
  "fmc bank on\n"                                                          \
  "W 000aaa 0070\n"                                                        \
  "R 000000 0080\n"                                                        \
  "clk 2 00000001 off\n"                                                   \
  "clk 2 00000001 on\n"                                                    \
  "W 000aaa 0060\n"                                                        \
  "W 000554 0060\n"                                                        \
  "W 200000 0061\n"                                                        \
  "W 2e0000 0061\n"                                                        \
  "clk 2 00000001 off\n"                                                   \
  "dr11 -> 00000000\n"                                                     \
  "FMC BCR1 000530db BTR1 0ff10511 BWTR1 0fffffff\n"                       \
  "GPIOB MODER a5a5a5a5 OTYPER 0000a525 OSPEEDR a5a5e5a5 PUPDR a5a525a5\n" \
  "GPIOB ODR 0000a5a5 AFRL c5a5a5a5 AFRH a5a5a5a5\n"                       \
  "GPIOD MODER aaaaaa9a OTYPER 00000000 OSPEEDR ffffffff PUPDR 00000000\n" \
  "GPIOD ODR 0000a5a5 AFRL ccccc5cc AFRH cccccccc\n"                       \
  "GPIOE MODER aaaaaaa5 OTYPER 00000001 OSPEEDR fffffff5 PUPDR 00000005\n" \
  "GPIOE ODR 0000a5a5 AFRL cccccca5 AFRH cccccccc\n"                       \
  ""

#define TRACE_INIT_COREDUMP \
  "clk 2 00000001 on\n"     \
  "clk 0 0000001a on\n"     \
  "rst 1\n"                 \
  "rst 0\n"                 \
  "wait 10us\n"             \
  "rst 1\n"                 \
  "wait 30us\n"             \
  "fmc bank off\n"          \
  "fmc bank on\n"           \
  "W 000aaa 00d0\n"         \
  "R 000000 df48\n"         \
  "W 000aaa 00f0\n"         \
  "W 000aaa 00d0\n"         \
  "W 000aaa 0025\n"         \
  "W 000554 0000\n"         \
  "W 000000 df48\n"         \
  "W 000aaa 0029\n"         \
  "W 000aaa 00f0\n"         \
  "fmc bank off\n"          \
  "fmc bank on\n"           \
  "W 000aaa 0070\n"         \
  "R 000000 0080\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 200000 0061\n"         \
  "W 2e0000 0061\n"         \
  "clk 2 00000001 off\n"    \
  ""

#define TRACE_INIT_IGNORED \
  "clk 2 00000001 on\n"    \
  "clk 0 0000001a on\n"    \
  "rst 1\n"                \
  "rst 0\n"                \
  "wait 10us\n"            \
  "rst 1\n"                \
  "wait 30us\n"            \
  "fmc bank off\n"         \
  "fmc bank on\n"          \
  "W 000aaa 00d0\n"        \
  "R 000000 df48\n"        \
  "W 000aaa 00f0\n"        \
  "W 000aaa 00d0\n"        \
  "W 000aaa 0025\n"        \
  "W 000554 0000\n"        \
  "W 000000 df48\n"        \
  "W 000aaa 0029\n"        \
  "W 000aaa 00f0\n"        \
  "fmc bank off\n"         \
  "fmc bank on\n"          \
  "W 000aaa 0070\n"        \
  "R 000000 0080\n"        \
  "clk 2 00000001 off\n"   \
  "clk 2 00000001 on\n"    \
  "W 000aaa 0060\n"        \
  "W 000554 0060\n"        \
  "W 200000 0061\n"        \
  "W 2e0000 0061\n"        \
  "clk 2 00000001 off\n"   \
  "dr11 -> 80200000\n"     \
  "dr11 = 00000000\n"      \
  "clk 2 00000001 on\n"    \
  "clk 0 0000001a on\n"    \
  "rst 1\n"                \
  "rst 0\n"                \
  "wait 10us\n"            \
  "rst 1\n"                \
  "wait 30us\n"            \
  "fmc bank off\n"         \
  "fmc bank on\n"          \
  "W 000aaa 00d0\n"        \
  "R 000000 df48\n"        \
  "W 000aaa 00f0\n"        \
  "W 000aaa 00d0\n"        \
  "W 000aaa 0025\n"        \
  "W 000554 0000\n"        \
  "W 000000 df48\n"        \
  "W 000aaa 0029\n"        \
  "W 000aaa 00f0\n"        \
  "fmc bank off\n"         \
  "fmc bank on\n"          \
  "W 000aaa 0070\n"        \
  "R 000000 0080\n"        \
  "clk 2 00000001 off\n"   \
  "clk 2 00000001 on\n"    \
  "W 000aaa 0060\n"        \
  "W 000554 0060\n"        \
  "W 200000 0061\n"        \
  "W 2e0000 0061\n"        \
  "clk 2 00000001 off\n"   \
  "dr11 -> 81000000\n"     \
  "dr11 = 00000000\n"      \
  ""

#define TRACE_INIT_NO_PROGRESS \
  "clk 2 00000001 on\n"        \
  "clk 0 0000001a on\n"        \
  "rst 1\n"                    \
  "rst 0\n"                    \
  "wait 10us\n"                \
  "rst 1\n"                    \
  "wait 30us\n"                \
  "fmc bank off\n"             \
  "fmc bank on\n"              \
  "W 000aaa 00d0\n"            \
  "R 000000 df48\n"            \
  "W 000aaa 00f0\n"            \
  "W 000aaa 00d0\n"            \
  "W 000aaa 0025\n"            \
  "W 000554 0000\n"            \
  "W 000000 df48\n"            \
  "W 000aaa 0029\n"            \
  "W 000aaa 00f0\n"            \
  "fmc bank off\n"             \
  "fmc bank on\n"              \
  "W 000aaa 0070\n"            \
  "R 000000 0080\n"            \
  "clk 2 00000001 off\n"       \
  "clk 2 00000001 on\n"        \
  "W 000aaa 0060\n"            \
  "W 000554 0060\n"            \
  "W 200000 0061\n"            \
  "W 2e0000 0061\n"            \
  "clk 2 00000001 off\n"       \
  "dr11 -> 0800abcd\n"         \
  ""

#define TRACE_INIT_REPAIR \
  "clk 2 00000001 on\n"   \
  "clk 0 0000001a on\n"   \
  "rst 1\n"               \
  "rst 0\n"               \
  "wait 10us\n"           \
  "rst 1\n"               \
  "wait 30us\n"           \
  "fmc bank off\n"        \
  "fmc bank on\n"         \
  "W 000aaa 00d0\n"       \
  "R 000000 df48\n"       \
  "W 000aaa 00f0\n"       \
  "W 000aaa 00d0\n"       \
  "W 000aaa 0025\n"       \
  "W 000554 0000\n"       \
  "W 000000 df48\n"       \
  "W 000aaa 0029\n"       \
  "W 000aaa 00f0\n"       \
  "fmc bank off\n"        \
  "fmc bank on\n"         \
  "W 000aaa 0070\n"       \
  "R 000000 0080\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 200000 0061\n"       \
  "W 2e0000 0061\n"       \
  "clk 2 00000001 off\n"  \
  "dr11 -> 80420000\n"    \
  "clk 2 00000001 on\n"   \
  "W 420aaa 0071\n"       \
  "W 420aaa 0070\n"       \
  "R 420000 0080\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 420080 0060\n"       \
  "W 420aaa 0080\n"       \
  "W 420554 0030\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 420000 0060\n"       \
  "W 420aaa 0070\n"       \
  "R 420000 0000\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0001\n"       \
  "clk 2 00000001 off\n"  \
  "wait 1000us\n"         \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0001\n"       \
  "clk 2 00000001 off\n"  \
  "wait 1000us\n"         \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0080\n"       \
  "clk 2 00000001 off\n"  \
  "dr11 = 00000000\n"     \
  ""

#define TRACE_LOCKED     \
  "clk 2 00000001 on\n"  \
  "W 2e0aaa 0071\n"      \
  "W 2e0aaa 0070\n"      \
  "R 2e0000 0080\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 2e0080 0060\n"      \
  "W 2e0aaa 0080\n"      \
  "W 2e0554 0030\n"      \
  "W 000aaa 0060\n"      \
  "W 000554 0060\n"      \
  "W 2e0000 0060\n"      \
  "W 2e0aaa 0070\n"      \
  "R 2e0000 0082\n"      \
  "clk 2 00000001 off\n" \
  ""

#define TRACE_POWER_DOWN                                                   \
  "FMC BCR1 000530db BTR1 0ff10511 BWTR1 0fffffff\n"                       \
  "GPIOB MODER a5a5e5a5 OTYPER 0000a525 OSPEEDR a5a5e5a5 PUPDR a5a525a5\n" \
  "GPIOB ODR 0000a5a5 AFRL c5a5a5a5 AFRH a5a5a5a5\n"                       \
  "GPIOD MODER ffffbfdf OTYPER 00000000 OSPEEDR ffffffff PUPDR 00000000\n" \
  "GPIOD ODR 0000a5a5 AFRL ccccc5cc AFRH cccccccc\n"                       \
  "GPIOE MODER fffffff5 OTYPER 00000001 OSPEEDR fffffff5 PUPDR 00000005\n" \
  "GPIOE ODR 0000a5a5 AFRL cccccca5 AFRH cccccccc\n"                       \
  "FMC BCR1 000530db BTR1 0ff10511 BWTR1 0fffffff\n"                       \
  "GPIOB MODER a5a5e5a5 OTYPER 0000a525 OSPEEDR a5a5e5a5 PUPDR a5a525a5\n" \
  "GPIOB ODR 0000a5a5 AFRL c5a5a5a5 AFRH a5a5a5a5\n"                       \
  "GPIOD MODER ffffbfdf OTYPER 00000000 OSPEEDR ffffffff PUPDR 00000000\n" \
  "GPIOD ODR 0000a5a5 AFRL ccccc5cc AFRH cccccccc\n"                       \
  "GPIOE MODER fffffff5 OTYPER 00000001 OSPEEDR fffffff5 PUPDR 00000005\n" \
  "GPIOE ODR 0000a5a5 AFRL cccccca5 AFRH cccccccc\n"                       \
  "clk 2 00000001 on\n"                                                    \
  "FMC BCR1 000530db BTR1 0ff10511 BWTR1 0fffffff\n"                       \
  "GPIOB MODER a5a5e5a5 OTYPER 0000a525 OSPEEDR a5a5e5a5 PUPDR a5a525a5\n" \
  "GPIOB ODR 0000a5a5 AFRL c5a5a5a5 AFRH a5a5a5a5\n"                       \
  "GPIOD MODER ffffbfdf OTYPER 00000000 OSPEEDR ffffffff PUPDR 00000000\n" \
  "GPIOD ODR 0000a5a5 AFRL ccccc5cc AFRH cccccccc\n"                       \
  "GPIOE MODER fffffff5 OTYPER 00000001 OSPEEDR fffffff5 PUPDR 00000005\n" \
  "GPIOE ODR 0000a5a5 AFRL cccccca5 AFRH cccccccc\n"                       \
  "FMC BCR1 000530db BTR1 0ff10511 BWTR1 0fffffff\n"                       \
  "GPIOB MODER a5a5a5a5 OTYPER 0000a525 OSPEEDR a5a5e5a5 PUPDR a5a525a5\n" \
  "GPIOB ODR 0000a5a5 AFRL c5a5a5a5 AFRH a5a5a5a5\n"                       \
  "GPIOD MODER aaaaaa9a OTYPER 00000000 OSPEEDR ffffffff PUPDR 00000000\n" \
  "GPIOD ODR 0000a5a5 AFRL ccccc5cc AFRH cccccccc\n"                       \
  "GPIOE MODER aaaaaaa5 OTYPER 00000001 OSPEEDR fffffff5 PUPDR 00000005\n" \
  "GPIOE ODR 0000a5a5 AFRL cccccca5 AFRH cccccccc\n"                       \
  "clk 2 00000001 off\n"                                                   \
  ""

#define TRACE_PROGRAM_ERROR \
  "clk 2 00000001 on\n"     \
  "W 400aaa 0071\n"         \
  "W 400aaa 0070\n"         \
  "R 400000 0080\n"         \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 400080 0060\n"         \
  "W 400aaa 0025\n"         \
  "W 400554 0001\n"         \
  "W 400000 0201\n"         \
  "W 400002 0403\n"         \
  "W 400aaa 0029\n"         \
  "W 400aaa 0070\n"         \
  "R 400000 0000\n"         \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 400000 0060\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 0090\n"         \
  "clk 2 00000001 off\n"    \
  ""

#define TRACE_READ       \
  "clk 2 00000001 on\n"  \
  "R 010000 0a03\n"      \
  "R 010002 1811\n"      \
  "R 010004 261f\n"      \
  "R 010006 342d\n"      \
  "clk 2 00000001 off\n" \
  "clk 2 00000001 on\n"  \
  "R 010002 1811\n"      \
  "R 010004 261f\n"      \
  "R 010006 342d\n"      \
  "clk 2 00000001 off\n" \
  "clk 2 00000001 on\n"  \
  "R 010004 261f\n"      \
  "R 010006 342d\n"      \
  "R 010008 423b\n"      \
  "clk 2 00000001 off\n" \
  ""

#define TRACE_WRITE_ALIGNED \
  "clk 2 00000001 on\n"     \
  "W 040aaa 0071\n"         \
  "W 040aaa 0070\n"         \
  "R 040000 0080\n"         \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 040080 0060\n"         \
  "W 040aaa 0025\n"         \
  "W 040554 001f\n"         \
  "W 05ffc0 3130\n"         \
  "W 05ffc2 3332\n"         \
  "W 05ffc4 3534\n"         \
  "W 05ffc6 3736\n"         \
  "W 05ffc8 3938\n"         \
  "W 05ffca 3b3a\n"         \
  "W 05ffcc 3d3c\n"         \
  "W 05ffce 3f3e\n"         \
  "W 05ffd0 4140\n"         \
  "W 05ffd2 4342\n"         \
  "W 05ffd4 4544\n"         \
  "W 05ffd6 4746\n"         \
  "W 05ffd8 4948\n"         \
  "W 05ffda 4b4a\n"         \
  "W 05ffdc 4d4c\n"         \
  "W 05ffde 4f4e\n"         \
  "W 05ffe0 5150\n"         \
  "W 05ffe2 5352\n"         \
  "W 05ffe4 5554\n"         \
  "W 05ffe6 5756\n"         \
  "W 05ffe8 5958\n"         \
  "W 05ffea 5b5a\n"         \
  "W 05ffec 5d5c\n"         \
  "W 05ffee 5f5e\n"         \
  "W 05fff0 6160\n"         \
  "W 05fff2 6362\n"         \
  "W 05fff4 6564\n"         \
  "W 05fff6 6766\n"         \
  "W 05fff8 6968\n"         \
  "W 05fffa 6b6a\n"         \
  "W 05fffc 6d6c\n"         \
  "W 05fffe 6f6e\n"         \
  "W 040aaa 0029\n"         \
  "W 040aaa 0070\n"         \
  "R 040000 0000\n"         \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 040000 0060\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 0080\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 060aaa 0071\n"         \
  "W 060aaa 0070\n"         \
  "R 060000 0080\n"         \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 060080 0060\n"         \
  "W 060aaa 0025\n"         \
  "W 060554 001f\n"         \
  "W 060000 7170\n"         \
  "W 060002 7372\n"         \
  "W 060004 7574\n"         \
  "W 060006 7776\n"         \
  "W 060008 7978\n"         \
  "W 06000a 7b7a\n"         \
  "W 06000c 7d7c\n"         \
  "W 06000e 7f7e\n"         \
  "W 060010 8180\n"         \
  "W 060012 8382\n"         \
  "W 060014 8584\n"         \
  "W 060016 8786\n"         \
  "W 060018 8988\n"         \
  "W 06001a 8b8a\n"         \
  "W 06001c 8d8c\n"         \
  "W 06001e 8f8e\n"         \
  "W 060020 9190\n"         \
  "W 060022 9392\n"         \
  "W 060024 9594\n"         \
  "W 060026 9796\n"         \
  "W 060028 9998\n"         \
  "W 06002a 9b9a\n"         \
  "W 06002c 9d9c\n"         \
  "W 06002e 9f9e\n"         \
  "W 060030 a1a0\n"         \
  "W 060032 a3a2\n"         \
  "W 060034 a5a4\n"         \
  "W 060036 a7a6\n"         \
  "W 060038 a9a8\n"         \
  "W 06003a abaa\n"         \
  "W 06003c adac\n"         \
  "W 06003e afae\n"         \
  "W 060aaa 0029\n"         \
  "W 060aaa 0070\n"         \
  "R 060000 0000\n"         \
  "W 000aaa 0060\n"         \
  "W 000554 0060\n"         \
  "W 060000 0060\n"         \
  "clk 2 00000001 off\n"    \
  "clk 2 00000001 on\n"     \
  "W 000aaa 0070\n"         \
  "R 000000 0080\n"         \
  "clk 2 00000001 off\n"    \
  ""

#define TRACE_WRITE_UNALIGNED \
  "clk 2 00000001 on\n"       \
  "W 000aaa 0071\n"           \
  "W 000aaa 0070\n"           \
  "R 000000 0080\n"           \
  "W 000aaa 0060\n"           \
  "W 000554 0060\n"           \
  "W 000080 0060\n"           \
  "W 000aaa 0025\n"           \
  "W 000554 000f\n"           \
  "W 007fe0 80ff\n"           \
  "W 007fe2 8281\n"           \
  "W 007fe4 8483\n"           \
  "W 007fe6 8685\n"           \
  "W 007fe8 8887\n"           \
  "W 007fea 8a89\n"           \
  "W 007fec 8c8b\n"           \
  "W 007fee 8e8d\n"           \
  "W 007ff0 908f\n"           \
  "W 007ff2 9291\n"           \
  "W 007ff4 9493\n"           \
  "W 007ff6 9695\n"           \
  "W 007ff8 9897\n"           \
  "W 007ffa 9a99\n"           \
  "W 007ffc 9c9b\n"           \
  "W 007ffe 9e9d\n"           \
  "W 000aaa 0029\n"           \
  "W 000aaa 0070\n"           \
  "R 000000 0000\n"           \
  "W 000aaa 0060\n"           \
  "W 000554 0060\n"           \
  "W 000000 0060\n"           \
  "clk 2 00000001 off\n"      \
  "clk 2 00000001 on\n"       \
  "W 000aaa 0070\n"           \
  "R 000000 0080\n"           \
  "clk 2 00000001 off\n"      \
  "clk 2 00000001 on\n"       \
  "W 008aaa 0071\n"           \
  "W 008aaa 0070\n"           \
  "R 008000 0080\n"           \
  "W 000aaa 0060\n"           \
  "W 000554 0060\n"           \
  "W 008080 0060\n"           \
  "W 008aaa 0025\n"           \
  "W 008554 0010\n"           \
  "W 008000 a09f\n"           \
  "W 008002 a2a1\n"           \
  "W 008004 a4a3\n"           \
  "W 008006 a6a5\n"           \
  "W 008008 a8a7\n"           \
  "W 00800a aaa9\n"           \
  "W 00800c acab\n"           \
  "W 00800e aead\n"           \
  "W 008010 b0af\n"           \
  "W 008012 b2b1\n"           \
  "W 008014 b4b3\n"           \
  "W 008016 b6b5\n"           \
  "W 008018 b8b7\n"           \
  "W 00801a bab9\n"           \
  "W 00801c bcbb\n"           \
  "W 00801e bebd\n"           \
  "W 008020 c0bf\n"           \
  "W 008aaa 0029\n"           \
  "W 008aaa 0070\n"           \
  "R 008000 0000\n"           \
  "W 000aaa 0060\n"           \
  "W 000554 0060\n"           \
  "W 008000 0060\n"           \
  "clk 2 00000001 off\n"      \
  "clk 2 00000001 on\n"       \
  "W 000aaa 0070\n"           \
  "R 000000 0080\n"           \
  "clk 2 00000001 off\n"      \
  "clk 2 00000001 on\n"       \
  "W 040aaa 0071\n"           \
  "W 040aaa 0070\n"           \
  "R 040000 0080\n"           \
  "W 000aaa 0060\n"           \
  "W 000554 0060\n"           \
  "W 040080 0060\n"           \
  "W 040aaa 0025\n"           \
  "W 040554 0000\n"           \
  "W 05fffe 8180\n"           \
  "W 040aaa 0029\n"           \
  "W 040aaa 0070\n"           \
  "R 040000 0000\n"           \
  "W 000aaa 0060\n"           \
  "W 000554 0060\n"           \
  "W 040000 0060\n"           \
  "clk 2 00000001 off\n"      \
  "clk 2 00000001 on\n"       \
  "W 000aaa 0070\n"           \
  "R 000000 0080\n"           \
  "clk 2 00000001 off\n"      \
  "clk 2 00000001 on\n"       \
  "W 060aaa 0071\n"           \
  "W 060aaa 0070\n"           \
  "R 060000 0080\n"           \
  "W 000aaa 0060\n"           \
  "W 000554 0060\n"           \
  "W 060080 0060\n"           \
  "W 060aaa 0025\n"           \
  "W 060554 0000\n"           \
  "W 060000 ff82\n"           \
  "W 060aaa 0029\n"           \
  "W 060aaa 0070\n"           \
  "R 060000 0000\n"           \
  "W 000aaa 0060\n"           \
  "W 000554 0060\n"           \
  "W 060000 0060\n"           \
  "clk 2 00000001 off\n"      \
  "clk 2 00000001 on\n"       \
  "W 000aaa 0070\n"           \
  "R 000000 0080\n"           \
  "clk 2 00000001 off\n"      \
  ""

#define TRACE_WRITE_PAGES \
  "clk 2 00000001 on\n"   \
  "W 400aaa 0071\n"       \
  "W 400aaa 0070\n"       \
  "R 400000 0080\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 400080 0060\n"       \
  "W 400aaa 0025\n"       \
  "W 400554 0007\n"       \
  "W 400030 1211\n"       \
  "W 400032 1413\n"       \
  "W 400034 1615\n"       \
  "W 400036 1817\n"       \
  "W 400038 1a19\n"       \
  "W 40003a 1c1b\n"       \
  "W 40003c 1e1d\n"       \
  "W 40003e 201f\n"       \
  "W 400aaa 0029\n"       \
  "W 400aaa 0070\n"       \
  "R 400000 0000\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 400000 0060\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0080\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 400aaa 0071\n"       \
  "W 400aaa 0070\n"       \
  "R 400000 0080\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 400080 0060\n"       \
  "W 400aaa 0025\n"       \
  "W 400554 001f\n"       \
  "W 400040 2221\n"       \
  "W 400042 2423\n"       \
  "W 400044 2625\n"       \
  "W 400046 2827\n"       \
  "W 400048 2a29\n"       \
  "W 40004a 2c2b\n"       \
  "W 40004c 2e2d\n"       \
  "W 40004e 302f\n"       \
  "W 400050 3231\n"       \
  "W 400052 3433\n"       \
  "W 400054 3635\n"       \
  "W 400056 3837\n"       \
  "W 400058 3a39\n"       \
  "W 40005a 3c3b\n"       \
  "W 40005c 3e3d\n"       \
  "W 40005e 403f\n"       \
  "W 400060 4241\n"       \
  "W 400062 4443\n"       \
  "W 400064 4645\n"       \
  "W 400066 4847\n"       \
  "W 400068 4a49\n"       \
  "W 40006a 4c4b\n"       \
  "W 40006c 4e4d\n"       \
  "W 40006e 504f\n"       \
  "W 400070 5251\n"       \
  "W 400072 5453\n"       \
  "W 400074 5655\n"       \
  "W 400076 5857\n"       \
  "W 400078 5a59\n"       \
  "W 40007a 5c5b\n"       \
  "W 40007c 5e5d\n"       \
  "W 40007e 605f\n"       \
  "W 400aaa 0029\n"       \
  "W 400aaa 0070\n"       \
  "R 400000 0000\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 400000 0060\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0080\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 400aaa 0071\n"       \
  "W 400aaa 0070\n"       \
  "R 400000 0080\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 400080 0060\n"       \
  "W 400aaa 0025\n"       \
  "W 400554 001f\n"       \
  "W 400080 6261\n"       \
  "W 400082 6463\n"       \
  "W 400084 6665\n"       \
  "W 400086 6867\n"       \
  "W 400088 6a69\n"       \
  "W 40008a 6c6b\n"       \
  "W 40008c 6e6d\n"       \
  "W 40008e 706f\n"       \
  "W 400090 7271\n"       \
  "W 400092 7473\n"       \
  "W 400094 7675\n"       \
  "W 400096 7877\n"       \
  "W 400098 7a79\n"       \
  "W 40009a 7c7b\n"       \
  "W 40009c 7e7d\n"       \
  "W 40009e 807f\n"       \
  "W 4000a0 8281\n"       \
  "W 4000a2 8483\n"       \
  "W 4000a4 8685\n"       \
  "W 4000a6 8887\n"       \
  "W 4000a8 8a89\n"       \
  "W 4000aa 8c8b\n"       \
  "W 4000ac 8e8d\n"       \
  "W 4000ae 908f\n"       \
  "W 4000b0 9291\n"       \
  "W 4000b2 9493\n"       \
  "W 4000b4 9695\n"       \
  "W 4000b6 9897\n"       \
  "W 4000b8 9a99\n"       \
  "W 4000ba 9c9b\n"       \
  "W 4000bc 9e9d\n"       \
  "W 4000be a09f\n"       \
  "W 400aaa 0029\n"       \
  "W 400aaa 0070\n"       \
  "R 400000 0000\n"       \
  "W 000aaa 0060\n"       \
  "W 000554 0060\n"       \
  "W 400000 0060\n"       \
  "clk 2 00000001 off\n"  \
  "clk 2 00000001 on\n"   \
  "W 000aaa 0070\n"       \
  "R 000000 0080\n"       \
  "clk 2 00000001 off\n"  \
  ""
