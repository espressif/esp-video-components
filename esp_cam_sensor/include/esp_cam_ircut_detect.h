/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_cam_ircut_types.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Detect camera IRCUT.
 *
 * @param config Pointer to the camera IRCUT configuration.
 * @param ret_dev Pointer to the camera IRCUT device.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on failure.
 */
esp_err_t esp_cam_ircut_detect(const esp_cam_ircut_config_t *config, esp_cam_ircut_device_t **ret_dev);

#ifdef __cplusplus
}
#endif
