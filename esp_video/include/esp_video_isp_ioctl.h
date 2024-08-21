/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "hal/isp_types.h"
#include <linux/v4l2-controls.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The base for the ESP32XX SoCes driver controls.
 */
#define V4L2_CID_USER_ESP_ISP_BASE          (V4L2_CID_USER_BASE + 0x10e0)

#define V4L2_CID_USER_ESP_ISP_CCM           (V4L2_CID_USER_ESP_ISP_BASE + 0x0000)   /*!< CCM V4L2 controller ID */
#define V4L2_CID_USER_ESP_ISP_GAMMA         (V4L2_CID_USER_ESP_ISP_BASE + 0x0001)   /*!< GAMMA V4L2 controller ID */
#define V4L2_CID_USER_ESP_ISP_BF            (V4L2_CID_USER_ESP_ISP_BASE + 0x0002)   /*!< BF V4L2 controller ID */
#define V4L2_CID_USER_ESP_ISP_SHARPEN       (V4L2_CID_USER_ESP_ISP_BASE + 0x0003)   /*!< Sharpen V4L2 controller ID */

/**
 * @brief ESP32XXX ISP image statistics output, data type is "esp_ipa_stats_t"
 */
#define V4L2_META_FMT_ESP_ISP_STATS         v4l2_fourcc('E', 'S', 'T', 'A')

/**
 * @brief GAMMA point coordinate.
 */
typedef struct esp_video_isp_gamma_point {
    uint8_t x;          /*!< GAMMA point X coordinate */
    uint8_t y;          /*!< GAMMA point Y coordinate */
} esp_video_isp_gamma_point_t;

/**
 * @brief ISP CCM configuration.
 */
typedef struct esp_video_isp_ccm {
    bool enable;        /*!< true: enable CCM, false: disable CCM */

    /**
     * CCM data matrix.
     *
     * ESP32-P4 supports 3x3 matrix and data range is (-4, 4)
     */

    float matrix[ISP_CCM_DIMENSION][ISP_CCM_DIMENSION];
} esp_video_isp_ccm_t;

/**
 * @brief ISP GAMMA configuration.
 */
typedef struct esp_video_isp_gamma {
    bool enable;        /*!< true: enable GAMMA, false: disable GAMMA */

    /**
     * GAMMA points coordinates.
     *
     * ESP32-P4 supports 16 points coordinate, each Y coordinate is 8-bit data,
     * the difference between every 2 X coordinates must be the Nth power of 2.
     * For example:
     *      points[1].x - points[2].x = 4 = 2^2
     */
    esp_video_isp_gamma_point_t points[ISP_GAMMA_CURVE_POINTS_NUM];
} esp_video_isp_gamma_t;

/**
 * @brief ISP BF(bayer filter) configuration.
 */
typedef struct esp_video_isp_bf {
    bool enable;        /*!< true: enable BF, false: disable BF */
    uint8_t level;      /*!< BF denoising level, ESP32-P4's range is [2, 20] */

    /**
     * BF filter matrix.
     *
     * ESP32-P4 supports 3x3 matrix and data range is [0, 15].
     */
    uint8_t matrix[ISP_BF_TEMPLATE_X_NUMS][ISP_BF_TEMPLATE_Y_NUMS];
} esp_video_isp_bf_t;

/**
 * @brief Sharpen configuration.
 */
typedef struct esp_video_isp_sharpen {
    bool enable;        /*!< true: enable sharpen, false: disable sharpen */

    uint8_t h_thresh;   /*!< Sharpen high threshold of high frequency component */
    uint8_t l_thresh;   /*!< Sharpen low threshold of high frequency component */

    /**
     * Sharpen threshold coefficient.
     *
     * ESP32-P4 supports integer type and data range is [0, 255/32], unit is 1/32.
     */

    float h_coeff;      /*!< Sharpen coefficient of high threshold */
    float m_coeff;      /*!< Sharpen coefficient of middle threshold(value between "l_thresh" and "h_thresh" ) */

    /**
     * Sharpen low-pass filter matrix.
     *
     * ESP32-P4 supports 3x3 matrix and data range is [0, 31].
     */
    uint8_t matrix[ISP_SHARPEN_TEMPLATE_X_NUMS][ISP_SHARPEN_TEMPLATE_Y_NUMS];
} esp_video_isp_sharpen_t;

#ifdef __cplusplus
}
#endif
