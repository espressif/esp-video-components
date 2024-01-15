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
 * @brief Create MIPI-CSI video device
 *
 * @param cam_dev camera devcie
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t csi_create_camera_video_device(esp_camera_device_t *cam_dev);

#ifdef __cplusplus
}
#endif
