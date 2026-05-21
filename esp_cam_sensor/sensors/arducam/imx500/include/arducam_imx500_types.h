/*
 * SPDX-FileCopyrightText: 2026 Arducam Electronic Technology (Nanjing) CO LTD
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
} imx500_reginfo_t;

typedef enum {
    IMX500_CTRL_MIN,
    IMX500_CTRL_MAX,
    IMX500_CTRL_STEP,
    IMX500_CTRL_DEF,
    IMX500_CTRL_VALUE,
} imx500_ctrltype_t;

typedef enum {
    IMX500_RAW8         = 0x2A,
    IMX500_RAW10        = 0x2B,
    IMX500_RAW12        = 0x2C,
    IMX500_YUV420_8BIT  = 0x18,
    IMX500_YUV420_10BIT = 0x19,
    IMX500_YUV422_8BIT  = 0x1E,
    IMX500_JPEG         = 0x30,
} imx500_pixtype_t;

typedef enum {
    IMX500_BAYER_ORDER_BGGR = 0,
    IMX500_BAYER_ORDER_GBRG,
    IMX500_BAYER_ORDER_GRBG,
    IMX500_BAYER_ORDER_RGGB,
    IMX500_BAYER_ORDER_GRAY,
} imx500_bayerorder_t;

#ifdef __cplusplus
}
#endif
