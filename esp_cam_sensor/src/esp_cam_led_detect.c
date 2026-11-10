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
#if CONFIG_CAM_LED_TCJ60
#include "tcj60.h"
#endif

esp_err_t esp_cam_led_detect(const esp_cam_led_config_t *config, esp_cam_led_device_t **ret_dev)
{
    if (config == NULL || ret_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#if !CONFIG_CAM_LED_IR2835 && !CONFIG_CAM_LED_TCJ60
    return ESP_ERR_NOT_SUPPORTED;
#endif
    *ret_dev = NULL;
    /* Each driver serves one hardware type */
#if CONFIG_CAM_LED_IR2835
    if (config->type == ESP_CAM_LED_HW_TYPE_GPIO) {
        *ret_dev = ir2835_detect(config);
    }
#endif
#if CONFIG_CAM_LED_TCJ60
    if (config->type == ESP_CAM_LED_HW_TYPE_LEDC) {
        *ret_dev = tcj60_detect(config);
    }
#endif
    if (*ret_dev == NULL) {
        ESP_LOGE("led_detect", "Failed to detect.");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}
