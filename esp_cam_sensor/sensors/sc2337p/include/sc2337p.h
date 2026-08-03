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
#include "sc2337p_types.h"
#if CONFIG_CAMERA_SC2337P_SID_HIGH
#define SC2337P_SCCB_ADDR   0x32
#else
#define SC2337P_SCCB_ADDR   0x30
#endif
#define SC2337P_PID1         0x9b3a
#define SC2337P_PID2         0xcb3a
#define SC2337P_SENSOR_NAME "SC2337P"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *sc2337p_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
