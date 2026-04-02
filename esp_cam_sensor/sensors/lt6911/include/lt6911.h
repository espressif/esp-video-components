/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_sensor_types.h"
#include "lt6911_types.h"

#define LT6911_SCCB_ADDR   0x2B
#define LT6911_PID         0x2102
#define LT6911_SENSOR_NAME "LT6911"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *lt6911_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
