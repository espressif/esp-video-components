/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "sc2336_regs.h"
#include "sc2336_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED

#include "sc2336_mipi_1lane_24Minput_1920x1080_raw10_25fps.h"

#include "sc2336_mipi_2lane_24Minput_640x480_raw10_50fps.h"

#include "sc2336_mipi_2lane_24Minput_800x800_raw8_30fps.h"

#include "sc2336_mipi_2lane_24Minput_800x800_raw10_30fps.h"

#include "sc2336_mipi_2lane_24Minput_1024x600_raw8_30fps.h"

#include "sc2336_mipi_2lane_24Minput_1280x720_raw8_30fps.h"

#include "sc2336_mipi_2lane_24Minput_1280x720_raw10_25fps.h"

#include "sc2336_mipi_2lane_24Minput_1280x720_raw10_30fps.h"

#include "sc2336_mipi_2lane_24Minput_1280x720_raw10_50fps.h"

#include "sc2336_mipi_2lane_24Minput_1280x720_raw10_60fps.h"

#include "sc2336_mipi_2lane_24Minput_1920x1080_raw8_30fps.h"

#include "sc2336_mipi_2lane_24Minput_1920x1080_raw10_15fps.h"

#include "sc2336_mipi_2lane_24Minput_1920x1080_raw10_25fps.h"

#include "sc2336_mipi_2lane_24Minput_1920x1080_raw10_30fps.h"

#endif

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED

#include "sc2336_dvp_8bit_24Minput_1280x720_raw10_30fps.h"

#endif

#ifdef __cplusplus
}
#endif
