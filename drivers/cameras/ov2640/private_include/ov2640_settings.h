/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "ov2640_regs.h"

#define OV2640_JPEG_QUALITY_DEFAULT 0x11

enum {
    OV2640_FORMAT_INDEX0,
    OV2640_FORMAT_INDEX1,
    OV2640_FORMAT_INDEX2,
    OV2640_FORMAT_INDEX3,
    OV2640_FORMAT_INDEX4,
    OV2640_FORMAT_INDEX5,
    OV2640_FORMAT_INDEX6,
    OV2640_FORMAT_INDEX7,
};

typedef enum {
    BANK_DSP,    // When register 0xFF=0x00, DSP register bank is available
    BANK_SENSOR, // When register 0xFF=0x01, Sensor register bank is available
    BANK_MAX,
} ov2640_bank_t;

typedef struct {
    uint8_t reg;
    uint8_t val;
} ov2640_reginfo_t;

#define NUM_AE_LEVELS (5)
static const uint8_t ov2640_ae_levels_regs[NUM_AE_LEVELS + 1][3] = {
    { AEW,  AEB,  VV  },
    {0x20, 0x18, 0x60 },
    {0x34, 0x1C, 0x00 },
    {0x3E, 0x38, 0x81 },
    {0x48, 0x40, 0x81 },
    {0x58, 0x50, 0x92 },
};

#define NUM_BRIGHTNESS_LEVELS (5)
static const uint8_t ov2640_brightness_regs[NUM_BRIGHTNESS_LEVELS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA },
    {0x00, 0x04, 0x09, 0x00, 0x00 }, /* -2 */
    {0x00, 0x04, 0x09, 0x10, 0x00 }, /* -1 */
    {0x00, 0x04, 0x09, 0x20, 0x00 }, /*  0 */
    {0x00, 0x04, 0x09, 0x30, 0x00 }, /* +1 */
    {0x00, 0x04, 0x09, 0x40, 0x00 }, /* +2 */
};

#define NUM_CONTRAST_LEVELS (5)
static const uint8_t ov2640_contrast_regs[NUM_CONTRAST_LEVELS + 1][7] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA, BPDATA, BPDATA },
    {0x00, 0x04, 0x07, 0x20, 0x18, 0x34, 0x06 }, /* -2 */
    {0x00, 0x04, 0x07, 0x20, 0x1c, 0x2a, 0x06 }, /* -1 */
    {0x00, 0x04, 0x07, 0x20, 0x20, 0x20, 0x06 }, /*  0 */
    {0x00, 0x04, 0x07, 0x20, 0x24, 0x16, 0x06 }, /* +1 */
    {0x00, 0x04, 0x07, 0x20, 0x28, 0x0c, 0x06 }, /* +2 */
};

#define NUM_SATURATION_LEVELS (5)
static const uint8_t ov2640_saturation_regs[NUM_SATURATION_LEVELS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA },
    {0x00, 0x02, 0x03, 0x28, 0x28 }, /* -2 */
    {0x00, 0x02, 0x03, 0x38, 0x38 }, /* -1 */
    {0x00, 0x02, 0x03, 0x48, 0x48 }, /*  0 */
    {0x00, 0x02, 0x03, 0x58, 0x58 }, /* +1 */
    {0x00, 0x02, 0x03, 0x68, 0x68 }, /* +2 */
};

#define NUM_SPECIAL_EFFECTS (7)
static const uint8_t ov2640_special_effects_regs[NUM_SPECIAL_EFFECTS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA },
    {0x00, 0x00, 0x05, 0x80, 0x80 }, /* no effect */
    {0x00, 0x40, 0x05, 0x80, 0x80 }, /* negative */
    {0x00, 0x18, 0x05, 0x80, 0x80 }, /* black and white */
    {0x00, 0x18, 0x05, 0x40, 0xC0 }, /* reddish */
    {0x00, 0x18, 0x05, 0x40, 0x40 }, /* greenish */
    {0x00, 0x18, 0x05, 0xA0, 0x40 }, /* blue */
    {0x00, 0x18, 0x05, 0x40, 0xA6 }, /* retro */
};

