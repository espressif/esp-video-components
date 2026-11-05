/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define OV02C10_REG_GROUP_HOLD              0x3208
#define OV02C10_REG_GROUP_HOLD_DELAY        0x3800

#define OV02C10_REG_DIG_COARSE_GAIN         0x3509
#define OV02C10_REG_DIG_FINE_GAIN_H         0x350b
#define OV02C10_REG_DIG_FINE_GAIN_L         0x350c    
#define OV02C10_REG_ANG_COARSE_GAIN         0x3508
#define OV02C10_REG_ANG_FINE_GAIN           0x3509

#define OV02C10_REG_SHUTTER_TIME_M          0x3501
#define OV02C10_REG_SHUTTER_TIME_L          0x3502

#define OV02C10_REG_DELAY            0xeeee
#define OV02C10_REG_END              0xffff
#define OV02C10_REG_SENSOR_ID_H      0x300a
#define OV02C10_REG_SENSOR_ID_L      0x300b
#define OV02C10_REG_SLEEP_MODE       0x0100
#define OV02C10_REG_MIPI_CTRL00      0x4800
#define OV02C10_REG_FRAME_OFF_NUMBER 0x4202
#define OV5640_REG_PAD_OUT          0x300d


#ifdef __cplusplus
}
#endif
