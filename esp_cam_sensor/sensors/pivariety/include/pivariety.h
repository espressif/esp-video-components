/*
 * SPDX-FileCopyrightText: 2026 Arducam Electronic Technology (Nanjing) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_sensor_types.h"
#include "pivariety_types.h"
#define PIVARIETY_SENSOR_NAME "PIVARIETY"
#define PIVARIETY_PID         0x00000030
#define PIVARIETY_SCCB_ADDR   0x0c

/**
 * @brief Power on camera sensor device and detect the device connected to the designated sccb bus.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera device handle on success, otherwise, failed.
 */
esp_cam_sensor_device_t *pivariety_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
