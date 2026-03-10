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
#include "sp0a39_types.h"

#define SP0A39_SCCB_ADDR   0x21
#define SP0A39_PID         0x0a39
#define SP0A39_SENSOR_NAME "SP0A39"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *sp0a39_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
