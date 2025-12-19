/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
static const ov5640_reginfo_t ov5640_mipi_2lane_24Minput_1280x720_rgb565_le_14fps[] = {
    {0x3103, 0x03},
    {0x3017, 0x00},
    {0x3018, 0x00},
    // {0x3034, (TEST_CSI_COLOR_MODE==MIPI_CSI_RAW10_MODE) ? 0x1A : 0x18},
    {0x3034, 0x18},
    {0x3035, 0x11},
    {0x3036, 0x38},
    {0x3037, 0x11},
    {0x3108, 0x01},
    {0x303D, 0x10},
    {0x303B, 0x19},

    {0x3630, 0x2e},
    {0x3631, 0x0e},
    {0x3632, 0xe2},
    {0x3633, 0x23},
    {0x3621, 0xe0},
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    {0x3905, 0x02},
    {0x3906, 0x10},
    {0x3901, 0x0a},
    {0x3731, 0x02},
    //VCM debug mode
    {0x3600, 0x37},
    {0x3601, 0x33},
    //System control register changing not recommended
    {0x302d, 0x60},
    {0x3620, 0x52},
    {0x371b, 0x20},
    {0x471c, 0x50},
    {0x3a13, 0x43},
    {0x3a18, 0x00},
    {0x3a19, 0xf8},
    {0x3635, 0x13},
    {0x3636, 0x06},
    {0x3634, 0x44},
    {0x3622, 0x01},
    {0x3c01, 0x34},
    {0x3c04, 0x28},
    {0x3c05, 0x98},
    {0x3c06, 0x00},
    {0x3c07, 0x08},
    {0x3c08, 0x00},
    {0x3c09, 0x1c},
    {0x3c0a, 0x9c},
    {0x3c0b, 0x40},
    {0x503d, 0x00},
    {0x3820, 0x46},
    //[7:5]=001 Two lane mode, [4]=0 MIPI HS TX no power down, [3]=0 MIPI LP RX no power down, [2]=1 MIPI enable, [1:0]=10 Debug mode; Default=0x58
    {0x300e, 0x45},
    //[5]=0 Clock free running, [4]=1 Send line short packet, [3]=0 Use lane1 as default, [2]=1 MIPI bus LP11 when no packet; Default=0x04
    {0x4800, CONFIG_CAMERA_OV5640_CSI_LINESYNC_ENABLE ? 0x14 : 0x04},
    {0x302e, 0x08},
    ov5640_settings_mipi_rgb565_le,

    {0x4713, 0x03},
    {0x4407, 0x04},
    {0x440e, 0x00},
    {0x460b, 0x35},
    {0x460c, 0x20},
    {0x3824, 0x01},
    {0x5000, 0x07},
    {0x5001, 0x03},
    // test
    {0x3035, 0x21},
    //[7:0]=40 PLL multiplier
    {0x3036, OV5640_IDI_CLOCK_RATE_1280x720_14FPS / 1000000},
    //[4]=0 PLL root divider /1, [3:0]=5 PLL pre-divider /1.5
    {0x3037, 0x05},
    //[5:4]=01 PCLK root divider /2, [3:2]=00 SCLK2x root divider /1, [1:0]=01 SCLK root divider /2
    {0x3108, 0x11},

    //[3:0]=0 X address start high byte
    {0x3800, (0 >> 8) & 0x0F},
    //[7:0]=0 X address start low byte
    {0x3801, 0 & 0xFF},
    //[2:0]=0 Y address start high byte
    {0x3802, (0 >> 8) & 0x07},
    //[7:0]=0 Y address start low byte
    {0x3803, 0 & 0xFF},

    //[3:0] X address end high byte
    {0x3804, (2623 >> 8) & 0x0F},
    //[7:0] X address end low byte
    {0x3805, 2623 & 0xFF},
    //[2:0] Y address end high byte
    {0x3806, (1951 >> 8) & 0x07},
    //[7:0] Y address end low byte
    {0x3807, 1951 & 0xFF},

    //[3:0]=0 timing hoffset high byte
    {0x3810, (16 >> 8) & 0x0F},
    //[7:0]=0 timing hoffset low byte
    {0x3811, 16 & 0xFF},
    //[2:0]=0 timing voffset high byte
    {0x3812, (4 >> 8) & 0x07},
    //[7:0]=0 timing voffset low byte
    {0x3813, 4 & 0xFF},

    //[3:0] Output horizontal width high byte
    {0x3808, (1280 >> 8) & 0x0F},
    //[7:0] Output horizontal width low byte
    {0x3809, 1280 & 0xFF},
    //[2:0] Output vertical height high byte
    {0x380a, (720 >> 8) & 0x7F},
    //[7:0] Output vertical height low byte
    {0x380b, 720 & 0xFF},

    //HTS line exposure time in # of pixels Tline=HTS/sclk
    {0x380c, (2844 >> 8) & 0x1F},
    {0x380d, 2844 & 0xFF},
    //VTS frame exposure time in # lines
    {0x380e, (1968 >> 8) & 0xFF},
    {0x380f, 1968 & 0xFF},

    //[7:4]=0x1 horizontal odd subsample increment, [3:0]=0x1 horizontal even subsample increment
    {0x3814, 0x11},
    //[7:4]=0x1 vertical odd subsample increment, [3:0]=0x1 vertical even subsample increment
    {0x3815, 0x11},

    //[2]=0 ISP mirror, [1]=0 sensor mirror, [0]=1 horizontal binning
    {0x3821, 0x00},

    //little MIPI shit: global timing unit, period of PCLK in ns * 2(depends on # of lanes)
    {0x4837, (1000000000 / OV5640_IDI_CLOCK_RATE_1280x720_14FPS) * 2}, // 1/40M*2

    //Undocumented anti-green settings
    {0x3618, 0x04}, // Removes vertical lines appearing under bright light
    {0x3612, 0x2b},
    // {0x3708, 0x64},
    {0x3709, 0x12},
    {0x370c, 0x00},

    ov5640_settings_mipi_rgb565_le,
    {0x5001, 0x23},

    // Enable Advanced AWB
    {0x3406, 0x00},
    {0x5192, 0x04},
    {0x5191, 0xf8},
    {0x518d, 0x26},
    {0x518f, 0x42},
    {0x518e, 0x2b},
    {0x5190, 0x42},
    {0x518b, 0xd0},
    {0x518c, 0xbd},
    {0x5187, 0x18},
    {0x5188, 0x18},
    {0x5189, 0x56},
    {0x518a, 0x5c},
    {0x5186, 0x1c},
    {0x5181, 0x50},
    {0x5184, 0x20},
    {0x5182, 0x11},
    {0x5183, 0x00},
    {OV5640_REG_END, 0x00},
};