#define NUM_WB_MODES (4)
static const uint8_t ov2640_wb_modes_regs[NUM_WB_MODES + 1][3] = {
    {0xCC, 0xCD, 0xCE },
    {0x5E, 0x41, 0x54 }, /* sunny */
    {0x65, 0x41, 0x4F }, /* cloudy */
    {0x52, 0x41, 0x66 }, /* office */
    {0x42, 0x3F, 0x71 }, /* home */
};

#define ov2640_settings_jpeg3 \
    {BANK_SEL, BANK_DSP}, \
    {RESET, RESET_JPEG | RESET_DVP}, \
    {IMAGE_MODE, IMAGE_MODE_JPEG_EN | IMAGE_MODE_HREF_VSYNC}, \
    {0xD7, 0x03}, \
    {0xE1, 0x77}, \
    {0xE5, 0x1F}, \
    {0xD9, 0x10}, \
    {0xDF, 0x80}, \
    {0x33, 0x80}, \
    {0x3C, 0x10}, \
    {0xEB, 0x30}, \
    {0xDD, 0x7F}, \
    {RESET, 0x00}, \

#define ov2640_settings_yuv422 \
    {BANK_SEL, BANK_DSP}, \
    {RESET, RESET_DVP}, \
    {IMAGE_MODE, IMAGE_MODE_YUV422}, \
    {0xD7, 0x01}, \
    {0xE1, 0x67}, \
    {RESET, 0x00}, \

#define ov2640_settings_rgb565 \
    {BANK_SEL, BANK_DSP}, \
    {RESET, RESET_DVP}, \
    {IMAGE_MODE, IMAGE_MODE_RGB565}, \
    {0xD7, 0x03}, \
    {0xE1, 0x77}, \
    {RESET, 0x00}, \

#define ov2640_settings_to_cif \
    {BANK_SEL, BANK_SENSOR}, \
    {COM7, COM7_RES_CIF}, \
    {COM1, 0x0A}, /*Set the sensor output window*/ \
    {REG32, REG32_CIF}, \
    {HSTART, 0x11}, \
    {HSTOP, 0x43}, \
    {VSTART, 0x00}, \
    {VSTOP, 0x25}, \
    {BD50, 0xca}, \
    {BD60, 0xa8}, \
    {0x5a, 0x23}, \
    {0x6d, 0x00}, \
    {0x3d, 0x38}, \
    {0x39, 0x92}, \
    {0x35, 0xda}, \
    {0x22, 0x1a}, \
    {0x37, 0xc3}, \
    {0x23, 0x00}, \
    {ARCOM2, 0xc0}, \
    {0x06, 0x88}, \
    {0x07, 0xc0}, \
    {COM4, 0x87}, \
    {0x0e, 0x41}, \
    {0x4c, 0x00}, \
    {BANK_SEL, BANK_DSP}, \
    {RESET, RESET_DVP}, \
    {HSIZE8, 0x32}, /*Set the sensor resolution (UXGA, SVGA, CIF)*/ \
    {VSIZE8, 0x25}, \
    {SIZEL, 0x00}, \
    {HSIZE, 0x64}, /*Set the image window size >= output size*/ \
    {VSIZE, 0x4a}, \
    {XOFFL, 0x00}, \
    {YOFFL, 0x00}, \
    {VHYX, 0x00}, \
    {TEST, 0x00}, \
    {CTRL2, CTRL2_DCW_EN | 0x1D}, \
    {CTRLI, CTRLI_LP_DP | 0x00}, \

