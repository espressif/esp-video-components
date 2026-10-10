/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_cam_als_types.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Detect Ambient Light Sensor.
 *
 * @param config Pointer to the Ambient Light Sensor configuration.
 * @param ret_dev Pointer to the Ambient Light Sensor device.
 * @return ESP_OK on success, Others if failed.
 */
esp_err_t esp_cam_als_detect(const esp_cam_als_config_t *config, esp_cam_als_device_t **ret_dev);

#ifdef __cplusplus
}
#endif
