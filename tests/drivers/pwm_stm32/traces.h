/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define TRACE_BACKLIGHT_AND_VIBE \
  "= backlight 80\n" \
  "stop block\n" \
  "clk tim12 on\n" \
  "tim12 CR1 0080 EGR 0001 CCMR1 0068 CCER 0001 PSC 0002 ARR f423 CCR1 c350\n" \
  "pb14 af 9\n" \
  "pb14 speed 3\n" \
  "pb14 pull down\n" \
  "pb14 af\n" \
  "tim12 CR1 0081\n" \
  "= vibe 100\n" \
  "stop block\n" \
  "clk tim10 on\n" \
  "tim10 CR1 0080 EGR 0001 CCMR1 0068 CCER 0001 ARR 110a CCR1 110b\n" \
  "pb8 af 3\n" \
  "pb8 speed 3\n" \
  "pb8 pull down\n" \
  "pb8 af\n" \
  "tim10 CR1 0081\n" \
  "pf4 level 1\n" \
  "= backlight 40\n" \
  "tim12 CCR1 61a8\n" \
  "= vibe stop\n" \
  "pb8 speed 0\n" \
  "pb8 pull none\n" \
  "pb8 out\n" \
  "tim10 CR1 0080\n" \
  "clk tim10 off\n" \
  "stop release\n" \
  "pf4 level 0\n" \
  "= vibe 100\n" \
  "stop block\n" \
  "clk tim10 on\n" \
  "pb8 speed 3\n" \
  "pb8 pull down\n" \
  "pb8 af\n" \
  "tim10 CR1 0081\n" \
  "pf4 level 1\n" \
  "= backlight 0\n" \
  "pb14 speed 0\n" \
  "pb14 pull none\n" \
  "pb14 out\n" \
  "tim12 CR1 0080\n" \
  "clk tim12 off\n" \
  "stop release\n" \
  "= vibe stop\n" \
  "pb8 speed 0\n" \
  "pb8 pull none\n" \
  "pb8 out\n" \
  "tim10 CR1 0080\n" \
  "clk tim10 off\n" \
  "stop release\n" \
  "pf4 level 0\n" \
  ""

#define TRACE_BACKLIGHT_APB_UNDIVIDED \
  "= backlight 80\n" \
  "stop block\n" \
  "clk tim12 on\n" \
  "tim12 CR1 0080 EGR 0001 CCMR1 0068 CCER 0001 PSC 0005 ARR f423 CCR1 c350\n" \
  "pb14 af 9\n" \
  "pb14 speed 3\n" \
  "pb14 pull down\n" \
  "pb14 af\n" \
  "tim12 CR1 0081\n" \
  "= backlight 0\n" \
  "pb14 speed 0\n" \
  "pb14 pull none\n" \
  "pb14 out\n" \
  "tim12 CR1 0080\n" \
  "clk tim12 off\n" \
  "stop release\n" \
  ""

