/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
static const ov3660_reginfo_t ov3660_dvp_8bit_20Minput_240x240_rgb565_be_24fps[] = {
    ov3660_settings_240X240
    ov3660_settings_fmt_rgb565_be
    {OV3660_REGLIST_TAIL, 0x00}, // tail
};
