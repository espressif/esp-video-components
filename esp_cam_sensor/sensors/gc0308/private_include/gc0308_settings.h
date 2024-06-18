/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "gc0308_regs.h"
#include "gc0308_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GC0308_RGB565_FMT   (0xa6)
#define GC0308_YUYV422_FMT  (0xa0) // send in [Cb Y Cr Y] order. The sequence of data stored is [Y Cb Y Cr]
#define GC0308_ONLY_Y_FMT   (0xb1)

#define gc0308_settings_rgb565 \
    {GC0308_REG_OUTPUT_FMT, GC0308_RGB565_FMT}, \

#define gc0308_settings_yuv422 \
    {GC0308_REG_OUTPUT_FMT, GC0308_YUYV422_FMT}, \

#define gc0308_settings_only_y \
    {GC0308_REG_OUTPUT_FMT, GC0308_ONLY_Y_FMT}, \

#define gc0308_select_page0_default \
    {0xfe, 0x80}, \
    {0xfe, 0x00}, \

#define init_reglist_DVP_8bit_640x480_16fps \
    {GC0308_REG_OUTPUT_EN, 0x00}, /* data disable */\
    {0xd2, 0x10}, /* close AEC */\
    {0x22, 0x55}, /* close AWB */\
    {0x03, 0x01}, \
    {0x04, 0x2c}, \
    {0x5a, 0x56}, \
    {0x5b, 0x40}, \
    {0x5c, 0x4a}, \
    {0x22, 0x57}, /* Open AWB */\
    {0x01, 0x6a}, \
    {0x02, 0x37}, \
    {0x0f, 0x10}, \
    {0xe2, 0x00}, \
    {0xe3, 0x7d}, \
    {0xe4, 0x02}, \
    {0xe5, 0x71}, \
    {0xe6, 0x02}, \
    {0xe7, 0x71}, \
    {0xe8, 0x02}, \
    {0xe9, 0x71}, \
    {0xea, 0x02}, \
    {0xeb, 0x71}, \
    {0xec, 0x00}, \
    {0x05, 0x00}, \
    {0x06, 0x00}, \
    {0x07, 0x00}, \
    {0x08, 0x00}, \
    {0x09, 0x01}, \
    {0x0a, 0xe8}, \
    {0x0b, 0x02}, \
    {0x0c, 0x88}, \
    {0x46, 0x80}, \
    {0x47, 0x00}, \
    {0x48, 0x00}, \
    {0x49, 0x01}, \
    {0x4a, 0xe0}, \
    {0x4b, 0x02}, \
    {0x4c, 0x80}, \
    {0x0d, 0x02}, \
    {0x0e, 0x02}, \
    {0x10, 0x22}, \
    {0x11, 0xfd}, \
    {0x12, 0x2a}, \
    {0x13, 0x00}, \
    {0x15, 0x0a}, \
    {0x16, 0x05}, \
    {0x17, 0x01}, \
    {0x18, 0x44}, \
    {0x19, 0x44}, \
    {0x1a, 0x1e}, \
    {0x1b, 0x00}, \
    {0x1c, 0xc1}, \
    {0x1d, 0x08}, \
    {0x1e, 0x60}, \
    {0x1f, 0x16}, \
    {0x20, 0xff}, \
    {0x21, 0xf8}, \
    {0x22, 0x57}, \
    {0x26, 0x03}, \
    {0x2f, 0x01}, \
    {0x30, 0xf7}, \
    {0x31, 0x50}, \
    {0x32, 0x00}, \
    {0x39, 0x04}, \
    {0x3a, 0x18}, \
    {0x3b, 0x20}, \
    {0x3c, 0x00}, \
    {0x3d, 0x00}, \
    {0x3e, 0x00}, \
    {0x3f, 0x00}, \
    {0x50, 0x10}, \
    {0x53, 0x82}, \
    {0x54, 0x80}, \
    {0x55, 0x80}, \
    {0x56, 0x82}, \
    {0x8b, 0x40}, \
    {0x8c, 0x40}, \
    {0x8d, 0x40}, \
    {0x8e, 0x2e}, \
    {0x8f, 0x2e}, \
    {0x90, 0x2e}, \
    {0x91, 0x3c}, \
    {0x92, 0x50}, \
    {0x5d, 0x12}, \
    {0x5e, 0x1a}, \
    {0x5f, 0x24}, \
    {0x60, 0x07}, \
    {0x61, 0x15}, \
    {0x62, 0x08}, \
    {0x64, 0x03}, \
    {0x66, 0xe8}, \
    {0x67, 0x86}, \
    {0x68, 0xa2}, \
    {0x69, 0x18}, \
    {0x6a, 0x0f}, \
    {0x6b, 0x00}, \
    {0x6c, 0x5f}, \
    {0x6d, 0x8f}, \
    {0x6e, 0x55}, \
    {0x6f, 0x38}, \
    {0x70, 0x15}, \
    {0x71, 0x33}, \
    {0x72, 0xdc}, \
    {0x73, 0x80}, \
    {0x74, 0x02}, \
    {0x75, 0x3f}, \
    {0x76, 0x02}, \
    {0x77, 0x36}, \
    {0x78, 0x88}, \
    {0x79, 0x81}, \
    {0x7a, 0x81}, \
    {0x7b, 0x22}, \
    {0x7c, 0xff}, \
    {0x93, 0x48}, \
    {0x94, 0x00}, \
    {0x95, 0x05}, \
    {0x96, 0xe8}, \
    {0x97, 0x40}, \
    {0x98, 0xf0}, \
    {0xb1, 0x38}, \
    {0xb2, 0x38}, \
    {0xbd, 0x38}, \
    {0xbe, 0x36}, \
    {0xd0, 0xc9}, \
    {0xd1, 0x10}, \
    {0xd3, 0x80}, \
    {0xd5, 0xf2}, \
    {0xd6, 0x16}, \
    {0xdb, 0x92}, \
    {0xdc, 0xa5}, \
    {0xdf, 0x23}, \
    {0xd9, 0x00}, \
    {0xda, 0x00}, \
    {0xe0, 0x09}, \
    {0xed, 0x04}, \
    {0xee, 0xa0}, \
    {0xef, 0x40}, \
    {0x80, 0x03}, \
    {0x80, 0x03}, \
    {0x9F, 0x10}, \
    {0xA0, 0x20}, \
    {0xA1, 0x38}, \
    {0xA2, 0x4E}, \
    {0xA3, 0x63}, \
    {0xA4, 0x76}, \
    {0xA5, 0x87}, \
    {0xA6, 0xA2}, \
    {0xA7, 0xB8}, \
    {0xA8, 0xCA}, \
    {0xA9, 0xD8}, \
    {0xAA, 0xE3}, \
    {0xAB, 0xEB}, \
    {0xAC, 0xF0}, \
    {0xAD, 0xF8}, \
    {0xAE, 0xFD}, \
    {0xAF, 0xFF}, \
    {0xc0, 0x00}, \
    {0xc1, 0x10}, \
    {0xc2, 0x1C}, \
    {0xc3, 0x30}, \
    {0xc4, 0x43}, \
    {0xc5, 0x54}, \
    {0xc6, 0x65}, \
    {0xc7, 0x75}, \
    {0xc8, 0x93}, \
    {0xc9, 0xB0}, \
    {0xca, 0xCB}, \
    {0xcb, 0xE6}, \
    {0xcc, 0xFF}, \
    {0xf0, 0x02}, \
    {0xf1, 0x01}, \
    {0xf2, 0x01}, \
    {0xf3, 0x30}, \
    {0xf9, 0x9f}, \
    {0xfa, 0x78}, \
    {0xfe, 0x01}, \
    {0x00, 0xf5}, \
    {0x02, 0x1a}, \
    {0x0a, 0xa0}, \
    {0x0b, 0x60}, \
    {0x0c, 0x08}, \
    {0x0e, 0x4c}, \
    {0x0f, 0x39}, \
    {0x11, 0x3f}, \
    {0x12, 0x72}, \
    {0x13, 0x13}, \
    {0x14, 0x42}, \
    {0x15, 0x43}, \
    {0x16, 0xc2}, \
    {0x17, 0xa8}, \
    {0x18, 0x18}, \
    {0x19, 0x40}, \
    {0x1a, 0xd0}, \
    {0x1b, 0xf5}, \
    {0x70, 0x40}, \
    {0x71, 0x58}, \
    {0x72, 0x30}, \
    {0x73, 0x48}, \
    {0x74, 0x20}, \
    {0x75, 0x60}, \
    {0x77, 0x20}, \
    {0x78, 0x32}, \
    {0x30, 0x03}, \
    {0x31, 0x40}, \
    {0x32, 0xe0}, \
    {0x33, 0xe0}, \
    {0x34, 0xe0}, \
    {0x35, 0xb0}, \
    {0x36, 0xc0}, \
    {0x37, 0xc0}, \
    {0x38, 0x04}, \
    {0x39, 0x09}, \
    {0x3a, 0x12}, \
    {0x3b, 0x1C}, \
    {0x3c, 0x28}, \
    {0x3d, 0x31}, \
    {0x3e, 0x44}, \
    {0x3f, 0x57}, \
    {0x40, 0x6C}, \
    {0x41, 0x81}, \
    {0x42, 0x94}, \
    {0x43, 0xA7}, \
    {0x44, 0xB8}, \
    {0x45, 0xD6}, \
    {0x46, 0xEE}, \
    {0x47, 0x0d}, \
    {0xfe, 0x00}, \
    {0xd2, 0x90}, \
    {0xfe, 0x00}, \
    {0x10, 0x26}, \
    {0x11, 0x0d}, \
    {0x1a, 0x2a}, \
    {0x1c, 0x49}, \
    {0x1d, 0x9a}, \
    {0x1e, 0x61}, \
    {0x3a, 0x20}, \
    {0x50, 0x14}, \
    {0x53, 0x80}, \
    {0x56, 0x80}, \
    {0x8b, 0x20}, \
    {0x8c, 0x20}, \
    {0x8d, 0x20}, \
    {0x8e, 0x14}, \
    {0x8f, 0x10}, \
    {0x90, 0x14}, \
    {0x94, 0x02}, \
    {0x95, 0x07}, \
    {0x96, 0xe0}, \
    {0xb1, 0x40}, \
    {0xb2, 0x40}, \
    {0xb3, 0x40}, \
    {0xb6, 0xe0}, \
    {0xd0, 0xcb}, \
    {0xd3, 0x48}, \
    {0xf2, 0x02}, \
    {0xf7, 0x12}, \
    {0xf8, 0x0a}, \
    {0xfe, 0x01}, \
    {0x02, 0x20}, \
    {0x04, 0x10}, \
    {0x05, 0x08}, \
    {0x06, 0x20}, \
    {0x08, 0x0a}, \
    {0x0e, 0x44}, \
    {0x0f, 0x32}, \
    {0x10, 0x41}, \
    {0x11, 0x37}, \
    {0x12, 0x22}, \
    {0x13, 0x19}, \
    {0x14, 0x44}, \
    {0x15, 0x44}, \
    {0x19, 0x50}, \
    {0x1a, 0xd8}, \
    {0x32, 0x10}, \
    {0x35, 0x00}, \
    {0x36, 0x80}, \
    {0x37, 0x00}, \
    {0xfe, 0x00}, \
    {0x9F, 0x0E}, \
    {0xA0, 0x1C}, \
    {0xA1, 0x34}, \
    {0xA2, 0x48}, \
    {0xA3, 0x5A}, \
    {0xA4, 0x6B}, \
    {0xA5, 0x7B}, \
    {0xA6, 0x95}, \
    {0xA7, 0xAB}, \
    {0xA8, 0xBF}, \
    {0xA9, 0xCE}, \
    {0xAA, 0xD9}, \
    {0xAB, 0xE4}, \
    {0xAC, 0xEC}, \
    {0xAD, 0xF7}, \
    {0xAE, 0xFD}, \
    {0xAF, 0xFF}, \
    {0x14, 0x10}, \