#define TRACE_BACKLIGHT_EVERY_LEVEL \
  "= backlight 1\n" \
  "stop block\n" \
  "clk tim12 on\n" \
  "tim12 CR1 0080 EGR 0001 CCMR1 0068 CCER 0001 PSC 0002 ARR f423 CCR1 0271\n" \
  "pb14 af 9\n" \
  "pb14 speed 3\n" \
  "pb14 pull down\n" \
  "pb14 af\n" \
  "tim12 CR1 0081\n" \
  "= backlight 2\n" \
  "tim12 CCR1 04e2\n" \
  "= backlight 3\n" \
  "tim12 CCR1 0753\n" \
  "= backlight 4\n" \
  "tim12 CCR1 09c4\n" \
  "= backlight 5\n" \
  "tim12 CCR1 0c35\n" \
  "= backlight 6\n" \
  "tim12 CCR1 0ea6\n" \
  "= backlight 7\n" \
  "tim12 CCR1 1117\n" \
  "= backlight 8\n" \
  "tim12 CCR1 1388\n" \
  "= backlight 9\n" \
  "tim12 CCR1 15f9\n" \
  "= backlight 10\n" \
  "tim12 CCR1 186a\n" \
  "= backlight 11\n" \
  "tim12 CCR1 1adb\n" \
  "= backlight 12\n" \
  "tim12 CCR1 1d4c\n" \
  "= backlight 13\n" \
  "tim12 CCR1 1fbd\n" \
  "= backlight 14\n" \
  "tim12 CCR1 222e\n" \
  "= backlight 15\n" \
  "tim12 CCR1 249f\n" \
  "= backlight 16\n" \
  "tim12 CCR1 2710\n" \
  "= backlight 17\n" \
  "tim12 CCR1 2981\n" \
  "= backlight 18\n" \
  "tim12 CCR1 2bf2\n" \
  "= backlight 19\n" \
  "tim12 CCR1 2e63\n" \
  "= backlight 20\n" \
  "tim12 CCR1 30d4\n" \
  "= backlight 21\n" \
  "tim12 CCR1 3345\n" \
  "= backlight 22\n" \
  "tim12 CCR1 35b6\n" \
  "= backlight 23\n" \
  "tim12 CCR1 3827\n" \
  "= backlight 24\n" \
  "tim12 CCR1 3a98\n" \
  "= backlight 25\n" \
  "tim12 CCR1 3d09\n" \
  "= backlight 26\n" \
  "tim12 CCR1 3f7a\n" \
  "= backlight 27\n" \
  "tim12 CCR1 41eb\n" \
  "= backlight 28\n" \
  "tim12 CCR1 445c\n" \
  "= backlight 29\n" \
  "tim12 CCR1 46cd\n" \
  "= backlight 30\n" \
  "tim12 CCR1 493e\n" \
  "= backlight 31\n" \
  "tim12 CCR1 4baf\n" \
  "= backlight 32\n" \
  "tim12 CCR1 4e20\n" \
  "= backlight 33\n" \
  "tim12 CCR1 5091\n" \
  "= backlight 34\n" \
  "tim12 CCR1 5302\n" \
  "= backlight 35\n" \
  "tim12 CCR1 5573\n" \
  "= backlight 36\n" \
  "tim12 CCR1 57e4\n" \
  "= backlight 37\n" \
  "tim12 CCR1 5a55\n" \
  "= backlight 38\n" \
  "tim12 CCR1 5cc6\n" \
  "= backlight 39\n" \
  "tim12 CCR1 5f37\n" \
  "= backlight 40\n" \
  "tim12 CCR1 61a8\n" \
  "= backlight 41\n" \
  "tim12 CCR1 6419\n" \
  "= backlight 42\n" \
  "tim12 CCR1 668a\n" \
  "= backlight 43\n" \
  "tim12 CCR1 68fb\n" \
  "= backlight 44\n" \
  "tim12 CCR1 6b6c\n" \
  "= backlight 45\n" \
  "tim12 CCR1 6ddd\n" \
  "= backlight 46\n" \
  "tim12 CCR1 704e\n" \
  "= backlight 47\n" \
  "tim12 CCR1 72bf\n" \
  "= backlight 48\n" \
  "tim12 CCR1 7530\n" \
  "= backlight 49\n" \
  "tim12 CCR1 77a1\n" \
  "= backlight 50\n" \
  "tim12 CCR1 7a12\n" \
  "= backlight 51\n" \
  "tim12 CCR1 7c83\n" \
  "= backlight 52\n" \
  "tim12 CCR1 7ef4\n" \
  "= backlight 53\n" \
  "tim12 CCR1 8165\n" \
  "= backlight 54\n" \
  "tim12 CCR1 83d6\n" \
  "= backlight 55\n" \
  "tim12 CCR1 8647\n" \
  "= backlight 56\n" \
  "tim12 CCR1 88b8\n" \
  "= backlight 57\n" \
  "tim12 CCR1 8b29\n" \
  "= backlight 58\n" \
  "tim12 CCR1 8d9a\n" \
  "= backlight 59\n" \
  "tim12 CCR1 900b\n" \
  "= backlight 60\n" \
  "tim12 CCR1 927c\n" \
  "= backlight 61\n" \
  "tim12 CCR1 94ed\n" \
  "= backlight 62\n" \
  "tim12 CCR1 975e\n" \
  "= backlight 63\n" \
  "tim12 CCR1 99cf\n" \
  "= backlight 64\n" \
  "tim12 CCR1 9c40\n" \
  "= backlight 65\n" \
  "tim12 CCR1 9eb1\n" \
  "= backlight 66\n" \
  "tim12 CCR1 a122\n" \
  "= backlight 67\n" \
  "tim12 CCR1 a393\n" \
  "= backlight 68\n" \
  "tim12 CCR1 a604\n" \
  "= backlight 69\n" \
  "tim12 CCR1 a875\n" \
  "= backlight 70\n" \
  "tim12 CCR1 aae6\n" \
  "= backlight 71\n" \
  "tim12 CCR1 ad57\n" \
  "= backlight 72\n" \
  "tim12 CCR1 afc8\n" \
  "= backlight 73\n" \
  "tim12 CCR1 b239\n" \
  "= backlight 74\n" \
  "tim12 CCR1 b4aa\n" \
  "= backlight 75\n" \
  "tim12 CCR1 b71b\n" \
  "= backlight 76\n" \
  "tim12 CCR1 b98c\n" \
  "= backlight 77\n" \
  "tim12 CCR1 bbfd\n" \
  "= backlight 78\n" \
  "tim12 CCR1 be6e\n" \
  "= backlight 79\n" \
  "tim12 CCR1 c0df\n" \
  "= backlight 80\n" \
  "tim12 CCR1 c350\n" \
  "= backlight 81\n" \
  "tim12 CCR1 c5c1\n" \
  "= backlight 82\n" \
  "tim12 CCR1 c832\n" \
  "= backlight 83\n" \
  "tim12 CCR1 caa3\n" \
  "= backlight 84\n" \
  "tim12 CCR1 cd14\n" \
  "= backlight 85\n" \
  "tim12 CCR1 cf85\n" \
  "= backlight 86\n" \
  "tim12 CCR1 d1f6\n" \
  "= backlight 87\n" \
  "tim12 CCR1 d467\n" \
  "= backlight 88\n" \
  "tim12 CCR1 d6d8\n" \
  "= backlight 89\n" \
  "tim12 CCR1 d949\n" \
  "= backlight 90\n" \
  "tim12 CCR1 dbba\n" \
  "= backlight 91\n" \
  "tim12 CCR1 de2b\n" \
  "= backlight 92\n" \
  "tim12 CCR1 e09c\n" \
  "= backlight 93\n" \
  "tim12 CCR1 e30d\n" \
  "= backlight 94\n" \
  "tim12 CCR1 e57e\n" \
  "= backlight 95\n" \
  "tim12 CCR1 e7ef\n" \
  "= backlight 96\n" \
  "tim12 CCR1 ea60\n" \
  "= backlight 97\n" \
  "tim12 CCR1 ecd1\n" \
  "= backlight 98\n" \
  "tim12 CCR1 ef42\n" \
  "= backlight 99\n" \
  "tim12 CCR1 f1b3\n" \
  "= backlight 100\n" \
  "tim12 CCR1 f424\n" \
  "= backlight 0\n" \
  "pb14 speed 0\n" \
  "pb14 pull none\n" \
  "pb14 out\n" \
  "tim12 CR1 0080\n" \
  "clk tim12 off\n" \
  "stop release\n" \
  ""

