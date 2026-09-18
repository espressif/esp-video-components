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
#include "gc032a_types.h"

#define GC032A_SCCB_ADDR   0x21
#define GC032A_PID         0x232a
#define GC032A_SENSOR_NAME "GC032A"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *gc032a_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