#define init_reglist_DVP_8bit_320x240_20fps_subsample \
    {GC0308_REG_OUTPUT_EN, 0x00}, /* data disable */\
    {0xec, 0x20}, \
    {0x05, 0x00}, \
    {0x06, 0x00}, \
    {0x07, 0x00}, \
    {0x08, 0x00}, \
    {0x09, 0x01}, \
    {0x0a, 0xe8}, \
    {0x0b, 0x02}, \
    {0x0c, 0x88}, \
    {0x0d, 0x02}, \
    {0x0e, 0x02}, \
    {0x10, 0x26}, \
    {0x11, 0x0d}, \
    {0x12, 0x2a}, \
    {0x13, 0x00}, \
    {0x14, 0x11}, \
    {0x15, 0x0a}, \
    {0x16, 0x05}, \
    {0x17, 0x01}, \
    {0x18, 0x44}, \
    {0x19, 0x44}, \
    {0x1a, 0x2a}, \
    {0x1b, 0x00}, \
    {0x1c, 0x49}, \
    {0x1d, 0x9a}, \
    {0x1e, 0x61}, \
    {0x1f, 0x00}, \
    {0x20, 0x7f}, \
    {0x21, 0xfa}, \
    {0x22, 0x57}, \
    {0x26, 0x03}, \
    {0x28, 0x00}, \
    {0x2d, 0x0a}, \
    {0x2f, 0x01}, \
    {0x30, 0xf7}, \
    {0x31, 0x50}, \
    {0x32, 0x00}, \
    {0x33, 0x28}, \
    {0x34, 0x2a}, \
    {0x35, 0x28}, \
    {0x39, 0x04}, \
    {0x3a, 0x20}, \
    {0x3b, 0x20}, \
    {0x3c, 0x00}, \
    {0x3d, 0x00}, \
    {0x3e, 0x00}, \
    {0x3f, 0x00}, \
    {0x50, 0x14}, \
    {0x52, 0x41}, \
    {0x53, 0x80}, \
    {0x54, 0x80}, \
    {0x55, 0x80}, \
    {0x56, 0x80}, \
    {0x8b, 0x20}, \
    {0x8c, 0x20}, \
    {0x8d, 0x20}, \
    {0x8e, 0x14}, \
    {0x8f, 0x10}, \
    {0x90, 0x14}, \
    {0x91, 0x3c}, \
    {0x92, 0x50}, \
    {0x5d, 0x12}, \
    {0x5e, 0x1a}, \
    {0x5f, 0x24}, \
    {0x60, 0x07}, \
    {0x61, 0x15}, \
    {0x62, 0x08}, \
    {0x64, 0x03}, \
    {0x66, 0xe8}, \
    {0x67, 0x86}, \
    {0x68, 0x82}, \
    {0x69, 0x18}, \
    {0x6a, 0x0f}, \
    {0x6b, 0x00}, \
    {0x6c, 0x5f}, \
    {0x6d, 0x8f}, \
    {0x6e, 0x55}, \
    {0x6f, 0x38}, \
    {0x70, 0x15}, \
    {0x71, 0x33}, \
    {0x72, 0xdc}, \
    {0x73, 0x00}, \
    {0x74, 0x02}, \
    {0x75, 0x3f}, \
    {0x76, 0x02}, \
    {0x77, 0x38}, \
    {0x78, 0x88}, \
    {0x79, 0x81}, \
    {0x7a, 0x81}, \
    {0x7b, 0x22}, \
    {0x7c, 0xff}, \
    {0x93, 0x48}, \
    {0x94, 0x02}, \
    {0x95, 0x07}, \
    {0x96, 0xe0}, \
    {0x97, 0x40}, \
    {0x98, 0xf0}, \
    {0xb1, 0x40}, \
    {0xb2, 0x40}, \
    {0xb3, 0x40}, \
    {0xb6, 0xe0}, \
    {0xbd, 0x38}, \
    {0xbe, 0x36}, \
    {0xd0, 0xCB}, \
    {0xd1, 0x10}, \
    {0xd2, 0x90}, \
    {0xd3, 0x48}, \
    {0xd5, 0xF2}, \
    {0xd6, 0x16}, \
    {0xdb, 0x92}, \
    {0xdc, 0xA5}, \
    {0xdf, 0x23}, \
    {0xd9, 0x00}, \
    {0xda, 0x00}, \
    {0xe0, 0x09}, \
    {0xed, 0x04}, \
    {0xee, 0xa0}, \
    {0xef, 0x40}, \
    {0x80, 0x03}, \
    {0x9F, 0x10}, \
    {0xA0, 0x20}, \
    {0xA1, 0x38}, \
    {0xA2, 0x4e}, \
    {0xA3, 0x63}, \
    {0xA4, 0x76}, \
    {0xA5, 0x87}, \
    {0xA6, 0xa2}, \
    {0xA7, 0xb8}, \
    {0xA8, 0xca}, \
    {0xA9, 0xd8}, \
    {0xAA, 0xe3}, \
    {0xAB, 0xeb}, \
    {0xAC, 0xf0}, \
    {0xAD, 0xF8}, \
    {0xAE, 0xFd}, \
    {0xAF, 0xFF}, \
    {0xc0, 0x00}, \
    {0xc1, 0x10}, \
    {0xc2, 0x1c}, \
    {0xc3, 0x30}, \
    {0xc4, 0x43}, \
    {0xc5, 0x54}, \
    {0xc6, 0x65}, \
    {0xc7, 0x75}, \
    {0xc8, 0x93}, \
    {0xc9, 0xB0}, \
    {0xca, 0xCB}, \
    {0xcb, 0xE6}, \
    {0xcc, 0xFF}, \
    {0xf0, 0x02}, \
    {0xf1, 0x01}, \
    {0xf2, 0x02}, \
    {0xf3, 0x30}, \
    {0xf7, 0x04}, \
    {0xf8, 0x02}, \
    {0xf9, 0x9f}, \
    {0xfa, 0x78}, \
    {0xfe, 0x01}, \
    {0x00, 0xf5}, \
    {0x02, 0x20}, \
    {0x04, 0x10}, \
    {0x05, 0x08}, \
    {0x06, 0x20}, \
    {0x08, 0x0a}, \
    {0x0a, 0xa0}, \
    {0x0b, 0x60}, \
    {0x0c, 0x08}, \
    {0x0e, 0x44}, \
    {0x0f, 0x32}, \
    {0x10, 0x41}, \
    {0x11, 0x37}, \
    {0x12, 0x22}, \
    {0x13, 0x19}, \
    {0x14, 0x44}, \
    {0x15, 0x44}, \
    {0x16, 0xc2}, \
    {0x17, 0xA8}, \
    {0x18, 0x18}, \
    {0x19, 0x50}, \
    {0x1a, 0xd8}, \
    {0x1b, 0xf5}, \
    {0x70, 0x40}, \
    {0x71, 0x58}, \
    {0x72, 0x30}, \
    {0x73, 0x48}, \
    {0x74, 0x20}, \
    {0x75, 0x60}, \
    {0x77, 0x20}, \
    {0x78, 0x32}, \
    {0x30, 0x03}, \
    {0x31, 0x40}, \
    {0x32, 0x10}, \
    {0x33, 0xe0}, \
    {0x34, 0xe0}, \
    {0x35, 0x00}, \
    {0x36, 0x80}, \
    {0x37, 0x00}, \
    {0x38, 0x04}, \
    {0x39, 0x09}, \
    {0x3a, 0x12}, \
    {0x3b, 0x1C}, \
    {0x3c, 0x28}, \
    {0x3d, 0x31}, \
    {0x3e, 0x44}, \
    {0x3f, 0x57}, \
    {0x40, 0x6C}, \
    {0x41, 0x81}, \
    {0x42, 0x94}, \
    {0x43, 0xA7}, \
    {0x44, 0xB8}, \
    {0x45, 0xD6}, \
    {0x46, 0xEE}, \
    {0x47, 0x0d}, \
    {0x53, 0x83}, \
    {0x54, 0x22}, \
    {0x55, 0x03}, \
    {0x56, 0x00}, \
    {0x57, 0x00}, \
    {0x58, 0x00}, \
    {0x59, 0x00}, \
    {0x62, 0xf7}, \
    {0x63, 0x68}, \
    {0x64, 0xd3}, \
    {0x65, 0xd3}, \
    {0x66, 0x60}, \
    {0xfe, 0x00}, \
    {0x01, 0x32}, /* frame setting */\
    {0x02, 0x0c}, \
    {0x0f, 0x01}, \
    {0xe2, 0x00}, \
    {0xe3, 0x78}, \
    {0xe4, 0x00}, \
    {0xe5, 0xfe}, \
    {0xe6, 0x01}, \
    {0xe7, 0xe0}, \
    {0xe8, 0x01}, \
    {0xe9, 0xe0}, \
    {0xea, 0x01}, \
    {0xeb, 0xe0}, \
    {0xfe, 0x00}, \

