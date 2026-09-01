/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_check.h"
#include "esp_cam_ircut_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Perform an ioctl request on the camera IRCUT.
 *
 * @param[in] dev Camera IRCUT device handle.
 * @param[in] cmd The ioctl command, see esp_cam_ircut_types.h.
 * @param[in] arg The argument accompanying the ioctl command.
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The IRCUT driver does not support this cmd or arg.
 */
esp_err_t esp_cam_ircut_ioctl(esp_cam_ircut_device_t *dev, uint32_t cmd, void *arg);

/**
 * @brief Get the module name of the current camera device.
 *
 * @param[in] dev Camera IRCUT device handle.
 * @return
 *      - Camera module name on success, or "NULL"
 */
const char *esp_cam_ircut_get_name(esp_cam_ircut_device_t *dev);

/**
 * @brief Delete camera device
 *
 * @param[in] dev Camera IRCUT device handle.
 * @return
 *        - ESP_OK: If Camera IRCUT is successfully deleted.
 */
esp_err_t esp_cam_ircut_del_dev(esp_cam_ircut_device_t *dev);

#ifdef __cplusplus
}
#endif