#define TRACE_BACKLIGHT_FADE \
  "= backlight 76\n" \
  "tim12 CCR1 b98c\n" \
  "= backlight 72\n" \
  "tim12 CCR1 afc8\n" \
  "= backlight 68\n" \
  "tim12 CCR1 a604\n" \
  "= backlight 64\n" \
  "tim12 CCR1 9c40\n" \
  "= backlight 60\n" \
  "tim12 CCR1 927c\n" \
  "= backlight 56\n" \
  "tim12 CCR1 88b8\n" \
  "= backlight 52\n" \
  "tim12 CCR1 7ef4\n" \
  "= backlight 48\n" \
  "tim12 CCR1 7530\n" \
  "= backlight 44\n" \
  "tim12 CCR1 6b6c\n" \
  "= backlight 40\n" \
  "tim12 CCR1 61a8\n" \
  "= backlight 36\n" \
  "tim12 CCR1 57e4\n" \
  "= backlight 32\n" \
  "tim12 CCR1 4e20\n" \
  "= backlight 28\n" \
  "tim12 CCR1 445c\n" \
  "= backlight 24\n" \
  "tim12 CCR1 3a98\n" \
  "= backlight 20\n" \
  "tim12 CCR1 30d4\n" \
  "= backlight 16\n" \
  "tim12 CCR1 2710\n" \
  "= backlight 12\n" \
  "tim12 CCR1 1d4c\n" \
  "= backlight 8\n" \
  "tim12 CCR1 1388\n" \
  "= backlight 4\n" \
  "tim12 CCR1 09c4\n" \
  "= backlight 0\n" \
  "pb14 speed 0\n" \
  "pb14 pull none\n" \
  "pb14 out\n" \
  "tim12 CR1 0080\n" \
  "clk tim12 off\n" \
  "stop release\n" \
  ""

