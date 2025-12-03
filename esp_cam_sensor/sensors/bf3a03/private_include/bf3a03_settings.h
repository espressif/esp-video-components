/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "bf3a03_regs.h"
#include "bf3a03_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BF3A03_SPEC_EFFECT_REG_SIZE (5)
#define BF3A03_SPEC_EFFECT_NUM      (8)
#define BF3A03_AEC_TARGET_MAX       (0x6f)
#define BF3A03_AEC_TARGET_MIN       (0X28)
#define BF3A03_AEC_TARGET_DEFAULT   (0x50)


#define BF3A03_YUV422_UYVY_FMT 0x02
#define BF3A03_YUV422_YUYV_FMT 0x00

/*
* The color effect settings
*/
static const bf3a03_reginfo_t bf3a03_spec_effect_regs[BF3A03_SPEC_EFFECT_NUM][BF3A03_SPEC_EFFECT_REG_SIZE] = {
    {
        {0x70, 0x0f},
        {0x69, 0x00},
        {0x67, 0x80},
        {0x68, 0x80},
        {0xb4, 0x91},
    }, /*colorfx_none*/
    {
        {0x70, 0x0f},
        {0x69, 0x20},
        {0x67, 0x80},
        {0x68, 0x80},
        {0xb4, 0x91},
    }, /*colorfx_bw*/
    {
        {0x70, 0x0b},
        {0x69, 0x20},
        {0x67, 0x60},
        {0x68, 0xa0},
        {0xb4, 0x91},
    }, /*colorfx_sepia*/
    {
        {0x70, 0x0f},
        {0x69, 0x01},
        {0x67, 0x80},
        {0x68, 0x80},
        {0xb4, 0x91},
    }, /*colorfx_negative*/
    {
        {0x70, 0x1b},
        {0x69, 0x01},
        {0x67, 0x80},
        {0x68, 0x80},
        {0xb4, 0x91},
    }, /*colorfx_emboss*/
    {
        {0x70, 0x3b},
        {0x69, 0x00},
        {0x67, 0x80},
        {0x68, 0x80},
        {0xb4, 0x91},
    }, /*colorfx_sketch*/
    {
        {0x70, 0x0b},
        {0x69, 0x20},
        {0x67, 0xe0},
        {0x68, 0x50},
        {0xb4, 0x91},
    }, /*sky_blue*/
    {
        {0x70, 0x0b},
        {0x69, 0x20},
        {0x67, 0x60},
        {0x68, 0x60},
        {0xb4, 0x91},
    }, /*grass_green*/
};

#include "bf3a03_dvp_8bit_20Minput_640x480_yuv422_uyvy_15fps_mono.h"
#include "bf3a03_dvp_8bit_20Minput_640x480_yuv422_yuyv_15fps_mono.h"

#include "bf3a03_dvp_8bit_20Minput_640x480_yuv422_uyvy_15fps.h"
#include "bf3a03_dvp_8bit_20Minput_640x480_yuv422_yuyv_15fps.h"

#ifdef __cplusplus
}
#endif
