/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_cam_ircut_detect.h"

#if CONFIG_CAM_IRCUT_AP1511B
#include "ap1511b.h"
#endif

esp_err_t esp_cam_ircut_detect(const esp_cam_ircut_config_t *config, esp_cam_ircut_device_t **ret_dev)
{
    if (config == NULL || ret_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_CAM_IRCUT_AP1511B
    *ret_dev = ap1511b_detect(config);
#else
    ESP_LOGE("ircut_detect", "No device was selected.");
    return ESP_ERR_NOT_SUPPORTED;
#endif
    if (*ret_dev == NULL) {
        ESP_LOGE("ircut_detect", "Failed to detect.");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}