#define TRACE_BACKLIGHT_ON_OFF \
  "= backlight 80\n" \
  "stop block\n" \
  "clk tim12 on\n" \
  "tim12 CR1 0080 EGR 0001 CCMR1 0068 CCER 0001 PSC 0002 ARR f423 CCR1 c350\n" \
  "pb14 af 9\n" \
  "pb14 speed 3\n" \
  "pb14 pull down\n" \
  "pb14 af\n" \
  "tim12 CR1 0081\n" \
  "= backlight 0\n" \
  "pb14 speed 0\n" \
  "pb14 pull none\n" \
  "pb14 out\n" \
  "tim12 CR1 0080\n" \
  "clk tim12 off\n" \
  "stop release\n" \
  "= backlight 0\n" \
  "= backlight 100\n" \
  "stop block\n" \
  "clk tim12 on\n" \
  "tim12 CCR1 f424\n" \
  "pb14 speed 3\n" \
  "pb14 pull down\n" \
  "pb14 af\n" \
  "tim12 CR1 0081\n" \
  "= backlight 0\n" \
  "pb14 speed 0\n" \
  "pb14 pull none\n" \
  "pb14 out\n" \
  "tim12 CR1 0080\n" \
  "clk tim12 off\n" \
  "stop release\n" \
  ""

#define TRACE_INIT \
  "pb14 out\n" \
  "pf4 out\n" \
  "pb8 out\n" \
  ""

#define TRACE_PERIOD_TOO_SHORT \
  ""

#define TRACE_VIBE_CRITICAL_BATTERY \
  "= vibe 100 x2\n" \
  "stop block\n" \
  "clk tim10 on\n" \
  "tim10 CR1 0080 EGR 0001 CCMR1 0068 CCER 0001 ARR 110a CCR1 110b\n" \
  "pb8 af 3\n" \
  "pb8 speed 3\n" \
  "pb8 pull down\n" \
  "pb8 af\n" \
  "tim10 CR1 0081\n" \
  "pf4 level 1\n" \
  "= vibe 100\n" \
  "pb8 speed 0\n" \
  "pb8 pull none\n" \
  "pb8 out\n" \
  "tim10 CR1 0080\n" \
  "clk tim10 off\n" \
  "stop release\n" \
  "pf4 level 0\n" \
  ""

