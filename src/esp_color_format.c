/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_color_formats.h"
#include"sdkconfig.h"

// Define the bitsPerPixel Bits per pixel
const pixformat_info_t pixformat_info_map[PIXFORMAT_INVALID] = {
    {16, color_encoding_RGB}, // PIXFORMAT_RGB565
    {16, color_encoding_YUV}, // PIXFORMAT_YUV422
    {12, color_encoding_YUV}, // PIXFORMAT_YUV420
    {8,  color_encoding_YUV}, // PIXFORMAT_GRAYSCALE
    {8,  color_encoding_YUV}, // PIXFORMAT_JPEG
    {24, color_encoding_RGB}, // PIXFORMAT_RGB888
    {8,  color_encoding_RAW}, // PIXFORMAT_RAW8
    {10, color_encoding_RAW}, // PIXFORMAT_RAW10
    {12, color_encoding_RAW}, // PIXFORMAT_RAW12
    {12, color_encoding_RGB}, // PIXFORMAT_RGB444
    {12, color_encoding_RGB}, // PIXFORMAT_RGB555
    {16, color_encoding_RGB}, // PIXFORMAT_BGR565
    {24, color_encoding_RGB}, // PIXFORMAT_BGR888
};
