/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
static const mt9d111_reginfo_t mt9d111_dvp_8bit_24Minput_800x600_yuv422_yuyv_16fps[] = {
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
