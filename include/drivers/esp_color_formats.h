/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumerated list of pixel data formats
 */
typedef enum {
    PIXFORMAT_RGB565,    // 2BPP/RGB565, 1Bytes Per Pixel
    PIXFORMAT_YUV422,    // 2BPP/YUV422
    PIXFORMAT_YUV420,    // 1.5BPP/YUV420
    PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
    PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    PIXFORMAT_RGB888,    // 3BPP/RGB888
    PIXFORMAT_RAW8,      // 1BPP/RAW8
    PIXFORMAT_RAW10,     // 5BP4P/RAW8
    PIXFORMAT_RAW12,     // 3BP2P/RAW8
    PIXFORMAT_RGB444,    // 3BP2P/RGB444
    PIXFORMAT_RGB555,    // 3BP2P/RGB555
    PIXFORMAT_BGR565,    // 2BPP/RGB565
    PIXFORMAT_BGR888,    // 3BPP/RGB888
    PIXFORMAT_INVALID,
} pixformat_t;

typedef enum {
    color_encoding_RGB,
    color_encoding_YUV,
    color_encoding_RAW,
} color_encoding_t;

typedef struct {
    const unsigned int bits_per_pixel;
    color_encoding_t color_encoding;
    // const char *name;
} pixformat_info_t;

// Resolution table (in esp_color_format.c)
extern const pixformat_info_t pixformat_info_map[];

#ifdef __cplusplus
}
#endif
