/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "linux/videodev2.h"
#include "esp_cam_sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VIDIOC_S_SENSOR_FMT _IOWR('V',  BASE_VIDIOC_PRIVATE + 1, esp_cam_sensor_format_t)
#define VIDIOC_G_SENSOR_FMT _IOWR('V',  BASE_VIDIOC_PRIVATE + 2, esp_cam_sensor_format_t)

#ifdef __cplusplus
}
#endif
