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
#include "gc2053_types.h"

#define GC2053_SCCB_ADDR   0x37
#define GC2053_PID         0x2053
#define GC2053_SENSOR_NAME "GC2053"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *gc2053_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
