/*
 * SPDX-FileCopyrightText: 2026 Shenzhen ALG-TECH Co., Ltd.,
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_sensor_types.h"
#include "sc121at_types.h"

#define SC121AT_SCCB_ADDR   0x30
#define SC121AT_PID         0x2ada
#define SC121AT_SENSOR_NAME "SC121AT"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *sc121at_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
