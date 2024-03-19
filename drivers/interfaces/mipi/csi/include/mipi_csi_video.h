/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Camera controller interface
 */
typedef enum cam_ctrl_port {
    ESP_VIDEO_CAM_INTF_MIPI_CSI = 0,    /*!< MIPI-CSI port for ESP32-P4 */
    ESP_VIDEO_CAM_INTF_ISP_DVP,         /*!< ISP-DVP port for ESP32-P4, not support yet */
    ESP_VIDEO_CAM_INTF_GENERA_DVP,      /*!< I2S port for ESP32 and ESP32-S2, LCD_CAM port for ESP32-S3 and ESP32-P4, not support yet */
    ESP_VIDEO_CAM_INTF_MAX              /*!< Camera controller interface max number */
} esp_video_cam_intf_t;

/**
 * @brief Create camera controller video device
 *
 * @param cam_dev camera sensor devcie
 * @param intf    camera controller interface
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_cam_device(esp_camera_device_t *cam_dev, esp_video_cam_intf_t intf);

#ifdef __cplusplus
}
#endif