#define ov2640_settings_to_svga \
    {BANK_SEL, BANK_SENSOR}, \
    {COM7, COM7_RES_SVGA}, \
    {COM1, 0x0A}, \
    {REG32, REG32_SVGA}, \
    {HSTART, 0x11}, \
    {HSTOP, 0x43}, \
    {VSTART, 0x00}, \
    {VSTOP, 0x4b}, \
    {0x37, 0xc0}, \
    {BD50, 0xca}, \
    {BD60, 0xa8}, \
    {0x5a, 0x23}, \
    {0x6d, 0x00}, \
    {0x3d, 0x38}, \
    {0x39, 0x92}, \
    {0x35, 0xda}, \
    {0x22, 0x1a}, \
    {0x37, 0xc3}, \
    {0x23, 0x00}, \
    {ARCOM2, 0xc0}, \
    {0x06, 0x88}, \
    {0x07, 0xc0}, \
    {COM4, 0x87}, \
    {0x0e, 0x41}, \
    {0x42, 0x03}, \
    {0x4c, 0x00}, \
    {BANK_SEL, BANK_DSP}, \
    {RESET, RESET_DVP}, \
    {HSIZE8, 0x64}, \
    {VSIZE8, 0x4B}, \
    {SIZEL, 0x00}, \
    {HSIZE, 0xC8}, \
    {VSIZE, 0x96}, \
    {XOFFL, 0x00}, \
    {YOFFL, 0x00}, \
    {VHYX, 0x00}, \
    {TEST, 0x00}, \
    {CTRL2, CTRL2_DCW_EN | 0x1D}, \
    {CTRLI, CTRLI_LP_DP | 0x00}, \

#define ov2640_settings_to_uxga \
    {BANK_SEL, BANK_SENSOR}, \
    {COM7, COM7_RES_UXGA}, \
    {COM1, 0x0F}, \
    {REG32, REG32_UXGA}, \
    {HSTART, 0x11}, \
    {HSTOP, 0x75}, \
    {VSTART, 0x01}, \
    {VSTOP, 0x97}, \
    {0x3d, 0x34}, \
    {BD50, 0xbb}, \
    {BD60, 0x9c}, \
    {0x5a, 0x57}, \
    {0x6d, 0x80}, \
    {0x39, 0x82}, \
    {0x23, 0x00}, \
    {0x07, 0xc0}, \
    {0x4c, 0x00}, \
    {0x35, 0x88}, \
    {0x22, 0x0a}, \
    {0x37, 0x40}, \
    {ARCOM2, 0xa0}, \
    {0x06, 0x02}, \
    {COM4, 0xb7}, \
    {0x0e, 0x01}, \
    {0x42, 0x83}, \
    {BANK_SEL, BANK_DSP}, \
    {RESET, RESET_DVP}, \
    {HSIZE8, 0xc8}, \
    {VSIZE8, 0x96}, \
    {SIZEL, 0x00}, \
    {HSIZE, 0x90}, /*Set the image window size >= output size*/\
    {VSIZE, 0x2c}, \
    {XOFFL, 0x00}, \
    {YOFFL, 0x00}, \
    {VHYX, 0x88}, \
    {TEST, 0x00}, \
    {CTRL2, CTRL2_DCW_EN | 0x1d}, \
    {CTRLI, 0x00}, \

