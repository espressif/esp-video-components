/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_cam_sensor_types.h"

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

#ifdef __cplusplus
}
#endif
