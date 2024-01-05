/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_camera.h"
#include "dvp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create DVP video device
 *
 * @param cam_dev camera devcie
 * @param port    DVP port
 * @param pin     DVP pin configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_create_camera_video_device(esp_camera_device_t *cam_dev, uint8_t port, const dvp_pin_config_t *pin);

#ifdef __cplusplus
}
#endif
