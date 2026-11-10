/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_led_types.h"

/**
 * @brief Configure the LEDC PWM output and create the LED device, the LED is off after this call.
 *
 * @param[in] config Configuration of the LED, type must be ESP_CAM_LED_HW_TYPE_LEDC.
 * @return
 *      - Camera LED device handle on success, otherwise, failed.
 */
esp_cam_led_device_t *tcj60_detect(const esp_cam_led_config_t *config);

#ifdef __cplusplus
}
#endif
