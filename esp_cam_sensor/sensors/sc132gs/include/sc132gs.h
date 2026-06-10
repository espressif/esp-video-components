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
#include "sc132gs_types.h"

#if CONFIG_CAMERA_SC132GS_SCCB_ID_30
#define SC132GS_SCCB_ADDR   0x30
#elif CONFIG_CAMERA_SC132GS_SCCB_ID_31
#define SC132GS_SCCB_ADDR   0x31
#elif CONFIG_CAMERA_SC132GS_SCCB_ID_32
#define SC132GS_SCCB_ADDR   0x32
#elif CONFIG_CAMERA_SC132GS_SCCB_ID_33
#define SC132GS_SCCB_ADDR   0x33
#endif
#define SC132GS_PID          0x0132
#define SC132GS_SENSOR_NAME "SC132GS"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *sc132gs_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
