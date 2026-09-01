/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************
Copyright (C), 2026, MetaSilicon Tech. Co., Ltd.
All rights reserved.
****************************************************/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MIT245_REG_END                       0xffff
#define MIT245_REG_DELAY                     0xfffe

#define MIT245_REG_SENSOR_ID_H               0x0000
#define MIT245_REG_SENSOR_ID_L               0x0001
#define MIT245_REG_SOFT_RESET                0x0103
#define MIT245_REG_GROUP_HOLD                0x0104
#define MIT245_REG_SLEEP_MODE                0x0100
#define MIT245_REG_FLIP_MIRROR               0x0101

#define MIT245_REG_SHUTTER_TIME_H            0x0202
#define MIT245_REG_SHUTTER_TIME_L            0x0203

#define MIT245_REG_ANG_GAIN_H                0x0204
#define MIT245_REG_ANG_GAIN_L                0x0205
#define MIT245_REG_DIG_GAIN_H                0x020e
#define MIT245_REG_DIG_GAIN_L                0x020f

#define MIT245_REG_TOTAL_HEIGHT_H            0x0340
#define MIT245_REG_TOTAL_HEIGHT_L            0x0341
#define MIT245_REG_TOTAL_WIDTH_H             0x0342
#define MIT245_REG_TOTAL_WIDTH_L             0x0343

#define MIT245_REG_OUT_START_PIXEL_H         0x0405
#define MIT245_REG_OUT_START_PIXEL_L         0x0404
#define MIT245_REG_OUT_START_LINE_H          0x0407
#define MIT245_REG_OUT_START_LINE_L          0x0406
#define MIT245_REG_OUT_WIDTH_H               0x0409
#define MIT245_REG_OUT_WIDTH_L               0x0408
#define MIT245_REG_OUT_HEIGHT_H              0x040b
#define MIT245_REG_OUT_HEIGHT_L              0x040a

#define MIT245_REG_TEST_PATTERN_ENABLE       0x0446
#define MIT245_REG_TEST_PATTERN_MODE         0x0447

#ifdef __cplusplus
}
#endif
