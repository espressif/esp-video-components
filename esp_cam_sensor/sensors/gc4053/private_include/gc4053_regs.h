/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * GC4053 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define GC4053_REG_TOTAL_GAIN_H         0x0807
#define GC4053_REG_TOTAL_GAIN_L         0x0808
#define GC4053_REG_LANE_EN              0x0100
#define GC4053_REG_SHUTTER_TIME_H       0x0202
#define GC4053_REG_SHUTTER_TIME_L       0x0203
#define GC4053_REG_END                  0xff
#define GC4053_REG_DELAY                0xbbbb
#define GC4053_REG_CHIP_ID_HIGH         0x03f0
#define GC4053_REG_CHIP_ID_LOW          0x03f1

#ifdef __cplusplus
}
#endif
