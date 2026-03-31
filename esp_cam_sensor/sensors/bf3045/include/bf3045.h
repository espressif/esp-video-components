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
#include "bf3045_types.h"

#define BF3045_SCCB_ADDR   0x6E
#define BF3045_PID         0x3003
#define BF3045_SENSOR_NAME "BF3045"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *bf3045_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
