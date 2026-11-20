/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************
Copyright (C), 2026, MetaSilicon Tech. Co., Ltd.
All rights reserved.
****************************************************/

#pragma once

#include "esp_cam_sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAT130A_SCCB_ADDR   0x20
#define MAT130A_PID         0x0130
#define MAT130A_SENSOR_NAME "MAT130A"

esp_cam_sensor_device_t *mat130a_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
