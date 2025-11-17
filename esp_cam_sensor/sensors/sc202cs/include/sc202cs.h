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
#include "sc202cs_types.h"

#define SC202CS_SCCB_ADDR   0x36
#define SC202CS_PID         0xeb52
#define SC202CS_SENSOR_NAME "SC202CS"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *sc202cs_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
