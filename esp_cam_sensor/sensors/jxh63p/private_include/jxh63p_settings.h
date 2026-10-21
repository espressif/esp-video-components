/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "jxh63p_regs.h"
#include "jxh63p_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
#if CONFIG_CAMERA_JXH63P_MIPI_RAW10_1280X720_30FPS
#include "jxh63p_mipi_1lane_24Minput_raw10_1280x720_30fps.h"
#endif
#if CONFIG_CAMERA_JXH63P_MIPI_RAW10_640X480_30FPS
#include "jxh63p_mipi_1lane_24Minput_raw10_640x480_30fps.h"
#endif
#endif

#ifdef __cplusplus
}
#endif
