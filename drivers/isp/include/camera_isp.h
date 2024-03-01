/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_color_formats.h"

int isp_init(uint32_t frame_width, uint32_t frame_height, pixformat_t in_format, pixformat_t out_format, bool isp_enable, void *sensor_info);
