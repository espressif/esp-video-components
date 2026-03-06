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
#include "mira220_types.h"

#define MIRA220_SCCB_ADDR   0x54
#define MIRA220_PID         0x130 // 0xcb3a
#define MIRA220_SENSOR_NAME "MIRA220"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *mira220_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
