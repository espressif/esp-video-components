/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_cam_led_types.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Detect LED.
 *
 * @param config Pointer to the LED configuration.
 * @param ret_dev Pointer to the LED device.
 * @return ESP_OK on success, Others if failed.
 */
esp_err_t esp_cam_led_detect(const esp_cam_led_config_t *config, esp_cam_led_device_t **ret_dev);

#ifdef __cplusplus
}
#endif
