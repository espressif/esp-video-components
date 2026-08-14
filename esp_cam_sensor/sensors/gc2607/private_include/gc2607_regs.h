/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * GC2607 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* gc2607 registers */
#define GC2607_REG_ID_HIGH           0x03f0
#define GC2607_REG_ID_LOW            0x03f1

#define GC2607_REG_DELAY             0xfefe

#define GC2607_REG_SHUTTER_TIME_H    0x0202
#define GC2607_REG_SHUTTER_TIME_L    0x0203
#define GC2607_REG_FRAME_LENGTH_H    0x0340
#define GC2607_REG_FRAME_LENGTH_L    0x0341
#define GC2607_REG_LINE_LENGTH_H     0x0342
#define GC2607_REG_LINE_LENGTH_L     0x0343

#define GC2607_REG_BINNING_ROW       0x0218
#define GC2607_REG_BINNING_COL       0x005e

#define GC2607_REG_AGAIN_PGA_GAIN_H  0x02b3
#define GC2607_REG_AGAIN_PGA_GAIN_L  0x02b4
#define GC2607_REG_COL_GAIN_H        0x020c
#define GC2607_REG_COL_GAIN_L        0x020d

#ifdef __cplusplus
}
#endif
