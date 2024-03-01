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
 * @brief Camera controller port
 */
typedef enum cam_ctrl_port {
    MIPI_CSI_PORT = 0,      /*!< MIPI-CSI port for ESP32-P4 */
    ISP_DVP_PORT,           /*!< ISP-DVP port for ESP32-P4, not support yet */
    DEFAULT_DVP_PORT,       /*!< I2S port for ESP32 and ESP32-S2, LCD_CAM port for ESP32-S3 and ESP32-P4, not support yet */
    CAM_CTRL_PORT_MAX       /*!< Camera controller port max number */
} cam_ctrl_port_t;

/**
 * @brief Create camera controller video device
 *
 * @param cam_dev camera sensor devcie
 * @param port    camera controller port
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t cam_ctrl_create_video_device(esp_camera_device_t *cam_dev, cam_ctrl_port_t port);

#ifdef __cplusplus
}
#endif
