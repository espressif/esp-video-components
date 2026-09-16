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

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_cam_sensor_types.h"
#include "mit245_types.h"

/* Datasheet gives 8-bit W/R addresses 0x40/0x41; esp_sccb uses 7-bit address. */
#define MIT245_SCCB_ADDR   0x20
#define MIT245_PID         0x0245
#define MIT245_SENSOR_NAME "MIT245"

esp_cam_sensor_device_t *mit245_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
