/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "sdkconfig.h"
#include "ov5647_regs.h"

#define BIT(nr)                                    (1UL << (nr))
#define OV5647_MIPI_CSI_LINESYNC_SUPPORT           CONFIG_MIPI_CSI_LINESYNC_SUPPORT
#define OV5647_IDI_CLOCK_RATE_800x800_50FPS        (100000000ULL)
#define OV5647_MIPI_CSI_LINE_RATE_800x800_50FPS    (OV5647_IDI_CLOCK_RATE_800x800_50FPS * 4)
#define OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS      (74000000ULL)
#define OV5647_MIPI_CSI_LINE_RATE_720P_50FPS       (OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS * 4)
#define OV5647_8BIT_MODE                           (0x18)
#define OV5647_10BIT_MODE                          (0x1A)

typedef struct {
    uint16_t reg;
    uint8_t val;
} reginfo_t;

static reginfo_t ov5647_reset_data[] = {
    {0x0100, 0x00}, // enable sleep
    {0x0103, 0x01},
    {OV5647_REG_DELAY, 0x10},
    {0x4800, BIT(0)},// Ensure streaming off to make `clock lane` go into LP-11 state.
    {OV5647_REG_END, 0x00},
};

static reginfo_t init_reglist_input_24M_MIPI_2lane_raw8_800x800_30fps[] = {
    {0x3034, OV5647_8BIT_MODE}, // set RAW format
    {0x3035, 0x41}, // system clk div
    {0x3036, ((OV5647_IDI_CLOCK_RATE_800x800_50FPS * 8 * 4) / 25000000)},
    {0x303c, 0x11},
    {0x3106, 0xf5},
    {0x3821, 0x01},
    {0x3820, 0x41},
    {0x3827, 0xec},
    {0x370c, 0x0f},
    {0x3612, 0x59},
    {0x3618, 0x00},
    {0x5000, 0x06},
    {0x5002, 0x41},
    {0x5003, 0x08},
    {0x5a00, 0x08},
    {0x3000, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x00},
    {0x3016, 0x08},
    {0x3017, 0xe0},
    {0x3018, 0x44},
    {0x301c, 0xf8},
    {0x301d, 0xf0},
    {0x3a18, 0x00},
    {0x3a19, 0xf8},
    {0x3c01, 0x80},
    {0x3b07, 0x0c},
    //HTS line exposure time in # of pixels
    {0x380c, (1896 >> 8) & 0x1F},
    {0x380d, 1896 & 0xFF},
    //VTS frame exposure time in # lines
    {0x380e, (984 >> 8) & 0xFF},
    {0x380f, 984 & 0xFF},
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3708, 0x64},
    {0x3709, 0x52},
    //[3:0]=0 X address start high byte
    {0x3800, (500 >> 8) & 0x0F},
    //[7:0]=0 X address start low byte
    {0x3801, 500 & 0xFF},
    //[2:0]=0 Y address start high byte
    {0x3802, (0 >> 8) & 0x07},
    //[7:0]=0 Y address start low byte
    {0x3803, 0 & 0xFF},
    //[3:0] X address end high byte
    {0x3804, ((2624 - 1) >> 8) & 0x0F},
    //[7:0] X address end low byte
    {0x3805, (2624 - 1) & 0xFF},
    //[2:0] Y address end high byte
    {0x3806, ((1954 - 1) >> 8) & 0x07},
    //[7:0] Y address end low byte
    {0x3807, (1954 - 1) & 0xFF},
    //[3:0] Output horizontal width high byte
    {0x3808, (800 >> 8) & 0x0F},
    //[7:0] Output horizontal width low byte
    {0x3809, 800 & 0xFF},
    //[2:0] Output vertical height high byte
    {0x380a, (800 >> 8) & 0x7F},
    //[7:0] Output vertical height low byte
    {0x380b, 800 & 0xFF},
    //[3:0]=0 timing hoffset high byte
    {0x3810, (8 >> 8) & 0x0F},
    //[7:0]=0 timing hoffset low byte
    {0x3811, 8 & 0xFF},
    //[2:0]=0 timing voffset high byte
    {0x3812, (0 >> 8) & 0x07},
    //[7:0]=0 timing voffset low byte
    {0x3813, 0 & 0xFF},
    {0x3630, 0x2e},
    {0x3632, 0xe2},
    {0x3633, 0x23},
    {0x3634, 0x44},
    {0x3636, 0x06},
    {0x3620, 0x64},
    {0x3621, 0xe0},
    {0x3600, 0x37},
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x3731, 0x02},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    {0x3f05, 0x02},
    {0x3f06, 0x10},
    {0x3f01, 0x0a},
    {0x3a08, 0x01},
    {0x3a09, 0x27},
    {0x3a0a, 0x00},
    {0x3a0b, 0xf6},
    {0x3a0d, 0x04},
    {0x3a0e, 0x03},
    {0x3a0f, 0x58},
    {0x3a10, 0x50},
    {0x3a1b, 0x58},
    {0x3a1e, 0x50},
    {0x3a11, 0x60},
    {0x3a1f, 0x28},
    {0x4001, 0x02},
    {0x4004, 0x02},
    {0x4000, 0x09},
    {0x4837, (1000000000 / OV5647_IDI_CLOCK_RATE_800x800_50FPS) * 2},
    {0x4050, 0x6e},
    {0x4051, 0x8f},
    {0x4800, OV5647_MIPI_CSI_LINESYNC_SUPPORT ? 0x14 : 0x04},

    {0x3002, 0x01}, // AF DW5714 XSD <-> set OV5647 GPIO0 to high
    {0x3010, 0x01},
    {0x300D, 0x01},
    {OV5647_REG_DELAY, 0x10},
    {0x0100, 0x01}, // exit sleep
    {OV5647_REG_END, 0x00},
};