#define TRACE_VIBE_EVERY_STRENGTH \
  "= vibe -100\n" \
  "stop block\n" \
  "clk tim10 on\n" \
  "tim10 CR1 0080 EGR 0001 CCMR1 0068 CCER 0001 ARR 110a\n" \
  "pb8 af 3\n" \
  "pb8 speed 3\n" \
  "pb8 pull down\n" \
  "pb8 af\n" \
  "tim10 CR1 0081\n" \
  "pf4 level 1\n" \
  "= vibe -99\n" \
  "tim10 CCR1 002b\n" \
  "= vibe -98\n" \
  "= vibe -97\n" \
  "tim10 CCR1 0057\n" \
  "= vibe -96\n" \
  "= vibe -95\n" \
  "tim10 CCR1 0082\n" \
  "= vibe -94\n" \
  "= vibe -93\n" \
  "tim10 CCR1 00ae\n" \
  "= vibe -92\n" \
  "= vibe -91\n" \
  "tim10 CCR1 00da\n" \
  "= vibe -90\n" \
  "= vibe -89\n" \
  "tim10 CCR1 0105\n" \
  "= vibe -88\n" \
  "= vibe -87\n" \
  "tim10 CCR1 0131\n" \
  "= vibe -86\n" \
  "= vibe -85\n" \
  "tim10 CCR1 015d\n" \
  "= vibe -84\n" \
  "= vibe -83\n" \
  "tim10 CCR1 0188\n" \
  "= vibe -82\n" \
  "= vibe -81\n" \
  "tim10 CCR1 01b4\n" \
  "= vibe -80\n" \
  "= vibe -79\n" \
  "tim10 CCR1 01e0\n" \
  "= vibe -78\n" \
  "= vibe -77\n" \
  "tim10 CCR1 020b\n" \
  "= vibe -76\n" \
  "= vibe -75\n" \
  "tim10 CCR1 0237\n" \
  "= vibe -74\n" \
  "= vibe -73\n" \
  "tim10 CCR1 0262\n" \
  "= vibe -72\n" \
  "= vibe -71\n" \
  "tim10 CCR1 028e\n" \
  "= vibe -70\n" \
  "= vibe -69\n" \
  "tim10 CCR1 02ba\n" \
  "= vibe -68\n" \
  "= vibe -67\n" \
  "tim10 CCR1 02e5\n" \
  "= vibe -66\n" \
  "= vibe -65\n" \
  "tim10 CCR1 0311\n" \
  "= vibe -64\n" \
  "= vibe -63\n" \
  "tim10 CCR1 033d\n" \
  "= vibe -62\n" \
  "= vibe -61\n" \
  "tim10 CCR1 0368\n" \
  "= vibe -60\n" \
  "= vibe -59\n" \
  "tim10 CCR1 0394\n" \
  "= vibe -58\n" \
  "= vibe -57\n" \
  "tim10 CCR1 03c0\n" \
  "= vibe -56\n" \
  "= vibe -55\n" \
  "tim10 CCR1 03eb\n" \
  "= vibe -54\n" \
  "= vibe -53\n" \
  "tim10 CCR1 0417\n" \
  "= vibe -52\n" \
  "= vibe -51\n" \
  "tim10 CCR1 0442\n" \
  "= vibe -50\n" \
  "= vibe -49\n" \
  "tim10 CCR1 046e\n" \
  "= vibe -48\n" \
  "= vibe -47\n" \
  "tim10 CCR1 049a\n" \
  "= vibe -46\n" \
  "= vibe -45\n" \
  "tim10 CCR1 04c5\n" \
  "= vibe -44\n" \
  "= vibe -43\n" \
  "tim10 CCR1 04f1\n" \
  "= vibe -42\n" \
  "= vibe -41\n" \
  "tim10 CCR1 051d\n" \
  "= vibe -40\n" \
  "= vibe -39\n" \
  "tim10 CCR1 0548\n" \
  "= vibe -38\n" \
  "= vibe -37\n" \
  "tim10 CCR1 0574\n" \
  "= vibe -36\n" \
  "= vibe -35\n" \
  "tim10 CCR1 05a0\n" \
  "= vibe -34\n" \
  "= vibe -33\n" \
  "tim10 CCR1 05cb\n" \
  "= vibe -32\n" \
  "= vibe -31\n" \
  "tim10 CCR1 05f7\n" \
  "= vibe -30\n" \
  "= vibe -29\n" \
  "tim10 CCR1 0622\n" \
  "= vibe -28\n" \
  "= vibe -27\n" \
  "tim10 CCR1 064e\n" \
  "= vibe -26\n" \
  "= vibe -25\n" \
  "tim10 CCR1 067a\n" \
  "= vibe -24\n" \
  "= vibe -23\n" \
  "tim10 CCR1 06a5\n" \
  "= vibe -22\n" \
  "= vibe -21\n" \
  "tim10 CCR1 06d1\n" \
  "= vibe -20\n" \
  "= vibe -19\n" \
  "tim10 CCR1 06fd\n" \
  "= vibe -18\n" \
  "= vibe -17\n" \
  "tim10 CCR1 0728\n" \
  "= vibe -16\n" \
  "= vibe -15\n" \
  "tim10 CCR1 0754\n" \
  "= vibe -14\n" \
  "= vibe -13\n" \
  "tim10 CCR1 0780\n" \
  "= vibe -12\n" \
  "= vibe -11\n" \
  "tim10 CCR1 07ab\n" \
  "= vibe -10\n" \
  "= vibe -9\n" \
  "tim10 CCR1 07d7\n" \
  "= vibe -8\n" \
  "= vibe -7\n" \
  "tim10 CCR1 0802\n" \
  "= vibe -6\n" \
  "= vibe -5\n" \
  "tim10 CCR1 082e\n" \
  "= vibe -4\n" \
  "= vibe -3\n" \
  "tim10 CCR1 085a\n" \
  "= vibe -2\n" \
  "= vibe -1\n" \
  "tim10 CCR1 0885\n" \
  "= vibe 0\n" \
  "= vibe 1\n" \
  "= vibe 2\n" \
  "tim10 CCR1 08b1\n" \
  "= vibe 3\n" \
  "= vibe 4\n" \
  "tim10 CCR1 08dd\n" \
  "= vibe 5\n" \
  "= vibe 6\n" \
  "tim10 CCR1 0908\n" \
  "= vibe 7\n" \
  "= vibe 8\n" \
  "tim10 CCR1 0934\n" \
  "= vibe 9\n" \
  "= vibe 10\n" \
  "tim10 CCR1 0960\n" \
  "= vibe 11\n" \
  "= vibe 12\n" \
  "tim10 CCR1 098b\n" \
  "= vibe 13\n" \
  "= vibe 14\n" \
  "tim10 CCR1 09b7\n" \
  "= vibe 15\n" \
  "= vibe 16\n" \
  "tim10 CCR1 09e2\n" \
  "= vibe 17\n" \
  "= vibe 18\n" \
  "tim10 CCR1 0a0e\n" \
  "= vibe 19\n" \
  "= vibe 20\n" \
  "tim10 CCR1 0a3a\n" \
  "= vibe 21\n" \
  "= vibe 22\n" \
  "tim10 CCR1 0a65\n" \
  "= vibe 23\n" \
  "= vibe 24\n" \
  "tim10 CCR1 0a91\n" \
  "= vibe 25\n" \
  "= vibe 26\n" \
  "tim10 CCR1 0abd\n" \
  "= vibe 27\n" \
  "= vibe 28\n" \
  "tim10 CCR1 0ae8\n" \
  "= vibe 29\n" \
  "= vibe 30\n" \
  "tim10 CCR1 0b14\n" \
  "= vibe 31\n" \
  "= vibe 32\n" \
  "tim10 CCR1 0b40\n" \
  "= vibe 33\n" \
  "= vibe 34\n" \
  "tim10 CCR1 0b6b\n" \
  "= vibe 35\n" \
  "= vibe 36\n" \
  "tim10 CCR1 0b97\n" \
  "= vibe 37\n" \
  "= vibe 38\n" \
  "tim10 CCR1 0bc2\n" \
  "= vibe 39\n" \
  "= vibe 40\n" \
  "tim10 CCR1 0bee\n" \
  "= vibe 41\n" \
  "= vibe 42\n" \
  "tim10 CCR1 0c1a\n" \
  "= vibe 43\n" \
  "= vibe 44\n" \
  "tim10 CCR1 0c45\n" \
  "= vibe 45\n" \
  "= vibe 46\n" \
  "tim10 CCR1 0c71\n" \
  "= vibe 47\n" \
  "= vibe 48\n" \
  "tim10 CCR1 0c9d\n" \
  "= vibe 49\n" \
  "= vibe 50\n" \
  "tim10 CCR1 0cc8\n" \
  "= vibe 51\n" \
  "= vibe 52\n" \
  "tim10 CCR1 0cf4\n" \
  "= vibe 53\n" \
  "= vibe 54\n" \
  "tim10 CCR1 0d20\n" \
  "= vibe 55\n" \
  "= vibe 56\n" \
  "tim10 CCR1 0d4b\n" \
  "= vibe 57\n" \
  "= vibe 58\n" \
  "tim10 CCR1 0d77\n" \
  "= vibe 59\n" \
  "= vibe 60\n" \
  "tim10 CCR1 0da2\n" \
  "= vibe 61\n" \
  "= vibe 62\n" \
  "tim10 CCR1 0dce\n" \
  "= vibe 63\n" \
  "= vibe 64\n" \
  "tim10 CCR1 0dfa\n" \
  "= vibe 65\n" \
  "= vibe 66\n" \
  "tim10 CCR1 0e25\n" \
  "= vibe 67\n" \
  "= vibe 68\n" \
  "tim10 CCR1 0e51\n" \
  "= vibe 69\n" \
  "= vibe 70\n" \
  "tim10 CCR1 0e7d\n" \
  "= vibe 71\n" \
  "= vibe 72\n" \
  "tim10 CCR1 0ea8\n" \
  "= vibe 73\n" \
  "= vibe 74\n" \
  "tim10 CCR1 0ed4\n" \
  "= vibe 75\n" \
  "= vibe 76\n" \
  "tim10 CCR1 0f00\n" \
  "= vibe 77\n" \
  "= vibe 78\n" \
  "tim10 CCR1 0f2b\n" \
  "= vibe 79\n" \
  "= vibe 80\n" \
  "tim10 CCR1 0f57\n" \
  "= vibe 81\n" \
  "= vibe 82\n" \
  "tim10 CCR1 0f82\n" \
  "= vibe 83\n" \
  "= vibe 84\n" \
  "tim10 CCR1 0fae\n" \
  "= vibe 85\n" \
  "= vibe 86\n" \
  "tim10 CCR1 0fda\n" \
  "= vibe 87\n" \
  "= vibe 88\n" \
  "tim10 CCR1 1005\n" \
  "= vibe 89\n" \
  "= vibe 90\n" \
  "tim10 CCR1 1031\n" \
  "= vibe 91\n" \
  "= vibe 92\n" \
  "tim10 CCR1 105d\n" \
  "= vibe 93\n" \
  "= vibe 94\n" \
  "tim10 CCR1 1088\n" \
  "= vibe 95\n" \
  "= vibe 96\n" \
  "tim10 CCR1 10b4\n" \
  "= vibe 97\n" \
  "= vibe 98\n" \
  "tim10 CCR1 10df\n" \
  "= vibe 99\n" \
  "= vibe 100\n" \
  "tim10 CCR1 110b\n" \
  "= vibe stop\n" \
  "pb8 speed 0\n" \
  "pb8 pull none\n" \
  "pb8 out\n" \
  "tim10 CR1 0080\n" \
  "clk tim10 off\n" \
  "stop release\n" \
  "pf4 level 0\n" \
  ""

