/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define TRACE_ADDRESS_NACK \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_BUS_CLEAR \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "sda in\n" \
  "scl out\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "sda released\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "sda out\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "sda 1\n" \
  "wait 10us\n" \
  "scl af\n" \
  "sda af\n" \
  "stop block\n" \
  "i2c S 28w+ 00+ Sr 28r+ <01-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_BUS_INIT \
  "clk i2c1 on\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "irq 31 on\n" \
  "irq 32 on\n" \
  "I2C1 CR1 0000 CR2 0000 OAR1 0000 CCR 0000 TRISE 0000 FLTR 0000\n" \
  "GPIOB MODER 00000280 OTYPER 00000000 OSPEEDR 000000c0 PUPDR 00000100\n" \
  "GPIOB ODR 00000000 AFRL 00000000 AFRH 00000000\n" \
  "GPIOF MODER 00000000 PUPDR 00000000 GPIOG MODER 00000000 PUPDR 00000000\n" \
  "EXTI IMR 00000000 RTSR 00000000 FTSR 00000000 SYSCFG EXTICR2 00000000 EXTICR4 00000000\n" \
  ""

#define TRACE_BUS_STUCK \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "sda in\n" \
  "scl out\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "scl 0\n" \
  "wait 10us\n" \
  "sda out\n" \
  "wait 10us\n" \
  "scl 1\n" \
  "wait 10us\n" \
  "sda 1\n" \
  "wait 10us\n" \
  "scl af\n" \
  "sda af\n" \
  "wait 1us x500\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_BUSY_GLITCH \
  "scl af\n" \
  "busy glitch gone\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 00+ Sr 28r+ <01-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_DATA_NACK \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <cb-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ ca-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_MAG \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 07+ Sr 0er+ <c4-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 10+ 00+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 08+ Sr 0er+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 00+ Sr 0er+ <00+ <01+ <10+ <ff+ <20+ <00+ <30-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 10+ 00+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 08+ Sr 0er+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 00+ Sr 0er+ <00+ <01+ <10+ <ff+ <20+ <00+ <30-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 11+ a0+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "irq 40 on\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 10+ 41+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 00+ Sr 0er+ <08+ <01+ <10+ <ff+ <20+ <00+ <30-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 10+ 00+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 08+ Sr 0er+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 00+ Sr 0er+ <08+ <01+ <10+ <ff+ <20+ <00+ <30-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 10+ 81+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 10+ 00+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 08+ Sr 0er+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 0ew+ 00+ Sr 0er+ <08+ <01+ <10+ <ff+ <20+ <00+ <30-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_PMIC_ABSENT \
  "clk i2c1 on\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "irq 31 on\n" \
  "irq 32 on\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "wait 10000us\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "wait 10000us\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_PMIC_BOOT \
  "clk i2c1 on\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "irq 31 on\n" \
  "irq 32 on\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "I2C1 CR1 0501 CR2 0320 OAR1 4000 CCR 801b TRISE 000a FLTR 0000\n" \
  "GPIOB MODER 00082280 OTYPER 00000240 OSPEEDR 000820c0 PUPDR 00000100\n" \
  "GPIOB ODR 00000000 AFRL 04000000 AFRH 00000040\n" \
  "GPIOF MODER 00000000 PUPDR 00000000 GPIOG MODER 00000000 PUPDR 00004000\n" \
  "EXTI IMR 00000000 RTSR 00000000 FTSR 00000000 SYSCFG EXTICR2 00000000 EXTICR4 00000000\n" \
  "i2c S 28w+ 00+ Sr 28r+ <01-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cd+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <cd-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cc+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <cc-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cd+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cb+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0b+ 65+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0c+ 18+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "irq 23 on\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 07+ 08+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 05+ Sr 28r+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 06+ Sr 28r+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 14+ Sr 28r+ <10-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 14+ 12+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "sleep 3ms\n" \
  "I2C1 CR1 0000 CR2 0000 OAR1 0000 CCR 0000 TRISE 0000 FLTR 0000\n" \
  "GPIOB MODER 00000280 OTYPER 00000000 OSPEEDR 000000c0 PUPDR 00000100\n" \
  "GPIOB ODR 00000000 AFRL 04000000 AFRH 00000040\n" \
  "GPIOF MODER 00000000 PUPDR 00000000 GPIOG MODER 00000000 PUPDR 00004000\n" \
  "EXTI IMR 00000080 RTSR 00000000 FTSR 00000080 SYSCFG EXTICR2 00006000 EXTICR4 00000000\n" \
  ""

