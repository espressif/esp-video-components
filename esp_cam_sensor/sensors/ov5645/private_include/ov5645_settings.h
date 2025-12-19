/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <sdkconfig.h>
#include "ov5645_regs.h"
#include "ov5645_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV5645_SOFT_POWER_DOWN_EN                 (0x42)
#define OV5645_SOFT_POWER_DOWN_DIS                (0x02)
#define OV5645_OUTPUT_ENABLE_DEFAULT              (0)
#define OV5645_IDI_CLOCK_RATE_640x480_24FPS       (104000000ULL)
#define OV5645_IDI_CLOCK_RATE_1280x960_30FPS      (112000000ULL)
#define OV5645_IDI_CLOCK_RATE_2592x1944_15FPS     (84000000ULL)
#define OV5645_IDI_CLOCK_RATE_1920x1080_15FPS     (84000000ULL)
// Note, all clock configurations default to using 2 data lane mode, so use bitwidth divide by 2
#define OV5645_LINE_RATE_8BITS_1280x960_30FPS     (OV5645_IDI_CLOCK_RATE_1280x960_30FPS * 4)
// Note, for mipi rgb565\yuv422\yuv420 use 16bit trans len for each pixel
#define OV5645_LINE_RATE_16BITS_640x480_24FPS     (OV5645_IDI_CLOCK_RATE_640x480_24FPS * 8)
#define OV5645_LINE_RATE_16BITS_1280x960_30FPS    (OV5645_IDI_CLOCK_RATE_1280x960_30FPS * 8)
#define OV5645_LINE_RATE_16BITS_1920x1080_15FPS   (OV5645_IDI_CLOCK_RATE_1920x1080_15FPS * 8)
#define OV5645_LINE_RATE_16BITS_2592x1944_15FPS   (OV5645_IDI_CLOCK_RATE_2592x1944_15FPS * 8)
#define OV5645_LINE_RATE_24BITS_1280x960_30FPS    (OV5645_IDI_CLOCK_RATE_1280x960_30FPS * 12)

#define ov5645_settings_raw8 \
    {FORMAT_CTRL0, 0x00}, \
    {FORMAT_MUX_CTRL, 0x03}

#define ov5645_settings_rgb565_le \
    {FORMAT_CTRL0, 0x6F}, \
    {FORMAT_MUX_CTRL, 0x01}

#define ov5645_settings_yuv422_uyvy \
    {FORMAT_CTRL0, 0x31}, \
    {FORMAT_MUX_CTRL, 0x00}

#define ov5645_settings_yuv420 \
    {FORMAT_CTRL0, 0x5F}, \
    {FORMAT_MUX_CTRL, 0x00}

/* Note, YUV444/RGB888 not available for full resolution(2592x1964) */
#define ov5645_settings_rgb888 \
    {FORMAT_CTRL0, 0x25}, \
    {FORMAT_MUX_CTRL, 0x00}

static const ov5645_reginfo_t ov5645_mipi_reset_regs[] = {
    {0x3103, 0x11},
    // Comment this out if want AF to work
    {0x3008, 0x82},
    // Ensure streaming off to make clock lane go into LP-11 state.
    {0x4800, 0x05},
    {OV5645_REG_DELAY, 0x10},
    {0x3008, OV5645_SOFT_POWER_DOWN_EN}, // bit[6]=1: software power down default
    {OV5645_REG_END, 0x00},
};

static const ov5645_reginfo_t ov5645_mipi_stream_on[] = {
    {0x3008, OV5645_SOFT_POWER_DOWN_DIS},
    {0x4202, 0x00},
    {OV5645_REG_END, 0x00},
};

static const ov5645_reginfo_t ov5645_mipi_stream_off[] = {
    {0x4202, 0x0f}, /* Sensor enter LP11*/
    {0x3008, OV5645_SOFT_POWER_DOWN_EN},
    {OV5645_REG_END, 0x00},
};

#include "ov5645_mipi_2lane_24Minput_1280x960_rgb565_le_30fps.h"

#include "ov5645_mipi_2lane_24Minput_1280x960_yuv420_30fps.h"

#include "ov5645_mipi_2lane_24Minput_640x480_yuv422_uyvy_24fps.h"

#include "ov5645_mipi_2lane_24Minput_1280x960_yuv422_uyvy_30fps.h"

#include "ov5645_mipi_2lane_24Minput_1920x1080_yuv422_uyvy_15fps.h"

#include "ov5645_mipi_2lane_24Minput_2592x1944_yuv422_uyvy_15fps.h"

#ifdef __cplusplus
}
#endif
