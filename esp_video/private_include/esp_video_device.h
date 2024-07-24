/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_cam_sensor_types.h"
#include "driver/jpeg_encode.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create MIPI CSI video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t *cam_dev);
#endif

/**
 * @brief Create DVP video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
esp_err_t esp_video_create_dvp_video_device(esp_cam_sensor_device_t *cam_dev);
#endif

/**
 * @brief Create H.264 video device
 *
 * @param hw_codec true: hardware H.264, false: software H.264(has not supported)
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#ifdef CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
esp_err_t esp_video_create_h264_video_device(bool hw_codec);
#endif

/**
 * @brief Create JPEG video device
 *
 * @param enc_handle JPEG encoder driver handle,
 *      - NULL, JPEG video device will create JPEG encoder driver handle by itself
 *      - Not null, JPEG video device will use this handle instead of creating JPEG encoder driver handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#ifdef CONFIG_ESP_VIDEO_ENABLE_JPEG_VIDEO_DEVICE
esp_err_t esp_video_create_jpeg_video_device(jpeg_encoder_handle_t enc_handle);
#endif

#ifdef __cplusplus
}
#endif
