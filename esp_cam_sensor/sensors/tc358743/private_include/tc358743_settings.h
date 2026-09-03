/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "tc358743_regs.h"
#include "tc358743_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_CAMERA_TC358743_MIPI_RGB888_1920X1080_30FPS
#include "tc358743_mipi_2lane_27Minput_1920x1080_rgb888_30fps.h"
#endif
#if CONFIG_CAMERA_TC358743_MIPI_YUV422_1920X1080_30FPS
#include "tc358743_mipi_2lane_27Minput_1920x1080_yuv422_30fps.h"
#endif

#ifdef __cplusplus
}
#endif
