/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * MIRA220 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MIRA220_REG_END      0xffff
#define MIRA220_REG_DELAY    0xfffe

/* MIRA220 registers */
#define MIRA220_REG_SENSOR_ID_L             0x102B
#define MIRA220_REG_SENSOR_ID_H             0x102C

#define MIRA220_REG_EXP_L                   0x100C
#define MIRA220_REG_EXP_H                   0x100D

#define MIRA220_REG_HFLIP                   0x209C
#define MIRA220_REG_VFLIP                   0x1095
#define MIRA220_REG_MODE                    0x1003
#define MIRA220_REG_START                   0x10F0
#define MIRA220_REG_VSIZE_L                 0x1087
#define MIRA220_REG_VSIZE_H                 0x1088
#define MIRA220_REG_VBLANK_L                0x1012
#define MIRA220_REG_VBLANK_H                0x1013
/* OTP control */
#define MIRA220_REG_OTP_CMD_REG             0x0080
#define MIRA220_REG_OTP_CMD_UP              0x4
#define MIRA220_REG_OTP_CMD_DOWN            0x8

#ifdef __cplusplus
}
#endif
