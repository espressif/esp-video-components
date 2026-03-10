/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * OS04C10 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define OS04C10_REG_END      0xffff
#define OS04C10_REG_DELAY    0xfffe

/* os04c10 registers */
#define OS04C10_REG_CHIP_ID_H              0x300a
#define OS04C10_REG_CHIP_ID_L              0x300b

#define OS04C10_REG_STREAM_ON              0x0100
#define OS04C10_REG_SOFT_RESET             0x0103

#define OS04C10_REG_EXP_H                  0x3501
#define OS04C10_REG_EXP_L                  0x3502

#define OS04C10_REG_ANALOG_GAIN_H          0x3508
#define OS04C10_REG_ANALOG_GAIN_L          0x3509
#define OS04C10_REG_FORMAT1                0x3820
#define OS04C10_REG_TEST_PATTERN           0x5040

#ifdef __cplusplus
}
#endif
