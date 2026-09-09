/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_als_types.h"

/**
 * @brief Power on PT1411 ALS device and detect the device connected to the designated GPIO pins.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera ALS device handle on success, otherwise, failed.
 */
esp_cam_als_device_t *pt1411_detect(const esp_cam_als_config_t *config);

#ifdef __cplusplus
}
#endif
