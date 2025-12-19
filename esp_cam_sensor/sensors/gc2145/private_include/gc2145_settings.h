/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "gc2145_regs.h"
#include "gc2145_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GC2145_ANTI_BANDING_REG_SIZE (15)

// Notes that RGB has byte order issues
#define gc2145_settings_rgb565_be \
    {GC2145_REG_RESET_RELATED, 0x00}, \
    {GC2145_REG_P0_OUTPUT_FORMAT, 0x06}

#define GC2145_YUV422_FMT_UYVY 0x00 // send in [Cb Y Cr Y] order.
#define GC2145_YUV422_FMT_YUYV 0x02 // send in [Y Cb Y Cr] order.

// Notes that DVP & MIPI need different YUV seq
#define gc2145_settings_yuv422_uyvy \
    {GC2145_REG_RESET_RELATED, 0x00}, \
    {GC2145_REG_P0_OUTPUT_FORMAT, GC2145_YUV422_FMT_UYVY}

#define gc2145_settings_yuv422_yuyv \
    {GC2145_REG_RESET_RELATED, 0x00}, \
    {GC2145_REG_P0_OUTPUT_FORMAT, GC2145_YUV422_FMT_YUYV}

#define gc2145_settings_yuv420 \
    {GC2145_REG_RESET_RELATED, 0x00}, \
    {GC2145_REG_P0_OUTPUT_FORMAT, 0x13}

#define gc2145_settings_1lane_stream_off \
    {GC2145_REG_RESET_RELATED, 0x03}, \
    {0x10, 0x84}

#define gc2145_settings_1lane_stream_on \
    {GC2145_REG_RESET_RELATED, 0x03}, \
    {0x10, 0x94}

#define gc2145_1lane_enable \
    {GC2145_REG_RESET_RELATED, 0x03}, \
    {0x01, 0x83}, \
    {0x10, 0x94}

#define gc2145_2lane_enable \
    {GC2145_REG_RESET_RELATED, 0x03}, \
    {0x01, 0x87}, \
    {0x10, 0x95}

static const gc2145_reginfo_t gc2145_antibanding[4][GC2145_ANTI_BANDING_REG_SIZE] = {
    {
        {0x05, 0x01},
        {0x06, 0x50},
        {0x07, 0x00},
        {0x08, 0x12},
        {0xfe, 0x01},
        {0x25, 0x00},
        {0x26, 0xfa},
        {0x27, 0x04},
        {0x28, 0xe2},
        {0x29, 0x06},
        {0x2a, 0xd6},
        {0x2b, 0x09},
        {0x2c, 0xc4},
        {0x2d, 0x0e},
        {0x2e, 0xa6},
    }, /*ANTIBANDING OFF*/
    {
        {0x05, 0x01},
        {0x06, 0x50},
        {0x07, 0x00},
        {0x08, 0x12},
        {0xfe, 0x01},
        {0x25, 0x00},
        {0x26, 0xfa},
        {0x27, 0x04},
        {0x28, 0xe2},
        {0x29, 0x06},
        {0x2a, 0xd6},
        {0x2b, 0x09},
        {0x2c, 0xc4},
        {0x2d, 0x0b},
        {0x2e, 0xb8},
    }, /*ANTIBANDING 50HZ*/
    {
        {0x05, 0x01},
        {0x06, 0x52},
        {0x07, 0x00},
        {0x08, 0x32},
        {0xfe, 0x01},
        {0x25, 0x00},
        {0x26, 0xd0},
        {0x27, 0x04},
        {0x28, 0xe0},
        {0x29, 0x07},
        {0x2a, 0x50},
        {0x2b, 0x09},
        {0x2c, 0xc0},
        {0x2d, 0x0d},
        {0x2e, 0xd0},
    }, /*ANTIBANDING 60HZ*/
    {
        {0x05, 0x01},
        {0x06, 0x50},
        {0x07, 0x00},
        {0x08, 0x12},
        {0xfe, 0x01},
        {0x25, 0x00},
        {0x26, 0xfa},
        {0x27, 0x04},
        {0x28, 0xe2},
        {0x29, 0x06},
        {0x2a, 0xd6},
        {0x2b, 0x09},
        {0x2c, 0xc4},
        {0x2d, 0x0d},
        {0x2e, 0xd0},
    }, /*ANTIBANDING AUTO*/
};

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
#include "gc2145_mipi_1lane_24Minput_640x480_rgb565_le_15fps.h"
#include "gc2145_mipi_1lane_24Minput_800x600_rgb565_le_30fps.h"
#include "gc2145_mipi_1lane_24Minput_1600x1200_rgb565_le_7fps.h"
#endif

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
#include "gc2145_dvp_8bit_10Minput_320x240_yuv422_uyvy_13fps.h"
#include "gc2145_dvp_8bit_10Minput_320x240_yuv422_yuyv_13fps.h"
#include "gc2145_dvp_8bit_20Minput_640x480_rgb565_be_15fps_windowing.h"
#include "gc2145_dvp_8bit_20Minput_800x600_rgb565_be_20fps.h"
#include "gc2145_dvp_8bit_20Minput_1600x1200_rgb565_be_13fps.h"
#endif

#ifdef __cplusplus
}
#endif
