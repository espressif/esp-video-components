/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_sensor_types.h"
#include "sc030iot_types.h"

#define SC030IOT_SCCB_ADDR   0x68
#define SC030IOT_PID         0x9a46
#define SC030IOT_SENSOR_NAME "SC030IOT"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *sc030iot_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
