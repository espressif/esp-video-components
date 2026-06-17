/*
 * SPDX-FileCopyrightText: 2026 Shenzhen ALG-TECH Co., Ltd.,
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <sdkconfig.h>
#include "sc121at_regs.h"
#include "sc121at_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SC121AT_SOFT_POWER_DOWN_EN                 (0x01)
#define SC121AT_OUTPUT_ENABLE_DEFAULT              (0)
#define SC121AT_IDI_CLOCK_RATE_1280x800_25FPS      (59000000ULL)
// Note, all clock configurations default to using 2 data lane mode, so use bitwidth divide by 2
#define SC121AT_LINE_RATE_16BITS_1280x800_25FPS     (SC121AT_IDI_CLOCK_RATE_1280x800_25FPS * 8)

#if CONFIG_SOC_MIPI_CSI_SUPPORTED

static const sc121at_reginfo_t sc121at_mipi_reset_regs[] = {
    // Ensure streaming off to make clock lane go into LP-11 state.
    {0x2100, 0x00},
    {SC121AT_REG_DELAY, 0x10},
    // {0x2103, SC121AT_SOFT_POWER_DOWN_EN}, // bit[6]=1: software power down default
    {SC121AT_REG_END, 0x00},
};

static const sc121at_reginfo_t sc121at_mipi_stream_on[] = {
    {0x2100, 0X01},
    {SC121AT_REG_END, 0x00},
};

static const sc121at_reginfo_t sc121at_mipi_stream_off[] = {
    {0x2100, 0x00}, /* Sensor enter LP11*/
    {SC121AT_REG_END, 0x00},
};

#include "sc121at_mipi_2lane_24Minput_1280x800_yuv422_uyvy_25fps.h"

#endif

#ifdef __cplusplus
}
#endif
