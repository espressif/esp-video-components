/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t reg;
    uint32_t val;
} pivariety_reginfo_t;

typedef enum {
    PIVARIETY_CTRL_MIN,
    PIVARIETY_CTRL_MAX,
    PIVARIETY_CTRL_STEP,
    PIVARIETY_CTRL_DEF,
    PIVARIETY_CTRL_VALUE,
} pivariety_ctrltype_t;

typedef enum {
    PIVARIETY_RAW8         = 0x2A,
    PIVARIETY_RAW10        = 0x2B,
    PIVARIETY_RAW12        = 0x2C,
    PIVARIETY_YUV420_8BIT  = 0x18,
    PIVARIETY_YUV420_10BIT = 0x19,
    PIVARIETY_YUV422_8BIT  = 0X1E,
    PIVARIETY_JPEG         = 0X30,
} pivariety_pixtype_t;

typedef enum {
    PIVARIETY_BAYER_ORDER_BGGR = 0,
    PIVARIETY_BAYER_ORDER_GBRG,
    PIVARIETY_BAYER_ORDER_GRBG,
    PIVARIETY_BAYER_ORDER_RGGB,
    PIVARIETY_BAYER_ORDER_GRAY,
} pivariety_bayerorder_t;

#ifdef __cplusplus
}
#endif
