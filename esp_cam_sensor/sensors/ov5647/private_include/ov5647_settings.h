/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "sdkconfig.h"
#include "ov5647_regs.h"
#include "ov5647_types.h"

#define BIT(nr)                                    (1UL << (nr))
#define OV5647_IDI_CLOCK_RATE_800x800_50FPS        (100000000ULL)
#define OV5647_MIPI_CSI_LINE_RATE_800x800_50FPS    (OV5647_IDI_CLOCK_RATE_800x800_50FPS * 4)
#define OV5647_IDI_CLOCK_RATE_800x640_50FPS        (100000000ULL)
#define OV5647_MIPI_CSI_LINE_RATE_800x640_50FPS    (OV5647_IDI_CLOCK_RATE_800x640_50FPS * 4)
#define OV5647_IDI_CLOCK_RATE_800x1280_50FPS       (100000000ULL)
#define OV5647_MIPI_CSI_LINE_RATE_800x1280_50FPS   (OV5647_IDI_CLOCK_RATE_800x1280_50FPS * 4)
#define OV5647_IDI_CLOCK_RATE_1920x1080_30FPS      (81666700ULL)
#define OV5647_MIPI_CSI_LINE_RATE_1920x1080_30FPS  (OV5647_IDI_CLOCK_RATE_1920x1080_30FPS * 5)
#define OV5647_IDI_CLOCK_RATE_1280x960_45FPS        (88333333ULL)
#define OV5647_MIPI_CSI_LINE_RATE_1280x960_45FPS    (OV5647_IDI_CLOCK_RATE_1280x960_45FPS * 5)
#define OV5647_8BIT_MODE                           (0x18)
#define OV5647_10BIT_MODE                          (0x1A)
#define OV5647_MIPI_CTRL00_CLOCK_LANE_GATE         BIT(5)
#define OV5647_MIPI_CTRL00_LINE_SYNC_ENABLE        BIT(4)
#define OV5647_MIPI_CTRL00_BUS_IDLE                BIT(2)
#define OV5647_MIPI_CTRL00_CLOCK_LANE_DISABLE      BIT(0)

static const ov5647_reginfo_t ov5647_mipi_reset_regs[] = {
    {0x0100, 0x00}, // enable sleep
    {0x0103, 0x01},
    {OV5647_REG_DELAY, 0x0a},
    {0x4800, BIT(0)},// Ensure streaming off to make `clock lane` go into LP-11 state.
    {OV5647_REG_END, 0x00},
};

#include "ov5647_mipi_2lane_24Minput_800x640_raw8_50fps.h"

#include "ov5647_mipi_2lane_24Minput_800x800_raw8_50fps.h"

#include "ov5647_mipi_2lane_24Minput_800x1280_raw8_50fps.h"

#include "ov5647_mipi_2lane_24Minput_1280x960_raw10_45fps.h"

#include "ov5647_mipi_2lane_24Minput_1920x1080_raw10_30fps.h"

#ifdef __cplusplus
}
#endif
