/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * JXH63P register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* jxh63p registers */
#define JXH63P_REG_DELAY               0xf9
#define JXH63P_REG_CHIP_ID_H           0x0a
#define JXH63P_REG_CHIP_ID_L           0x0b
#define JXH63P_REG_CTRL_MODE           0x12
#define JXH63P_REG_LONG_EXPO_HIGH_REG  0x02    /* Exposure Bits 8-15 */
#define JXH63P_REG_LONG_EXPO_LOW_REG   0x01    /* Exposure Bits 0-7 */
#define JXH63P_REG_TEST_PATTERN        0x0c
#define JXH63P_REG_VTS_H               0x23
#define JXH63P_REG_VTS_L               0x22
#define JXH63P_REG_AEC_PK_LONG_GAIN    0x00 /* Bits 0 - 7 */

#ifdef __cplusplus
}
#endif
