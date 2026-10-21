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
#include "jxh63p_types.h"

#define JXH63P_SCCB_ADDR   0x40
#define JXH63P_PID         0x0848
#define JXH63P_SENSOR_NAME "JXH63P"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *jxh63p_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
