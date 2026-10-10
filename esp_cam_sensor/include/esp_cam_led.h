/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_check.h"
#include "esp_cam_led_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Perform an ioctl request on the LED.
 *
 * @param[in] dev LED device handle.
 * @param[in] cmd The ioctl command, see esp_cam_led_types.h.
 * @param[in] arg The argument accompanying the ioctl command.
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The LED driver does not support this cmd or arg.
 */
esp_err_t esp_cam_led_ioctl(esp_cam_led_device_t *dev, uint32_t cmd, void *arg);

/**
 * @brief Get the module name of the current LED device.
 *
 * @param[in] dev LED device handle.
 * @return
 *      - LED module name on success, or "NULL"
 */
const char *esp_cam_led_get_name(esp_cam_led_device_t *dev);

/**
 * @brief Delete LED device
 *
 * @param[in] dev LED device handle.
 * @return
 *        - ESP_OK: If LED is successfully deleted.
 */
esp_err_t esp_cam_led_del_dev(esp_cam_led_device_t *dev);

#ifdef __cplusplus
}
#endif
