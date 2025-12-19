/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "os02n10_regs.h"
#include "os02n10_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED

#include "os02n10_mipi_2lane_24Minput_960x540_raw8_binning_25fps.h"

#include "os02n10_mipi_2lane_24Minput_1280x720_raw8_50fps.h"

#include "os02n10_mipi_2lane_24Minput_1280x720_raw10_50fps.h"

#include "os02n10_mipi_2lane_24Minput_1920x1080_raw8_25fps.h"

#include "os02n10_mipi_2lane_24Minput_1920x1080_raw10_25fps.h"

#endif

#ifdef __cplusplus
}
#endif
