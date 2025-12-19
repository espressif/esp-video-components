/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
static const ov2640_reginfo_t ov2640_dvp_8bit_20Minput_1600x1200_jpeg_12fps[] = {
    {BANK_SEL, BANK_DSP},
    {R_BYPASS, R_BYPASS_DSP_BYPAS},
    // switch uxga mode
    ov2640_settings_to_uxga,
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
    ov2640_settings_jpeg3,
    {REG_DELAY, 0x10},
};
