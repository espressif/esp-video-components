/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
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
#define OV5645_IDI_CLOCK_RATE_1280x960_30FPS      (28000000ULL)
// Note, all clock configurations default to using 2 data lane mode, so use bitwidth divide by 2
#define OV5645_LINE_RATE_8BITS_1280x960_30FPS     (OV5645_IDI_CLOCK_RATE_1280x960_30FPS * 4)
// Note, for mipi rgb565\yuv422\yuv420 use 16bit trans len for each pixel
#define OV5645_LINE_RATE_16BITS_1280x960_30FPS    (OV5645_IDI_CLOCK_RATE_1280x960_30FPS * 8)

#define ov5645_settings_raw8 \
    {FORMAT_CTRL0, 0x00}, \
    {FORMAT_MUX_CTRL, 0x03}, \

#define ov5645_settings_rgb565 \
    {FORMAT_CTRL0, 0x6F}, \
    {FORMAT_MUX_CTRL, 0x01}, \

#define ov5645_settings_yuv422 \
    {FORMAT_CTRL0, 0x30}, \
    {FORMAT_MUX_CTRL, 0x00}, \

#define ov5645_settings_yuv420 \
    {FORMAT_CTRL0, 0x5F}, \
    {FORMAT_MUX_CTRL, 0x00}, \

/* Note, YUV444/RGB888 not available for full resolution(2592x1964) */
#define ov5645_settings_rgb888 \
    {FORMAT_CTRL0, 0x25}, \
    {FORMAT_MUX_CTRL, 0x00}, \

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