static const ov2640_reginfo_t ov2640_settings_cif[] = {
    {BANK_SEL, BANK_SENSOR},
    {COM7, COM7_SRST}, // soft reset
    {REG_DELAY, 0x10}, // delay
    {BANK_SEL, BANK_DSP},
    {0x2c, 0xff},
    {0x2e, 0xdf},
    {BANK_SEL, BANK_SENSOR},
    {0x3c, 0x32},
    {CLKRC, 0x01},
    {COM2, COM2_OUT_DRIVE_3x},
    {REG04, REG04_DEFAULT},
    {COM8, COM8_DEFAULT | COM8_BNDF_EN | COM8_AGC_EN | COM8_AEC_EN},
    {COM9, COM9_AGC_SET(COM9_AGC_GAIN_2x)},
    {0x2c, 0x0c},
    {0x33, 0x78},
    {0x3a, 0x33},
    {0x3b, 0xfB},
    {0x3e, 0x00},
    {0x43, 0x11},
    {0x16, 0x10},
    {0x39, 0x92},
    {0x35, 0xda},
    {0x22, 0x1a},
    {0x37, 0xc3},
    {0x23, 0x00},
    {ARCOM2, 0xc0},
    {0x06, 0x88},
    {0x07, 0xc0},
    {COM4, 0x87},
    {0x0e, 0x41},
    {0x4c, 0x00},
    {0x4a, 0x81},
    {0x21, 0x99},
    {AEW, 0x40},
    {AEB, 0x38},
    {VV, VV_AGC_TH_SET(8, 2)},
    {0x5c, 0x00},
    {0x63, 0x00},
    {HISTO_LOW, 0x70},
    {HISTO_HIGH, 0x80},
    {0x7c, 0x05},
    {0x20, 0x80},
    {0x28, 0x30},
    {0x6c, 0x00},
    {0x6d, 0x80},
    {0x6e, 0x00},
    {0x70, 0x02},
    {0x71, 0x94},
    {0x73, 0xc1},
    {0x3d, 0x34},
    {0x5a, 0x57},
    {BD50, 0xbb},
    {BD60, 0x9c},
    {COM7, COM7_RES_CIF},
    {HSTART, 0x11},
    {HSTOP, 0x43},
    {VSTART, 0x00},
    {VSTOP, 0x25},
    {REG32, 0x89},
    {0x37, 0xc0},
    {BD50, 0xca},
    {BD60, 0xa8},
    {0x6d, 0x00},
    {0x3d, 0x38},
    {BANK_SEL, BANK_DSP},
    {0xe5, 0x7f},
    {MC_BIST, MC_BIST_RESET | MC_BIST_BOOT_ROM_SEL},
    {0x41, 0x24},
    {RESET, RESET_JPEG | RESET_DVP},
    {0x76, 0xff},
    {0x33, 0xa0},
    {0x42, 0x20},
    {0x43, 0x18},
    {0x4c, 0x00},
    {CTRL3, CTRL3_WPC_EN | 0x10 },
    {0x88, 0x3f},
    {0xd7, 0x03},
    {0xd9, 0x10},
    {R_DVP_SP, R_DVP_SP_AUTO_MODE | 0x02},
    {0xc8, 0x08},
    {0xc9, 0x80},
    {BPADDR, 0x00},
    {BPDATA, 0x00},
    {BPADDR, 0x03},
    {BPDATA, 0x48},
    {BPDATA, 0x48},
    {BPADDR, 0x08},
    {BPDATA, 0x20},
    {BPDATA, 0x10},
    {BPDATA, 0x0e},
    {0x90, 0x00},
    {0x91, 0x0e},
    {0x91, 0x1a},
    {0x91, 0x31},
    {0x91, 0x5a},
    {0x91, 0x69},
    {0x91, 0x75},
    {0x91, 0x7e},
    {0x91, 0x88},
    {0x91, 0x8f},
    {0x91, 0x96},
    {0x91, 0xa3},
    {0x91, 0xaf},
    {0x91, 0xc4},
    {0x91, 0xd7},
    {0x91, 0xe8},
    {0x91, 0x20},
    {0x92, 0x00},
    {0x93, 0x06},
    {0x93, 0xe3},
    {0x93, 0x05},
    {0x93, 0x05},
    {0x93, 0x00},
    {0x93, 0x04},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x96, 0x00},
    {0x97, 0x08},
    {0x97, 0x19},
    {0x97, 0x02},
    {0x97, 0x0c},
    {0x97, 0x24},
    {0x97, 0x30},
    {0x97, 0x28},
    {0x97, 0x26},
    {0x97, 0x02},
    {0x97, 0x98},
    {0x97, 0x80},
    {0x97, 0x00},
    {0x97, 0x00},
    {0xa4, 0x00},
    {0xa8, 0x00},
    {0xc5, 0x11},
    {0xc6, 0x51},
    {0xbf, 0x80},
    {0xc7, 0x10},
    {0xb6, 0x66},
    {0xb8, 0xA5},
    {0xb7, 0x64},
    {0xb9, 0x7C},
    {0xb3, 0xaf},
    {0xb4, 0x97},
    {0xb5, 0xFF},
    {0xb0, 0xC5},
    {0xb1, 0x94},
    {0xb2, 0x0f},
    {0xc4, 0x5c},
    {CTRL1, 0xff},
    {0x7f, 0x00},
    {0xe5, 0x1f},
    {0xe1, 0x67},
    {0xdd, 0x7f},
    {IMAGE_MODE, 0x00},
    {RESET, 0x00},
    {R_BYPASS, R_BYPASS_DSP_EN},
};

