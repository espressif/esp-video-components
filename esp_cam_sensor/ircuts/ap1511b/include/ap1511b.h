/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_ircut_types.h"

/**
 * @brief Power on AP1511B IRCUT device and detect the device connected to the designated GPIO pins.
 *
 * @param[in] config Configuration related to device power-on and detection.
 * @return
 *      - Camera IRCUT device handle on success, otherwise, failed.
 */
esp_cam_ircut_device_t *ap1511b_detect(const esp_cam_ircut_config_t *config);

#ifdef __cplusplus
}
#endif
