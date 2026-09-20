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
 * @brief Power on IR2835 LED device and detect the device connected to the designated GPIO pins.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera LED device handle on success, otherwise, failed.
 */
esp_cam_led_device_t *ir2835_detect(const esp_cam_led_config_t *config);

#ifdef __cplusplus
}
#endif
