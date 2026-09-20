/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_cam_led_detect.h"

#if CONFIG_CAM_LED_IR2835
#include "ir2835.h"
#endif

esp_err_t esp_cam_led_detect(const esp_cam_led_config_t *config, esp_cam_led_device_t **ret_dev)
{
    if (config == NULL || ret_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_CAM_LED_IR2835
    *ret_dev = ir2835_detect(config);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
    if (*ret_dev == NULL) {
        ESP_LOGE("led_detect", "Failed to detect.");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}
