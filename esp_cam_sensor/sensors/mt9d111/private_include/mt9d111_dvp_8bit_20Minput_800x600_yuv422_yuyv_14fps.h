/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
static const mt9d111_reginfo_t mt9d111_dvp_8bit_20Minput_800x600_yuv422_yuyv_14fps[] = {
    // yuv422
    {MT9D111_REG_WRITE_PAGE, 0x01},
    {0x09, 0x01},
    {0x97, YUV422_YUYV},
    {0x98, 0x00},
    {0xC6, 0xA77D},  // MODE_OUTPUT_FORMAT_A
    {0xC8, YUV422_YUYV},  // MODE_OUTPUT_FORMAT_A; yuv422
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
