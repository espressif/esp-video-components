/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "mt9d111_regs.h"
#include "mt9d111_types.h"

#if CONFIG_CAMERA_SENSOR_SWAP_PIXEL_BYTE_ORDER
#define YUV422 0x0000
#define RGB565 0x0022
#else
#define YUV422 0x0002
#define RGB565 0x0020
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Soft Reset
  * 1. Bypass the PLL, R0x65:0=0xA000, if it is currently used
  * 2. Perform MCU reset by setting R0xC3:1=0x0501
  * 3. Enable soft reset by setting R0x0D:0=0x0021. Bit 0 is used for
  *    the sensor core reset while bit 5 refers to SOC reset.
  * 4. Disable soft reset by setting R0x0D:0=0x0000
  * 5. Wait 24 clock cycles before using the two-wire serial interface
  */
static const mt9d111_reginfo_t mt9d111_soft_reset_regs[] = {
    // Reset
    {MT9D111_REG_WRITE_PAGE, 0x0000},   // Page Register
    {0x65, 0xA000},     // bypassed PLL (prepare for soft reset)

    {MT9D111_REG_WRITE_PAGE, 0x0001},   // Page Register
    {0xC3, 0x0501},     // MCU_BOOT_MODE (MCU reset)
    {0xC3, 0x0500},     // MCU_BOOT_MODE (MCU reset)

    {MT9D111_REG_WRITE_PAGE, 0x0000},   // Page Register
    {0x0D, 0x0021},     // RESET_REG (enable soft reset)
    {0x0D, 0x0000},     // RESET_REG (disable soft reset)
    {MT9D111_REG_DELAY, 10},//DELAY=10
};

static const mt9d111_reginfo_t DVP_8bit_20Minput_320x240_rgb565_10fps[] = {
    // lenc
    {MT9D111_REG_WRITE_PAGE, 0x0002},   // PAGE REGISTER
    {0x80, 0x0160},     // LENS_CORRECTION_CONTROL
    {MT9D111_REG_WRITE_PAGE, 0x0001},   // PAGE REGISTER
    {0x08, 0x05FC},     // COLOR_PIPELINE_CONTROL

    // QVGA
    {MT9D111_REG_WRITE_PAGE, 0x1},
    {0xC6, 0x2703}, //MODE_OUTPUT_WIDTH_A
    {0xC8, 0x0140}, //MODE_OUTPUT_WIDTH_A
    {0xC6, 0x2705}, //MODE_OUTPUT_HEIGHT_A
    {0xC8, 0x00F0}, //MODE_OUTPUT_HEIGHT_A
    {0xC6, 0x2707}, //MODE_OUTPUT_WIDTH_B
    {0xC8, 0x0280}, //MODE_OUTPUT_WIDTH_B
    {0xC6, 0x2709}, //MODE_OUTPUT_HEIGHT_B
    {0xC8, 0x01E0}, //MODE_OUTPUT_HEIGHT_B
    {0xC6, 0x2779}, //Spoof Frame Width
    {0xC8, 0x0140}, //Spoof Frame Width
    {0xC6, 0x277B}, //Spoof Frame Height
    {0xC8, 0x00F0}, //Spoof Frame Height
    {0xC6, 0xA103}, //SEQ_CMD
    {0xC8, 0x0005}, //SEQ_CMD

    // rgb565
    {MT9D111_REG_WRITE_PAGE, 0x01},
    {0x09, 0x01},
    {0x97, RGB565},
    {0x98, 0x00},
    {0xC6, 0xA77D},  // MODE_OUTPUT_FORMAT_A
    {0xC8, RGB565},  // MODE_OUTPUT_FORMAT_A; RGB565
    {0xC6, 0x270B},  // MODE_CONFIG
    {0xC8, 0x0030},  // MODE_CONFIG, JPEG disabled for A and B
    {0xC6, 0xA103},  // SEQ_CMD
    {0xC8, 0x0005},   // SEQ_CMD, refresh

    // PLL control
    {MT9D111_REG_WRITE_PAGE, 0x00},
    {0x65, 0xA000},  //Clock: <15> PLL BYPASS = 1 --- make sure that PLL is bypassed
    {0x65, 0xE000},  //Clock: <14> PLL OFF = 1 --- make sure that PLL is powered-down
    {0x66, 0x3201},  //PLL Control 1: <15:8> M = 50, <5:0> N = 1
    {0x67, 0x0503},  //PLL Control 2: <6:0> P = 3
    {0x65, 0xA000},  //Clock: <14> PLL OFF = 0 --- PLL is powered-up
    {MT9D111_REG_DELAY, 10}, //Delay 10 ms

    {0x65, 0x2000},  //Clock: <15> PLL BYPASS = 0 --- enable PLL as master clock
};

