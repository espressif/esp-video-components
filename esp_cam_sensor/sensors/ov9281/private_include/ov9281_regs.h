/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * OV9281 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define OV9281_REG_END      0xffff
#define OV9281_REG_DELAY    0xfffe

/* ov9281 registers */
#define OV9281_REG_CHIP_ID_H           0x300a
#define OV9281_REG_CHIP_ID_L           0x300b
#define OV9281_REG_CTRL_MODE           0x0100
#define OV9281_REG_SW_RESET            0x0103
#define OV9281_REG_GROUP_HOLD_ADDR     0x3208
#define OV9281_REG_EXPOSURE_H          0x3500
#define OV9281_REG_EXPOSURE_M          0x3501
#define OV9281_REG_EXPOSURE_L          0x3502
#define OV9281_REG_GAIN_SHIFT_ADDR     0x3507
#define OV9281_REG_GAIN                0x3509
#define OV9281_REG_TEST_PATTERN        0x5e00
#define OV9281_REG_HTS_H               0x380c
#define OV9281_REG_HTS_L               0x380d
#define OV9281_REG_VTS_H               0x380e
#define OV9281_REG_VTS_L               0x380f

/* General register values */
#define OV9281_TEST_PATTERN_ENABLE     0x80
#define OV9281_TEST_PATTERN_DISABLE    0x0
#define OV9281_GAIN_H_MASK             0x07
#define OV9281_GAIN_H_SHIFT            8
#define OV9281_GAIN_L_MASK             0xff
#define OV9281_GAIN_MIN                0x10
#define OV9281_GAIN_MAX                0xf8
#define OV9281_GAIN_STEP               1
#define OV9281_GAIN_DEFAULT            0x18
#define OV9281_EXPOSURE_MIN            4
#define OV9281_EXPOSURE_STEP           1
#define OV9281_VTS_MAX                 0x7fff
#define OV9281_VTS_MIN                 0x0000
#define OV9281_VTS_DEFAULT             0x0000
#define OV9281_VTS_STEP                1
#define OV9281_MODE_SW_STANDBY         0x0
#define OV9281_MODE_STREAMING          0x01
#define OV9281_GROUP_HOLD_START        0x00
#define OV9281_GROUP_HOLD_END          0x10

#if CONFIG_SOC_ISP_BLC_SUPPORTED
#define OV9281_BLC_TARGET_DEFAULT      0x40
#else
#define OV9281_BLC_TARGET_DEFAULT      0x10
#endif

#ifdef __cplusplus
}
#endif
