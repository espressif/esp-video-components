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
#include "ov3640_types.h"

#define OV3640_SCCB_ADDR   0x3c // 0x78
#define OV3640_PID1        0x364C
#define OV3640_PID2        0x3641
#define OV3640_SENSOR_NAME "OV3640"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *ov3640_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
