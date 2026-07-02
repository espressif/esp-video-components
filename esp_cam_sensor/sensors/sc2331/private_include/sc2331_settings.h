/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "sc2331_regs.h"
#include "sc2331_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED

#include "sc2331_mipi_2lane_24Minput_1920x1080_raw10_25fps.h"
#include "sc2331_mipi_2lane_24Minput_1920x1080_raw8_5fps.h"
#include "sc2331_mipi_2lane_24Minput_1920x1080_raw8_25fps.h"
#include "sc2331_mipi_2lane_24Minput_1920x1080_raw8_30fps.h"
#include "sc2331_mipi_2lane_24Minput_1280x720_raw8_25fps.h"
#include "sc2331_mipi_2lane_24Minput_1280x720_raw8_30fps.h"

#endif

#ifdef __cplusplus
}
#endif
