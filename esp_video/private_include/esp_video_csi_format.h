/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_video.h"
#include "esp_video_cam.h"
#include "esp_video_internal.h"
#include "driver/isp.h"
#include "esp_cam_ctlr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_video_csi_isp_in_out_format {
    bool isp_bypass_required;
    isp_color_t isp_input_fmt;
    isp_color_t isp_output_fmt;
    uint8_t isp_bpp;                            /**< Bits per pixel of ISP output format, this used when ISP is in bypass mode */

    cam_ctlr_color_t csi_input_fmt;
    cam_ctlr_color_t csi_output_fmt;
} esp_video_csi_isp_in_out_format_t;

/**
 * Check if the requested V4L2 format can be supported by the CSI-ISP pipeline
 *
 * This function validates if the requested output format can be produced
 * given the sensor's output format and the capabilities of ISP and MIPI-CSI.
 *
 * @param sensor_fmt Sensor output format
 * @param v4l2_fmt V4L2 format, if 0, use the sensor format
 * @param in_out_format Pointer to the CSI-ISP input/output format structure
 *
 * @return ESP_OK if format is supported, ESP_ERR_NOT_SUPPORTED otherwise
 */
esp_err_t esp_video_csi_check_format(esp_cam_sensor_output_format_t sensor_fmt, uint32_t v4l2_fmt, esp_video_csi_isp_in_out_format_t *in_out_format);

/**
 * Enumerate all supported output formats for the CSI-ISP pipeline
 *
 * This function lists all possible output formats given the current
 * sensor output format and hardware capabilities.
 *
 * @param sensor_fmt Sensor output format
 * @param index Index of the format to enumerate
 * @param pixel_format Pointer to store the enumerated pixel format
 *
 * @return ESP_OK if format found, ESP_ERR_INVALID_ARG if index out of range
 */
esp_err_t esp_video_csi_enum_format(esp_cam_sensor_output_format_t sensor_fmt, uint32_t index, uint32_t *pixel_format);

#ifdef __cplusplus
}
#endif
