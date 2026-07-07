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
#include "gc4053_types.h"

#if CONFIG_CAMERA_GC4053_SID_HIGH
#define GC4053_SCCB_ADDR 0x10
#else
#define GC4053_SCCB_ADDR 0x31
#endif

#define GC4053_PID         0x4053
#define GC4053_SENSOR_NAME "GC4053"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *gc4053_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