#define TRACE_VIBE_ON_OFF \
  "= vibe 100\n" \
  "stop block\n" \
  "clk tim10 on\n" \
  "tim10 CR1 0080 EGR 0001 CCMR1 0068 CCER 0001 ARR 110a CCR1 110b\n" \
  "pb8 af 3\n" \
  "pb8 speed 3\n" \
  "pb8 pull down\n" \
  "pb8 af\n" \
  "tim10 CR1 0081\n" \
  "pf4 level 1\n" \
  "= vibe stop\n" \
  "pb8 speed 0\n" \
  "pb8 pull none\n" \
  "pb8 out\n" \
  "tim10 CR1 0080\n" \
  "clk tim10 off\n" \
  "stop release\n" \
  "pf4 level 0\n" \
  "= vibe stop\n" \
  "= vibe 50\n" \
  "stop block\n" \
  "clk tim10 on\n" \
  "tim10 CCR1 0cc8\n" \
  "pb8 speed 3\n" \
  "pb8 pull down\n" \
  "pb8 af\n" \
  "tim10 CR1 0081\n" \
  "pf4 level 1\n" \
  "= vibe -100\n" \
  "tim10 CCR1 0000\n" \
  "= vibe stop\n" \
  "pb8 speed 0\n" \
  "pb8 pull none\n" \
  "pb8 out\n" \
  "tim10 CR1 0080\n" \
  "clk tim10 off\n" \
  "stop release\n" \
  "pf4 level 0\n" \
  "= vibe 0\n" \
  "stop block\n" \
  "clk tim10 on\n" \
  "tim10 CCR1 0885\n" \
  "pb8 speed 3\n" \
  "pb8 pull down\n" \
  "pb8 af\n" \
  "tim10 CR1 0081\n" \
  "pf4 level 1\n" \
  "= vibe stop\n" \
  "pb8 speed 0\n" \
  "pb8 pull none\n" \
  "pb8 out\n" \
  "tim10 CR1 0080\n" \
  "clk tim10 off\n" \
  "stop release\n" \
  "pf4 level 0\n" \
  ""
