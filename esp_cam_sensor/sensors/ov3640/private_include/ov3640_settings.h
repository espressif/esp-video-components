/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "ov3640_regs.h"
#include "ov3640_types.h"

#define ov3640_settings_fmt_yuv422_yuyv \
    {0x3400, 0x00}, \
    {0x3404, 0x00}, \

#define ov3640_settings_fmt_yuv422_uyvy \
    {0x3400, 0x00}, \
    {0x3404, 0x02}, \

#define ov3640_settings_fmt_sbggr8 \
    {0x3400, 0x01}, \
    {0x3404, 0x18}, \

#define ov3640_settings_fmt_rgb565_be \
    {0x3400, 0x01}, \
    {0x3404, 0x11}, \

#define ov3640_settings_fmt_rgb565_le \
    {0x3400, 0x01}, \
    {0x3404, 0x30}, \


#define NUM_COLORS (8)
/* Color Settings - 7 colors
    * Value must be in the range of
    * 0 to 7
    *
    *  Normal          0
    *  Sepia(antique)  1
    *  Mono chrome     2
    *  Negative        3
    *  Bluish          4
    *  Greenish        5
    *  Reddish         6
    *  Yellowish       7
*/
const static ov3640_reginfo_t ov3640_colors[NUM_COLORS][5] = {
    {
        {0x3302, 0xef},
        {0x3355, 0x00},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    {
        {0x3302, 0xef},
        {0x3355, 0x18},
        {0x335a, 0x40},
        {0x335b, 0xa6},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    {
        {0x3302, 0xef},
        {0x3355, 0x18},
        {0x335a, 0x80},
        {0x335b, 0x80},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    {
        {0x3302, 0xef},
        {0x3355, 0x40},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    {
        {0x3302, 0xef},
        {0x3355, 0x18},
        {0x335a, 0xa0},
        {0x335b, 0x40},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    {
        {0x3302, 0xef},
        {0x3355, 0x18},
        {0x335a, 0x60},
        {0x335b, 0x60},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    {
        {0x3302, 0xef},
        {0x3355, 0x18},
        {0x335a, 0x80},
        {0x335b, 0xc0},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    {
        {0x3302, 0xef},
        {0x3355, 0x18},
        {0x335a, 0x30},
        {0x335b, 0x90},
        {OV3640_REGLIST_TAIL, 0x00}
    },
};

/* Average Based Algorithm - Based on target Luminance */
#define NUM_EXPOSURE_AVG (11)
const static ov3640_reginfo_t ov3640_ae_levels[NUM_EXPOSURE_AVG][5] = {
    /* -1.7EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x10},
        {OV3640_REG_BPT_HISL, 0x08},
        {OV3640_REG_VPT, 0x21},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* -1.3EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x18},
        {OV3640_REG_BPT_HISL, 0x10},
        {OV3640_REG_VPT, 0x31},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* -1.0EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x20},
        {OV3640_REG_BPT_HISL, 0x18},
        {OV3640_REG_VPT, 0x41},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* -0.7EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x28},
        {OV3640_REG_BPT_HISL, 0x20},
        {OV3640_REG_VPT, 0x51},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* -0.3EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x30},
        {OV3640_REG_BPT_HISL, 0x28},
        {OV3640_REG_VPT, 0x61},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* default */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x38},
        {OV3640_REG_BPT_HISL, 0x30},
        {OV3640_REG_VPT, 0x61},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* 0.3EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x40},
        {OV3640_REG_BPT_HISL, 0x38},
        {OV3640_REG_VPT, 0x71},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* 0.7EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x48},
        {OV3640_REG_BPT_HISL, 0x40},
        {OV3640_REG_VPT, 0x81},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* 1.0EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x50},
        {OV3640_REG_BPT_HISL, 0x48},
        {OV3640_REG_VPT, 0x91},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* 1.3EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x58},
        {OV3640_REG_BPT_HISL, 0x50},
        {OV3640_REG_VPT, 0x91},
        {OV3640_REGLIST_TAIL, 0x00}
    },
    /* 1.7EV */
    {
        {OV3640_REG_HISTO7, 0x00},
        {OV3640_REG_WPT_HISH, 0x60},
        {OV3640_REG_BPT_HISL, 0x58},
        {OV3640_REG_VPT, 0xa1},
        {OV3640_REGLIST_TAIL, 0x00}
    },
};

#if CONFIG_CAMERA_OV3640_DVP_RGB565_LE_640X480_7FPS
#include "ov3640_dvp_8bit_24Minput_640x480_rgb565_le_7fps.h"
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_640X480_30FPS
#include "ov3640_dvp_8bit_24Minput_640x480_yuv422_uyvy_30fps.h"
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1024X768_5FPS
#include "ov3640_dvp_8bit_24Minput_1024x768_yuv422_uyvy_5fps.h"
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1024X768_25FPS
#include "ov3640_dvp_8bit_24Minput_1024x768_yuv422_uyvy_25fps.h"
#endif
#if CONFIG_CAMERA_OV3640_DVP_YUV422_UYVY_1280X720_15FPS
#include "ov3640_dvp_8bit_24Minput_1280x720_yuv422_uyvy_15fps.h"
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_2048X1536_15FPS
#include "ov3640_dvp_8bit_24Minput_2048x1536_jpeg_15fps.h"
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_1024X768_25FPS
#include "ov3640_dvp_8bit_24Minput_1024x768_jpeg_25fps.h"
#endif
#if CONFIG_CAMERA_OV3640_DVP_JPEG_1280X720_15FPS
#include "ov3640_dvp_8bit_24Minput_1280x720_jpeg_15fps.h"
#endif
#ifdef __cplusplus
}
#endif
