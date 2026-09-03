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
#include "tc358743_types.h"

#define TC358743_SCCB_ADDR   0x0f
#define TC358743_PID         0x00
#define TC358743_SENSOR_NAME "TC358743"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *tc358743_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
