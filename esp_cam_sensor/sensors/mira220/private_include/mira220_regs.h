/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * mira220 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// #define mira220_REG_END      0xffff
// #define mira220_REG_DELAY    0xfffe

/* mira220 registers */
#define mira220_REG_SENSOR_ID_L             0x102B
#define mira220_REG_SENSOR_ID_H             0x102C

// #define mira220_REG_GROUP_HOLD              0x3812
// #define mira220_REG_GROUP_HOLD_DELAY        0x3802


#define mira220_REG_EXP_L                   0x100C
#define mira220_REG_EXP_H                   0x100D

#define mira220_REG_FLIP_MIRROR             0x209C
#define mira220_REG_MODE                    0x1003
#define mira220_REG_START                   0x10F0

#ifdef __cplusplus
}
#endif
