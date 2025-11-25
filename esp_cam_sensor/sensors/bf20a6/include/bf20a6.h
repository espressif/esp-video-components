/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_sensor_types.h"
#include "bf20a6_types.h"

#define BF20A6_SCCB_ADDR   0x6E
#define BF20A6_PID         0x20a6
#define BF20A6_SENSOR_NAME "BF20A6"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *bf20a6_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
