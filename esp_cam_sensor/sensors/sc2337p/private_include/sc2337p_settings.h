/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "sc2337p_regs.h"
#include "sc2337p_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED

#if CONFIG_CAMERA_SC2337P_MIPI_RAW10_1088X1080_30FPS
#include "sc2337p_mipi_2lane_24Minput_1088x1080_raw10_30fps.h"
#endif
#if CONFIG_CAMERA_SC2337P_MIPI_RAW10_1920X1080_30FPS
#include "sc2337p_mipi_2lane_24Minput_1920x1080_raw10_30fps.h"
#endif
#if CONFIG_CAMERA_SC2337P_MIPI_RAW8_1920X1080_30FPS
#include "sc2337p_mipi_2lane_24Minput_1920x1080_raw8_30fps.h"
#endif

#endif

#ifdef __cplusplus
}
#endif
