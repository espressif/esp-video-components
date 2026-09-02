/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_cam_sensor_types.h"
#include "hal/cam_ctlr_types.h"
#include "linux/videodev2.h"
#include "esp_video_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_video_dvp_in_out_format {
    cam_ctlr_color_t in_color;   /*!< DVP controller input color (sensor) */
    cam_ctlr_color_t out_color;  /*!< DVP controller output color (after optional conversion) */
} esp_video_dvp_in_out_format_t;

/**
 * Enumerate DVP capture pixel formats for the given sensor format.
 *
 * When ``ESP_VIDEO_DVP_DEVICE_CONV_FORMAT`` is enabled and the sensor outputs YUV422 YUYV,
 * index 0 is the sensor native format and a later index is ``V4L2_PIX_FMT_RGB565X``.
 * Converted RGB565 is big-endian (``V4L2_PIX_FMT_RGB565X``), not little-endian (``V4L2_PIX_FMT_RGB565``).
 *
 * @param sensor_fmt Sensor output format
 * @param index Format index
 * @param pixel_format Output V4L2 pixel format
 *
 * @return ESP_OK, ESP_ERR_INVALID_ARG if index is out of range, or ESP_ERR_NOT_SUPPORTED
 */
esp_err_t esp_video_dvp_enum_format(esp_cam_sensor_output_format_t sensor_fmt, uint32_t index, uint32_t *pixel_format);

/**
 * Check whether the requested V4L2 format can be produced by DVP for the sensor format.
 *
 * @param sensor_fmt Sensor output format
 * @param v4l2_fmt Requested V4L2 format; must be a non-zero FOURCC (callers that want the
 *                 sensor native format must pass that FOURCC explicitly)
 * @param in_out_format Output input/output color pair
 *
 * @return ESP_OK if supported, ESP_ERR_INVALID_ARG if ``v4l2_fmt`` is 0, otherwise an error
 */
esp_err_t esp_video_dvp_check_format(esp_cam_sensor_output_format_t sensor_fmt, uint32_t v4l2_fmt, esp_video_dvp_in_out_format_t *in_out_format);

#ifdef __cplusplus
}
#endif