static const mt9d111_reginfo_t DVP_8bit_20Minput_800x600_yuv422_8fps[] = {
    // lenc
    {MT9D111_REG_WRITE_PAGE, 0x0002},   // PAGE REGISTER
    {0x80, 0x0160},     // LENS_CORRECTION_CONTROL
    {MT9D111_REG_WRITE_PAGE, 0x0001},   // PAGE REGISTER
    {0x08, 0x05FC},     // COLOR_PIPELINE_CONTROL

    // yuv422
    {MT9D111_REG_WRITE_PAGE, 0x01},
    {0x09, 0x01},
    {0x97, YUV422},
    {0x98, 0x00},
    {0xC6, 0xA77D},  // MODE_OUTPUT_FORMAT_A
    {0xC8, YUV422},  // MODE_OUTPUT_FORMAT_A; yuv422
    {0xC6, 0x270B},  // MODE_CONFIG
    {0xC8, 0x0030},  // MODE_CONFIG, JPEG disabled for A and B
    {0xC6, 0xA103},  // SEQ_CMD
    {0xC8, 0x0005},   // SEQ_CMD, refresh
    // PLL control
    {MT9D111_REG_WRITE_PAGE, 0x00},
    {0x65, 0xA000},  //Clock: <15> PLL BYPASS = 1 --- make sure that PLL is bypassed
    {0x65, 0xE000},  //Clock: <14> PLL OFF = 1 --- make sure that PLL is powered-down
    {0x66, 0x2801},  //PLL Control 1: <15:8> M = 40, <5:0> N = 1
    {0x67, 0x0503},  //PLL Control 2: <6:0> P = 3
    {0x65, 0xA000},  //Clock: <14> PLL OFF = 0 --- PLL is powered-up
    {MT9D111_REG_DELAY, 10}, //Delay 10 ms

    {0x65, 0x2000},  //Clock: <15> PLL BYPASS = 0 --- enable PLL as master clock
};

static const mt9d111_reginfo_t DVP_8bit_20Minput_800x600_rgb565_10fps[] = {
    // lenc
    {MT9D111_REG_WRITE_PAGE, 0x0002},   // PAGE REGISTER
    {0x80, 0x0160},     // LENS_CORRECTION_CONTROL
    {MT9D111_REG_WRITE_PAGE, 0x0001},   // PAGE REGISTER
    {0x08, 0x05FC},     // COLOR_PIPELINE_CONTROL

    // rgb565
    {MT9D111_REG_WRITE_PAGE, 0x01},
    {0x09, 0x01},
    {0x97, RGB565},
    {0x98, 0x00},
    {0xC6, 0xA77D},  // MODE_OUTPUT_FORMAT_A
    {0xC8, RGB565},  // MODE_OUTPUT_FORMAT_A; RGB565
    {0xC6, 0x270B},  // MODE_CONFIG
    {0xC8, 0x0030},  // MODE_CONFIG, JPEG disabled for A and B
    {0xC6, 0xA103},  // SEQ_CMD
    {0xC8, 0x0005},   // SEQ_CMD, refresh

    // PLL control
    {MT9D111_REG_WRITE_PAGE, 0x00},
    {0x65, 0xA000},  //Clock: <15> PLL BYPASS = 1 --- make sure that PLL is bypassed
    {0x65, 0xE000},  //Clock: <14> PLL OFF = 1 --- make sure that PLL is powered-down
    {0x66, 0x3201},  //PLL Control 1: <15:8> M = 50, <5:0> N = 1
    {0x67, 0x0503},  //PLL Control 2: <6:0> P = 3
    {0x65, 0xA000},  //Clock: <14> PLL OFF = 0 --- PLL is powered-up
    {MT9D111_REG_DELAY, 10}, //Delay 10 ms

    {0x65, 0x2000},  //Clock: <15> PLL BYPASS = 0 --- enable PLL as master clock
};