static const gc0308_reginfo_t DVP_8bit_20Minput_640x480_only_y_16fps[] = {
    gc0308_select_page0_default
    gc0308_settings_only_y
    init_reglist_DVP_8bit_640x480_16fps
};

static const gc0308_reginfo_t DVP_8bit_20Minput_640x480_rgb565_16fps[] = {
    gc0308_select_page0_default
    gc0308_settings_rgb565
    init_reglist_DVP_8bit_640x480_16fps
};

static const gc0308_reginfo_t DVP_8bit_20Minput_640x480_yuv422_16fps[] = {
    gc0308_select_page0_default
    gc0308_settings_yuv422
    init_reglist_DVP_8bit_640x480_16fps
};

static const gc0308_reginfo_t DVP_8bit_20Minput_320x240_only_y_20fps_subsample[] = {
    gc0308_select_page0_default
    gc0308_settings_only_y
    init_reglist_DVP_8bit_320x240_20fps_subsample
};

static const gc0308_reginfo_t DVP_8bit_20Minput_320x240_rgb565_20fps_subsample[] = {
    gc0308_select_page0_default
    gc0308_settings_rgb565
    init_reglist_DVP_8bit_320x240_20fps_subsample
};

static const gc0308_reginfo_t DVP_8bit_20Minput_320x240_yuv422_20fps_subsample[] = {
    gc0308_select_page0_default
    gc0308_settings_yuv422
    init_reglist_DVP_8bit_320x240_20fps_subsample
};

#ifdef __cplusplus
}
#endif