static reginfo_t init_reglist_input_24M_MIPI_2lane_raw8_1280x720_50fps[] = {
    {0x3034, OV5647_8BIT_MODE}, // set RAW format
    {0x3035, 0x41},
    {0x3036, ((OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS * 8 * 4 * 2) / 25000000ULL)},
    {0x303c, 0x11},
    {0x3106, 0xf5},
    {0x3821, 0x07},
    {0x3820, 0x41},
    {0x3827, 0xec},
    {0x370c, 0x0f},
    {0x3612, 0x59},
    {0x3618, 0x00},
    {0x5000, 0x06},
    {0x5002, 0x41},
    {0x5003, 0x08},
    {0x5a00, 0x08},
    {0x3000, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x00},
    {0x3016, 0x08},
    {0x3017, 0xe0},
    {0x3018, 0x44},
    {0x301c, 0xf8},
    {0x301d, 0xf0},
    {0x3a18, 0x00},
    {0x3a19, 0xf8},
    {0x3c01, 0x80},
    {0x3b07, 0x0c},
    //[3:0] Output horizontal width high byte
    {0x3808, (1280 >> 8) & 0x0F},
    //[7:0] Output horizontal width low byte
    {0x3809, 1280 & 0xFF},
    //[2:0] Output vertical height high byte
    {0x380a, (720 >> 8) & 0x7F},
    //[7:0] Output vertical height low byte
    {0x380b, 720 & 0xFF},
    //HTS line exposure time in # of pixels
    {0x380c, (1896 >> 8) & 0x1F},
    {0x380d, 1896 & 0xFF},
    //VTS frame exposure time in # lines
    {0x380e, (984 >> 8) & 0xFF},
    {0x380f, 984 & 0xFF},
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3708, 0x64},
    {0x3709, 0x52},
    //[3:0]=0 X address start high byte
    {0x3800, (0 >> 8) & 0x0F},
    //[7:0]=0 X address start low byte
    {0x3801, 0 & 0xFF},
    //[2:0]=0 Y address start high byte
    {0x3802, (0 >> 8) & 0x07},
    //[7:0]=0 Y address start low byte
    {0x3803, 0 & 0xFF},
    //[3:0] X address end high byte
    {0x3804, ((2624 - 1) >> 8) & 0x0F},
    //[7:0] X address end low byte
    {0x3805, (2624 - 1) & 0xFF},
    //[2:0] Y address end high byte
    {0x3806, ((1954 - 1) >> 8) & 0x07},
    //[7:0] Y address end low byte
    {0x3807, (1954 - 1) & 0xFF},
    //[3:0]=0 timing hoffset high byte
    {0x3810, (8 >> 8) & 0x0F},
    //[7:0]=0 timing hoffset low byte
    {0x3811, 8 & 0xFF},
    //[2:0]=0 timing voffset high byte
    {0x3812, (0 >> 8) & 0x07},
    //[7:0]=0 timing voffset low byte
    {0x3813, 0 & 0xFF},
    {0x3630, 0x2e},
    {0x3632, 0xe2},
    {0x3633, 0x23},
    {0x3634, 0x44},
    {0x3636, 0x06},
    {0x3620, 0x64},
    {0x3621, 0xe0},
    {0x3600, 0x37},
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x3731, 0x02},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    {0x3f05, 0x02},
    {0x3f06, 0x10},
    {0x3f01, 0x0a},
    {0x3a08, 0x01},
    {0x3a09, 0x27},
    {0x3a0a, 0x00},
    {0x3a0b, 0xf6},
    {0x3a0d, 0x04},
    {0x3a0e, 0x03},
    {0x3a0f, 0x58},
    {0x3a10, 0x50},
    {0x3a1b, 0x58},
    {0x3a1e, 0x50},
    {0x3a11, 0x60},
    {0x3a1f, 0x28},
    {0x4001, 0x02},
    {0x4004, 0x02},
    {0x4000, 0x09},
    {0x4837, 1000000000ULL * 2 / OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS},
    {0x4050, 0x6e},
    {0x4051, 0x8f},
    {0x4800, OV5647_MIPI_CSI_LINESYNC_SUPPORT ? 0x14 : 0x04},
    // AF DW5714 XSD <-> set OV5647 GPIO0 to high
    {0x3002, 0x01},
    {0x3010, 0x01},
    {0x300D, 0x01},
    {OV5647_REG_DELAY, 0x10},
    {0x0100, 0x01}, // exit sleep
    {OV5647_REG_END, 0x00},
};

#ifdef __cplusplus
}
#endif