static const mt9d111_reginfo_t DVP_8bit_20Minput_800x600_yuv422_14fps[] = {
    // yuv422
    {MT9D111_REG_WRITE_PAGE, 0x01},
    {0x09, 0x01},
    {0x97, YUV422},
    {0x98, 0x00},
    {0xC6, 0xA77D},  // MODE_OUTPUT_FORMAT_A
    {0xC8, YUV422},  // MODE_OUTPUT_FORMAT_A; yuv422
    {0xC6, 0x270B},  // MODE_CONFIG
    {0xC8, 0x0030},  // MODE_CONFIG, JPEG disabled for A and B
    {0xC6, 0xA103},  // SEQ_CMD
    {0xC8, 0x0005},   // SEQ_CMD, refresh
    // PLL control
    {MT9D111_REG_WRITE_PAGE, 0x00},
    {0x65, 0xA000},  //Clock: <15> PLL BYPASS = 1 --- make sure that PLL is bypassed
    {0x65, 0xE000},  //Clock: <14> PLL OFF = 1 --- make sure that PLL is powered-down
    {0x66, 0x3201},  //PLL Control 1: <15:8> M = 50, <5:0> N = 1
    {0x67, 0x0502},  //PLL Control 2: <6:0> P = 2

    {0x65, 0xA000},     //Clock CNTRL: PLL ON = 40960
    {MT9D111_REG_DELAY, 10},
    {0x65, 0x2000},     //Clock CNTRL: USE PLL = 8192
    {MT9D111_REG_DELAY, 10},

    // AE TARGET
    {0xC6, 0xA206},     // MCU_ADDRESS [AE_TARGET]
    {0xC8, 0x0032},     // MCU_DATA_0
};

static const mt9d111_reginfo_t DVP_8bit_24Minput_800x600_yuv422_16fps[] = {
    // yuv422
    {MT9D111_REG_WRITE_PAGE, 0x01},
    {0x09, 0x01},
    {0x97, YUV422},
    {0x98, 0x00},
    {0xC6, 0xA77D},  // MODE_OUTPUT_FORMAT_A
    {0xC8, YUV422},  // MODE_OUTPUT_FORMAT_A; yuv422
    {0xC6, 0x270B},  // MODE_CONFIG
    {0xC8, 0x0030},  // MODE_CONFIG, JPEG disabled for A and B
    {0xC6, 0xA103},  // SEQ_CMD
    {0xC8, 0x0005},   // SEQ_CMD, refresh

    {MT9D111_REG_WRITE_PAGE, 0x00},
    {0x2B, 0x0020},
    {0x66, 0x1402},
    {0x67, 0x0500},
    {0x65, 0xA000},
    {MT9D111_REG_DELAY, 10}, //Delay 10 ms
    {0x65, 0x2000},
    {MT9D111_REG_DELAY, 10}, //Delay 10 ms

    // Frame 15-30fps
    {MT9D111_REG_WRITE_PAGE, 0x01},
    {0xC6, 0xA20D},
    {0xC8, 0x0004},
    {0xC6, 0xA20E},
    {0xC8, 0x0008},
    {0xC6, 0xA217},
    {0xC8, 0x0005},

    {MT9D111_REG_WRITE_PAGE, 0x00},
};

#ifdef __cplusplus
}
#endif
