/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SC1346 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SC1346_REG_END      0xffff
#define SC1346_REG_DELAY    0xfffe

/* sc1346 registers */
#define SC1346_REG_SENSOR_ID_H             0x3107
#define SC1346_REG_SENSOR_ID_L             0x3108

#define SC1346_REG_GROUP_HOLD              0x3812

#define SC1346_REG_DIG_COARSE_GAIN         0x3e06
#define SC1346_REG_DIG_FINE_GAIN           0x3e07
#define SC1346_REG_ANG_GAIN                0x3e09

#define SC1346_REG_SHUTTER_TIME_H          0x3e00
#define SC1346_REG_SHUTTER_TIME_M          0x3e01
#define SC1346_REG_SHUTTER_TIME_L          0x3e02

#define SC1346_REG_TOTAL_WIDTH_H           0x320c // HTS,line width
#define SC1346_REG_TOTAL_WIDTH_L           0x320d
#define SC1346_REG_TOTAL_HEIGHT_H          0x320e // VTS,frame height
#define SC1346_REG_TOTAL_HEIGHT_L          0x320f

#define SC1346_REG_OUT_WIDTH_H             0x3208 // width
#define SC1346_REG_OUT_WIDTH_L             0x3209
#define SC1346_REG_OUT_HEIGHT_H            0x320a // height
#define SC1346_REG_OUT_HEIGHT_L            0x320b

#define SC1346_REG_OUT_START_PIXEL_H       0x3210 // start X
#define SC1346_REG_OUT_START_PIXEL_L       0x3211
#define SC1346_REG_OUT_START_LINE_H        0x3212 // start Y
#define SC1346_REG_OUT_START_LINE_L        0x3213

#define SC1346_REG_FLIP_MIRROR             0x3221
#define SC1346_REG_SLEEP_MODE              0x0100
#define SC1346_REG_SOFT_RESET              0x0103

#ifdef __cplusplus
}
#endif
