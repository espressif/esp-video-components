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
#include "sc1346_types.h"

#define SC1346_SCCB_ADDR 0x30
#define SC1346_PID       0xda4d
#define SC1346_SENSOR_NAME "SC1346"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *sc1346_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