static const ov2640_reginfo_t init_reglist_DVP_8bit_RGB565_640x480_XCLK_20_6fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch svga mode
    ov2640_settings_to_svga
    // set win_regs, zoom from svga
    {BANK_SEL, BANK_DSP},
    {0x51, 0xc8},
    {0x52, 0x96},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x57, 0x00},
    {0x5a, 0xa0},
    {0x5b, 0x78},
    {0x5c, 0x00},
    // sensor clk
    {BANK_SEL, BANK_SENSOR},
    {CLKRC, 0x87},
    // DSP PCLK
    {BANK_SEL, BANK_DSP},
    {R_DVP_SP, 0x88},
    // DSP output en
    {R_BYPASS, R_BYPASS_DSP_EN},
    {REG_DELAY, 0x05},
    ov2640_settings_rgb565
};

static const ov2640_reginfo_t init_reglist_DVP_8bit_YUV422_640x480_XCLK_20_6fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch svga mode
    ov2640_settings_to_svga
    // set win_regs, zoom from svga
    {BANK_SEL, BANK_DSP},
    {0x51, 0xc8},
    {0x52, 0x96},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x57, 0x00},
    {0x5a, 0xa0},
    {0x5b, 0x78},
    {0x5c, 0x00},
    // sensor clk
    {BANK_SEL, BANK_SENSOR},
    {CLKRC, 0x87},
    // DSP PCLK
    {BANK_SEL, BANK_DSP},
    {R_DVP_SP, 0x88},
    // DSP output en
    {R_BYPASS, R_BYPASS_DSP_EN},
    {REG_DELAY, 0x05},
    ov2640_settings_yuv422
};

static const ov2640_reginfo_t init_reglist_DVP_8bit_JPEG_640x480_XCLK_20_25fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch svga mode
    ov2640_settings_to_svga
    // set win_regs, zoom from svga
    {BANK_SEL, BANK_DSP},
    {0x51, 0xc8},
    {0x52, 0x96},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x57, 0x00},
    {0x5a, 0xa0},
    {0x5b, 0x78},
    {0x5c, 0x00},
    // sensor clk
    {BANK_SEL, BANK_SENSOR},
    {CLKRC, 0x00},
    // DSP PCLK
    {BANK_SEL, BANK_DSP},
    {R_DVP_SP, 0x08},
    // JPEG Quality
    {QS, OV2640_JPEG_QUALITY_DEFAULT},
    // DSP output en
    {R_BYPASS, R_BYPASS_DSP_EN},
    {REG_DELAY, 0x05},
    ov2640_settings_jpeg3
    {REG_DELAY, 0x10},
};

static const ov2640_reginfo_t init_reglist_DVP_8bit_RGB565_240x240_XCLK_20_25fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch cif mode
    ov2640_settings_to_cif
    // set win_regs, zoom from cif
    {BANK_SEL, BANK_DSP},
    {0x51, 0x4b},
    {0x52, 0x4a},
    {0x53, 0x32},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x57, 0x00},
    {0x5a, 0x3c},
    {0x5b, 0x3c},
    {0x5c, 0x00},
    // sensor clk
    {BANK_SEL, BANK_SENSOR},
    {CLKRC, 0x83},
    // DSP PCLK
    {BANK_SEL, BANK_DSP},
    {R_DVP_SP, 0x88},
    // DSP output en
    {R_BYPASS, R_BYPASS_DSP_EN},
    {REG_DELAY, 0x05},
    ov2640_settings_rgb565
};