static const ov5645_reginfo_t ov5645_MIPI_2lane_yuv422_960p_30fps[] = {
    /* MIPI_2lane_SXGA(YUV422&RGB565) 1280x960,30fps */
    /* MIPI data rate is 448 Mbps/lane */
    ov5645_settings_yuv422
    {0x5001, 0x83},
    {0x3103, 0x03},
    {0x3503, 0x07},
    {0x3002, 0x1c},
    {0x3006, 0xc3},
    {0x300e, 0x45}, /* MIPI 2 lane */
    //[5]=0 Clock free running, [4]=1 Send line short packet, [3]=0 Use lane1 as default, [2]=1 MIPI bus LP11 when no packet; Default=0x04
    {0x4800, CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE ? 0x14 : 0x04},
    {0x3017, 0x40},
    {0x3018, 0x00},
    {0x302e, 0x0b},
    {0x3037, 0x13},
    {0x3108, 0x01},
    {0x3611, 0x06},
    {0x3612, 0xab},
    {0x3614, 0x50},
    {0x3618, 0x04},
    {0x3034, 0x18},
    {0x3035, 0x21},
    {0x3036, OV5645_IDI_CLOCK_RATE_1280x960_30FPS / 1000000},
    {0x3500, 0x00},
    {0x3501, 0x01},
    {0x3502, 0x00},
    {0x350a, 0x00},
    {0x350b, 0x3f},
    {0x3600, 0x09},
    {0x3601, 0x43},
    {0x3620, 0x33},
    {0x3621, 0xe0},
    {0x3622, 0x01},
    {0x3630, 0x2d},
    {0x3631, 0x00},
    {0x3632, 0x32},
    {0x3633, 0x52},
    {0x3634, 0x70},
    {0x3635, 0x13},
    {0x3636, 0x03},
    {0x3702, 0x6e},
    {0x3703, 0x52},
    {0x3704, 0xa0},
    {0x3705, 0x33},
    {0x3708, 0x66},
    {0x3709, 0x12},
    {0x370b, 0x61},
    {0x370c, 0xc3},
    {0x370f, 0x10},
    {0x3715, 0x08},
    {0x3717, 0x01},
    {0x371b, 0x20},
    {0x3731, 0x22},
    {0x3739, 0x70},
    {0x3901, 0x0a},
    {0x3905, 0x02},
    {0x3906, 0x10},
    {0x3719, 0x86},
    {0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0x06},
    {0x3804, 0x0a},
    {0x3805, 0x3f},
    {0x3806, 0x07},
    {0x3807, 0x9d},
    {0x3808, 0x05},
    {0x3809, 0x00},
    {0x380a, 0x03},
    {0x380b, 0xc0},
    {0x380c, 0x07}, /*linelength = 0x768=1896*/
    {0x380d, 0x68},
    {0x380e, 0x03}, /*framelength = 0x3d8=984*/
    {0x380f, 0xd8},
    {0x3810, 0x00},
    {0x3811, 0x10},
    {0x3812, 0x00},
    {0x3813, 0x06},
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3820, 0x41},
    {0x3821, 0x07},
    {0x3824, 0x01},
    {0x3826, 0x03},
    {0x3828, 0x08},
    {0x3a02, 0x03},
    {0x3a03, 0xd8},
    {0x3a08, 0x01},
    {0x3a09, 0xf8},
    {0x3a0a, 0x01},
    {0x3a0b, 0xa4},
    {0x3a0e, 0x02},
    {0x3a0d, 0x02},
    {0x3a14, 0x03},
    {0x3a15, 0xd8},
    {0x3a18, 0x00},
    {0x3a19, 0xf8},
    {0x3c01, 0x34},
    {0x3c04, 0x28},
    {0x3c05, 0x98},
    {0x3c07, 0x07},
    {0x3c09, 0xc2},
    {0x3c0a, 0x9c},
    {0x3c0b, 0x40},
    {0x3c01, 0x34},
    {0x4001, 0x02},
    {0x4004, 0x02},
    {0x4005, 0x18},
    {0x4050, 0x6e},
    {0x4051, 0x8f},
    {0x4514, 0xbb},
    {0x4520, 0xb0},
    {0x460b, 0x37},
    {0x460c, 0x20},
    {0x4818, 0x01},
    {0x481d, 0xf0},
    {0x481f, 0x50},
    {0x4823, 0x70},
    {0x4831, 0x14},
    //little MIPI shit: global timing unit, period of PCLK in ns * 2(depends on # of lanes) notice the settle delay.
    {0x4837, (1000000000 / OV5645_IDI_CLOCK_RATE_1280x960_30FPS) * 2},
    {0x5000, 0xa7},
    {0x501d, 0x00},
    {0x503d, 0x00},
    {0x505c, 0x30},
    {0x5181, 0x59},
    {0x5183, 0x00},
    {0x5191, 0xf0},
    {0x5192, 0x03},
    {0x5684, 0x10},
    {0x5685, 0xa0},
    {0x5686, 0x0c},
    {0x5687, 0x78},
    {0x5a00, 0x08},
    {0x5a21, 0x00},
    {0x5a24, 0x00},
    {0x3503, 0x00},
    {0x5180, 0xff},
    {0x5181, 0xf2},
    {0x5182, 0x00},
    {0x5183, 0x14},
    {0x5184, 0x25},
    {0x5185, 0x24},
    {0x5186, 0x09},
    {0x5187, 0x09},
    {0x5188, 0x0a},
    {0x5189, 0x75},
    {0x518a, 0x52},
    {0x518b, 0xea},
    {0x518c, 0xa8},
    {0x518d, 0x42},
    {0x518e, 0x38},
    {0x518f, 0x56},
    {0x5190, 0x42},
    {0x5191, 0xf8},
    {0x5192, 0x04},
    {0x5193, 0x70},
    {0x5194, 0xf0},
    {0x5195, 0xf0},
    {0x5196, 0x03},
    {0x5197, 0x01},
    {0x5198, 0x04},
    {0x5199, 0x12},
    {0x519a, 0x04},
    {0x519b, 0x00},
    {0x519c, 0x06},
    {0x519d, 0x82},
    {0x519e, 0x38},
    {0x5381, 0x1e},
    {0x5382, 0x5b},
    {0x5383, 0x08},
    {0x5384, 0x0b},
    {0x5385, 0x84},
    {0x5386, 0x8f},
    {0x5387, 0x82},
    {0x5388, 0x71},
    {0x5389, 0x11},
    {0x538a, 0x01},
    {0x538b, 0x98},
    {0x5300, 0x08},
    {0x5301, 0x1e},
    {0x5302, 0x10},
    {0x5303, 0x00},
    {0x5304, 0x08},
    {0x5305, 0x1e},
    {0x5306, 0x08},
    {0x5307, 0x16},
    {0x5309, 0x08},
    {0x530a, 0x1e},
    {0x530b, 0x04},
    {0x530c, 0x06},
    {0x5480, 0x01},
    {0x5481, 0x0e},
    {0x5482, 0x18},
    {0x5483, 0x2b},
    {0x5484, 0x52},
    {0x5485, 0x65},
    {0x5486, 0x71},
    {0x5487, 0x7d},
    {0x5488, 0x87},
    {0x5489, 0x91},
    {0x548a, 0x9a},
    {0x548b, 0xaa},
    {0x548c, 0xb8},
    {0x548d, 0xcd},
    {0x548e, 0xdd},
    {0x548f, 0xea},
    {0x5490, 0x1d},
    {0x5580, 0x02},
    {0x5583, 0x40},
    {0x5584, 0x30},
    {0x5589, 0x10},
    {0x558a, 0x00},
    {0x558b, 0xf8},
    {0x5780, 0xfc},
    {0x5781, 0x13},
    {0x5782, 0x03},
    {0x5786, 0x20},
    {0x5787, 0x40},
    {0x5788, 0x08},
    {0x5789, 0x08},
    {0x578a, 0x02},
    {0x578b, 0x01},
    {0x578c, 0x01},
    {0x578d, 0x0c},
    {0x578e, 0x02},
    {0x578f, 0x01},
    {0x5790, 0x01},
    {0x5800, 0x3f},
    {0x5801, 0x16},
    {0x5802, 0x0e},
    {0x5803, 0x0d},
    {0x5804, 0x17},
    {0x5805, 0x3f},
    {0x5806, 0x0b},
    {0x5807, 0x06},
    {0x5808, 0x04},
    {0x5809, 0x04},
    {0x580a, 0x06},
    {0x580b, 0x0b},
    {0x580c, 0x09},
    {0x580d, 0x03},
    {0x580e, 0x00},
    {0x580f, 0x00},
    {0x5810, 0x03},
    {0x5811, 0x08},
    {0x5812, 0x0a},
    {0x5813, 0x03},
    {0x5814, 0x00},
    {0x5815, 0x00},
    {0x5816, 0x04},
    {0x5817, 0x09},
    {0x5818, 0x0f},
    {0x5819, 0x08},
    {0x581a, 0x06},
    {0x581b, 0x06},
    {0x581c, 0x08},
    {0x581d, 0x0c},
    {0x581e, 0x3f},
    {0x581f, 0x1e},
    {0x5820, 0x12},
    {0x5821, 0x13},
    {0x5822, 0x21},
    {0x5823, 0x3f},
    {0x5824, 0x68},
    {0x5825, 0x28},
    {0x5826, 0x2c},
    {0x5827, 0x28},
    {0x5828, 0x08},
    {0x5829, 0x48},
    {0x582a, 0x64},
    {0x582b, 0x62},
    {0x582c, 0x64},
    {0x582d, 0x28},
    {0x582e, 0x46},
    {0x582f, 0x62},
    {0x5830, 0x60},
    {0x5831, 0x62},
    {0x5832, 0x26},
    {0x5833, 0x48},
    {0x5834, 0x66},
    {0x5835, 0x44},
    {0x5836, 0x64},
    {0x5837, 0x28},
    {0x5838, 0x66},
    {0x5839, 0x48},
    {0x583a, 0x2c},
    {0x583b, 0x28},
    {0x583c, 0x26},
    {0x583d, 0xae},
    {0x5025, 0x00},
    {0x3a0f, 0x38},
    {0x3a10, 0x30},
    {0x3a11, 0x70},
    {0x3a1b, 0x38},
    {0x3a1e, 0x30},
    {0x3a1f, 0x18},
#if OV5645_OUTPUT_ENABLE_DEFAULT
    {0x3008, OV5645_SOFT_POWER_DOWN_DIS}, // wake up from software standby(power down)
#endif
    {OV5645_REG_END, 0x00},
};

#ifdef __cplusplus
}
#endif