#define TRACE_PMIC_BOOT_APB1_24MHZ \
  "clk i2c1 on\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "irq 31 on\n" \
  "irq 32 on\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "I2C1 CR1 0501 CR2 0318 OAR1 4000 CCR 8014 TRISE 0008 FLTR 0000\n" \
  "GPIOB MODER 00082280 OTYPER 00000240 OSPEEDR 000820c0 PUPDR 00000100\n" \
  "GPIOB ODR 00000000 AFRL 04000000 AFRH 00000040\n" \
  "GPIOF MODER 00000000 PUPDR 00000000 GPIOG MODER 00000000 PUPDR 00004000\n" \
  "EXTI IMR 00000000 RTSR 00000000 FTSR 00000000 SYSCFG EXTICR2 00000000 EXTICR4 00000000\n" \
  "i2c S 28w+ 00+ Sr 28r+ <01-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cd+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <cd-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cc+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <cc-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cd+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cb+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0b+ 65+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0c+ 18+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "irq 23 on\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 07+ 08+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 05+ Sr 28r+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 06+ Sr 28r+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 14+ Sr 28r+ <10-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 14+ 12+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "sleep 3ms\n" \
  "I2C1 CR1 0000 CR2 0000 OAR1 0000 CCR 0000 TRISE 0000 FLTR 0000\n" \
  "GPIOB MODER 00000280 OTYPER 00000000 OSPEEDR 000000c0 PUPDR 00000100\n" \
  "GPIOB ODR 00000000 AFRL 04000000 AFRH 00000040\n" \
  "GPIOF MODER 00000000 PUPDR 00000000 GPIOG MODER 00000000 PUPDR 00004000\n" \
  "EXTI IMR 00000080 RTSR 00000000 FTSR 00000080 SYSCFG EXTICR2 00006000 EXTICR4 00000000\n" \
  ""

#define TRACE_PMIC_CHARGING \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 05+ Sr 28r+ <08-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 06+ Sr 28r+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 03+ Sr 28r+ <08-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cd+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <cd-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cc+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <cc-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cd+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cb+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0b+ 65+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0c+ 18+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 02+ Sr 28r+ <03-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 03+ Sr 28r+ <08-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 02+ Sr 28r+ <03-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 03+ Sr 28r+ <08-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 02+ Sr 28r+ <03-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <cb-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ ca+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ Sr 28r+ <ca-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0a+ cb+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 03+ Sr 28r+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 02+ Sr 28r+ <03-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 03+ Sr 28r+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_PMIC_POWER_OFF \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 1f+ b2+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "powered off\n" \
  ""

#define TRACE_PMIC_RAILS_MON \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 16+ Sr 28r+ <14-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 16+ 12+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "sleep 3ms\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 16+ Sr 28r+ <12-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 16+ 10+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 00+ Sr 28r+ <01-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 01+ Sr 28r+ <42-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 0e+ Sr 28r+ <12-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 19+ 11+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "wait 200us\n" \
  "adc 1\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 19+ 00+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 19+ 02+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "wait 200us\n" \
  "adc 1\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 19+ 00+\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""

#define TRACE_TIMEOUT \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w~\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  "scl af\n" \
  "sda af\n" \
  "clk i2c1 on\n" \
  "stop block\n" \
  "i2c S 28w+ 02+ Sr 28r+ <00-\n" \
  "stop release\n" \
  "i2c P\n" \
  "rst i2c1 on\n" \
  "rst i2c1 off\n" \
  "clk i2c1 off\n" \
  "scl in\n" \
  "sda in\n" \
  ""
