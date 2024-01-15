/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_color_formats.h"
#include"sdkconfig.h"

typedef struct {
    pixformat_t format;
    uint8_t bpp;
    color_encoding_t color_encoding;
} pixformat_info_t;

// Define the bitsPerPixel Bits per pixel
static const pixformat_info_t s_pixformat_info_map[] = {
    {PIXFORMAT_RGB565,    16, color_encoding_RGB},
    {PIXFORMAT_YUV422,    16, color_encoding_YUV},
    {PIXFORMAT_YUV420,    12, color_encoding_YUV},
    {PIXFORMAT_GRAYSCALE, 8,  color_encoding_YUV},
    {PIXFORMAT_JPEG,      8,  color_encoding_YUV},
    {PIXFORMAT_RGB888,    24, color_encoding_RGB},
    {PIXFORMAT_RAW8,      8,  color_encoding_RAW},
    {PIXFORMAT_RAW10,     10, color_encoding_RAW},
    {PIXFORMAT_RAW12,     12, color_encoding_RAW},
    {PIXFORMAT_RGB444,    12, color_encoding_RGB},
    {PIXFORMAT_RGB555,    12, color_encoding_RGB},
    {PIXFORMAT_BGR888,    24, color_encoding_RGB},
    // {PIXFORMAT_BGR565, 16, color_encoding_RGB},
};

/**
 * @brief Get bits per pixel of format.
 *
 * @param format Video data format
 *
 * @return Byte per pixel of format
 */
uint8_t esp_video_get_bpp_by_format(uint32_t format)
{
    for (int i = 0; i < ARRAY_SIZE(s_pixformat_info_map); i++) {
        if (s_pixformat_info_map[i].format == format) {
            return s_pixformat_info_map[i].bpp;
        }
    }
    // assert(0);
    return 0;
}

/**
 * @brief Get encoding of format.
 *
 * @param format Video data format
 *
 * @return Encoding of format
 */
color_encoding_t esp_video_get_encoding_by_format(uint32_t format)
{
    for (int i = 0; i < ARRAY_SIZE(s_pixformat_info_map); i++) {
        if (s_pixformat_info_map[i].format == format) {
            return s_pixformat_info_map[i].color_encoding;
        }
    }
    // assert(0);
    return color_encoding_invalid;
}
