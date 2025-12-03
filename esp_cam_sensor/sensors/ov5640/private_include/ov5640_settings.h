/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <sdkconfig.h>
#include "ov5640_regs.h"
#include "ov5640_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV5640_SOFT_POWER_DOWN_EN                 (0x42)
#define OV5640_SOFT_POWER_DOWN_DIS                (0x02)
#define OV5640_OUTPUT_ENABLE_DEFAULT              (0)
#define OV5640_IDI_CLOCK_RATE_1280x720_14FPS      (80000000ULL)
// Note, for mipi rgb565\yuv422\yuv420 use 16bit trans len for each pixel
#define OV5640_LINE_RATE_16BITS_1280x720_14FPS    (OV5640_IDI_CLOCK_RATE_1280x720_14FPS * 8)

#define ov5640_settings_raw8 \
    {FORMAT_CTRL0, 0x00}, \
    {FORMAT_MUX_CTRL, 0x03}, \

#define ov5640_settings_yuv420 \
    {FORMAT_CTRL0, 0x5F}, \
    {FORMAT_MUX_CTRL, 0x00}, \

// send in [Cb Y Cr Y] order.
#define ov5640_settings_yuv422_uyvy \
    {FORMAT_CTRL0, 0x32}, \
    {FORMAT_MUX_CTRL, 0x00}

// send in [R[4:0],G[5:3],G[2:0],B[4:0]] order.
#define ov5640_settings_dvp_rgb565_le \
    {FORMAT_CTRL0, 0x61}, \
    {FORMAT_MUX_CTRL, 0x01}

// send in [R[4:0],G[5:3],G[2:0],B[4:0]] order.
#define ov5640_settings_mipi_rgb565_le \
    {FORMAT_CTRL0, 0x6f}, \
    {FORMAT_MUX_CTRL, 0x01}

// send in [Y Cb Y Cr] order.
#define ov5640_settings_yuv422_yuyv \
    {FORMAT_CTRL0, 0x30}, \
    {FORMAT_MUX_CTRL, 0x00}

// send in [G[2:0],B[4:0],R[4:0],G[5:3]] order.
#define ov5640_settings_rgb565_be \
    {FORMAT_CTRL0, 0x6F}, \
    {FORMAT_MUX_CTRL, 0x01}

/* Note, YUV444/RGB888 not available for full resolution(2592x1964) */
#define ov5640_settings_rgb888 \
    {FORMAT_CTRL0, 0x25}, \
    {FORMAT_MUX_CTRL, 0x00}

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
static const ov5640_reginfo_t ov5640_mipi_reset_regs[] = {
    {0x3103, 0x11},
    // Comment this out if want AF to work
    {0x3008, 0x82},
    // Ensure streaming off to make clock lane go into LP-11 state.
    {0x4800, 0x05},
    {OV5640_REG_DELAY, 0x10},
    {0x3008, OV5640_SOFT_POWER_DOWN_EN}, // bit[6]=1: software power down default
    {OV5640_REG_END, 0x00},
};

#include "ov5640_mipi_2lane_24Minput_1280x720_rgb565_le_14fps.h"
#endif

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
static const ov5640_reginfo_t ov5640_dvp_reset_regs[] = {
    {0x3103, 0x11},
    {0x3008, 0x82},
    {OV5640_REG_DELAY, 0x05},
    {0x3008, OV5640_SOFT_POWER_DOWN_EN},
    {OV5640_REG_END, 0x00},
};

#include "ov5640_dvp_8bit_24Minput_800x600_rgb565_be_10fps.h"
#include "ov5640_dvp_8bit_24Minput_800x600_rgb565_le_10fps.h"

#include "ov5640_dvp_8bit_24Minput_800x600_yuv422_yuyv_10fps.h"
#include "ov5640_dvp_8bit_24Minput_800x600_yuv422_uyvy_10fps.h"

#endif

#ifdef __cplusplus
}
#endif
