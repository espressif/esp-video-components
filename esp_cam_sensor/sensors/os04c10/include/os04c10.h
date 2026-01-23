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
#include "os04c10_types.h"

#if CONFIG_CAMERA_OS04C10_SID_HIGH
#define OS04C10_SCCB_ADDR 0x10 // SCCB ID select to 0x10 if SID pin set to 1
#else
#define OS04C10_SCCB_ADDR 0x36 // SCCB ID select to 0x36 if SID pin set to 0
#endif
#define OS04C10_PID         0x5304
#define OS04C10_SENSOR_NAME "OS04C10"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *os04c10_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
