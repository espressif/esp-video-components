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
#include "mt9d111_types.h"

#define MT9D111_SCCB_ADDR   0x48
#define MT9D111_PID         0x1519
#define MT9D111_SENSOR_NAME "MT9D111"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *mt9d111_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
