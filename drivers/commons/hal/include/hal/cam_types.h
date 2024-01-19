/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "soc/soc_caps_extra.h"
#include "soc/clk_tree_defs_extra.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SOC_CAM_SUPPORTED
/**
 * @brief CAM clock source
 */
typedef soc_periph_cam_clk_src_t cam_clock_source_t;
#else
typedef int cam_clock_source_t;
#endif

/**
 * @brief CAM color range
 */
typedef enum {
    CAM_COLOR_RANGE_LIMIT = 0, /*!< Limited color range */
    CAM_COLOR_RANGE_FULL,      /*!< Full color range */
} cam_color_range_t;

/**
 * @brief YUV sampling method
 */
typedef enum {
    CAM_YUV_SAMPLE_422 = 0, /*!< YUV 4:2:2 sampling */
    CAM_YUV_SAMPLE_420,     /*!< YUV 4:2:0 sampling */
    CAM_YUV_SAMPLE_411,     /*!< YUV 4:1:1 sampling */
} cam_yuv_sample_t;

/**
 * @brief The standard used for conversion between RGB and YUV
 */
typedef enum {
    CAM_YUV_CONV_STD_BT601 = 0, /*!< YUV<->RGB conversion standard: BT.601 */
    CAM_YUV_CONV_STD_BT709, /*!< YUV<->RGB conversion standard: BT.709 */
} cam_yuv_conv_std_t;

#ifdef __cplusplus
}
#endif
