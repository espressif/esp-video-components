/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_cam_sensor_types.h"
#include "driver/jpeg_encode.h"
#include "esp_video_device.h"
#include "hal/cam_ctlr_types.h"

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

#if CONFIG_ESP_VIDEO_ENABLE_ISP
/**
 * @brief Start ISP process
 *
 * @param bypass    Bypass ISP and MIPI-CSI output sensor original data
 * @param in_color  ISP input image color type
 * @param out_color ISP putput image color type
 * @param line_sync true: sensor data has no sync signal, false: sensor data has no sync signal
 * @param width     Image width
 * @param height    Image height
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_start(bool bypass, cam_ctlr_color_t in_color, cam_ctlr_color_t out_color,
                              bool line_sync, uint32_t width, uint32_t height);

/**
 * @brief Stop ISP process
 *
 * @param bypass true: bypass ISP and MIPI-CSI output sensor original data, false: ISP process image
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_stop(bool bypass);

/**
 * @brief Enumerate ISP supported output pixel format
 *
 * @param index        Enumerated number index
 * @param pixel_format Supported output pixel format
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_enum_format(uint32_t index, uint32_t *pixel_format);

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
/**
 * @brief Create ISP video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_isp_video_device(void);
#endif
#endif

#ifdef __cplusplus
}
#endif