static const ov2640_reginfo_t init_reglist_DVP_8bit_YUV422_240x240_XCLK_20_25fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch cif mode
    ov2640_settings_to_cif
    // set win_regs, zoom from cif
    {BANK_SEL, BANK_DSP},
    {0x51, 0x4b},
    {0x52, 0x4a},
    {0x53, 0x32},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x57, 0x00},
    {0x5a, 0x3c},
    {0x5b, 0x3c},
    {0x5c, 0x00},
    // sensor clk
    {BANK_SEL, BANK_SENSOR},
    {CLKRC, 0x83},
    // DSP PCLK
    {BANK_SEL, BANK_DSP},
    {R_DVP_SP, 0x88},
    // DSP output en
    {R_BYPASS, R_BYPASS_DSP_EN},
    {REG_DELAY, 0x05},
    ov2640_settings_yuv422
};

static const ov2640_reginfo_t init_reglist_DVP_8bit_JPEG_320x240_XCLK_20_50fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch cif mode
    ov2640_settings_to_cif
    // set win_regs, zoom from cif
    {BANK_SEL, BANK_DSP},
    {0x51, 0x64},
    {0x52, 0x4a},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x57, 0x00},
    {0x5a, 0x50},
    {0x5b, 0x3c},
    {0x5c, 0x00},
    // sensor clk
    {BANK_SEL, BANK_SENSOR},
    {CLKRC, 0x00},
    // DSP PCLK
    {BANK_SEL, BANK_DSP},
    {R_DVP_SP, 0x08},
    // JPEG Quality
    {QS, OV2640_JPEG_QUALITY_DEFAULT},
    // DSP output en
    {R_BYPASS, R_BYPASS_DSP_EN},
    {REG_DELAY, 0x05},
    ov2640_settings_jpeg3
    {REG_DELAY, 0x10},
};

static const ov2640_reginfo_t init_reglist_DVP_8bit_JPEG_1280x720_XCLK_20_12fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch uxga mode
    ov2640_settings_to_uxga
    // set win_regs, zoom from uxga
    {BANK_SEL, BANK_DSP},
    {0x51, 0x90},
    {0x52, 0xe1},
    {0x53, 0x00},
    {0x54, 0x96},
    {0x55, 0x08},
    {0x57, 0x00},
    {0x5a, 0x40},
    {0x5b, 0xb4},
    {0x5c, 0x01},
    // sensor clk
    {BANK_SEL, BANK_SENSOR},
    {CLKRC, 0x00},
    // DSP PCLK
    {BANK_SEL, BANK_DSP},
    {R_DVP_SP, 0x0C},
    // JPEG Quality
    {QS, OV2640_JPEG_QUALITY_DEFAULT},
    // DSP output en
    {R_BYPASS, R_BYPASS_DSP_EN},
    {REG_DELAY, 0x05},
    ov2640_settings_jpeg3
    {REG_DELAY, 0x10},
};

static const ov2640_reginfo_t init_reglist_DVP_8bit_JPEG_1600x1200_XCLK_20_12fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch uxga mode
    ov2640_settings_to_uxga
    // set win_regs, zoom from uxga
    {BANK_SEL, BANK_DSP},
    {0x51, 0x90},
    {0x52, 0x2c},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x88},
    {0x57, 0x00},
    {0x5a, 0x90},
    {0x5b, 0x2c},
    {0x5c, 0x05},
    // sensor clk
    {BANK_SEL, BANK_SENSOR},
    {CLKRC, 0x00},
    // DSP PCLK
    {BANK_SEL, BANK_DSP},
    {R_DVP_SP, 0x0c},
    // JPEG Quality
    {QS, OV2640_JPEG_QUALITY_DEFAULT},
    // DSP output en
    {R_BYPASS, R_BYPASS_DSP_EN},
    {REG_DELAY, 0x05},
    ov2640_settings_jpeg3
    {REG_DELAY, 0x10},
};

#ifdef __cplusplus
}
#endif
