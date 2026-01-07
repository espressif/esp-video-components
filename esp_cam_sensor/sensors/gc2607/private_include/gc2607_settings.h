/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "gc2607_regs.h"
#include "gc2607_types.h"

#ifdef __cplusplus
extern "C" {
#endif

static const gc2607_reginfo_t gc2607_stream_off[] = {
    {0x03fe, 0x00},
    {0x0117, 0x01},
    {0x0229, 0x03},
    {0x0100, 0x81},
};

static const gc2607_reginfo_t gc2607_stream_on[] = {
    {0x03fe, 0x20},
    {0x03fe, 0x00},
    {0x0117, 0x91},
    {0x0229, 0x02},
    {0x0100, 0x00},
};

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
#if CONFIG_CAMERA_GC2607_MIPI_RAW10_1920X1080_25FPS
#include "gc2607_mipi_2lane_24Minput_raw10_1920x1080_25fps.h"
#endif
#endif

#ifdef __cplusplus
}
#endif
