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
#include "sti2250_types.h"

#define STI2250_SCCB_ADDR   0x37 // 0x37 if IDSEL == Low, 0x10 if IDSEL == High
#define STI2250_PID         0x2250
#define STI2250_SENSOR_NAME "STI2250"

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *sti2250_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
