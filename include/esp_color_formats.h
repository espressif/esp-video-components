/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_camera.h"

/**
 * @brief Enumerated list of pixel data formats
 */

#define PIXFORMAT_RGB565    CAM_SENSOR_PIXFORMAT_RGB565
#define PIXFORMAT_YUV422    CAM_SENSOR_PIXFORMAT_YUV422
#define PIXFORMAT_YUV420    CAM_SENSOR_PIXFORMAT_YUV420
#define PIXFORMAT_GRAYSCALE CAM_SENSOR_PIXFORMAT_GRAYSCALE
#define PIXFORMAT_JPEG      CAM_SENSOR_PIXFORMAT_JPEG
#define PIXFORMAT_RGB888    CAM_SENSOR_PIXFORMAT_BGR888
#define PIXFORMAT_RAW8      CAM_SENSOR_PIXFORMAT_RAW8
#define PIXFORMAT_RAW10     CAM_SENSOR_PIXFORMAT_RAW10
#define PIXFORMAT_RAW12     CAM_SENSOR_PIXFORMAT_RAW12
#define PIXFORMAT_RGB444    CAM_SENSOR_PIXFORMAT_RGB444
#define PIXFORMAT_RGB555    CAM_SENSOR_PIXFORMAT_RGB555
// #define PIXFORMAT_BGR565
#define PIXFORMAT_BGR888    CAM_SENSOR_PIXFORMAT_BGR888

typedef uint32_t pixformat_t;

typedef enum color_encoding {
    color_encoding_RGB = 0,
    color_encoding_YUV = 1,
    color_encoding_RAW = 2,
    color_encoding_invalid = 3,
} color_encoding_t;

/**
 * @brief Get bits per pixel of format.
 *
 * @param format Video data format
 *
 * @return bits per pixel of format
 */
uint8_t esp_video_get_bpp_by_format(uint32_t format);

/**
 * @brief Get encoding of format.
 *
 * @param format Video data format
 *
 * @return Encoding of format
 */
color_encoding_t esp_video_get_encoding_by_format(uint32_t format);

#ifdef __cplusplus
}
#endif
