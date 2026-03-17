/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "pivariety_regs.h"
#include "pivariety_types.h"

#ifdef __cplusplus
extern "C" {
#endif

static const pivariety_reginfo_t pivariety_mipi_stream_on[] = {
    {STREAM_ON,            0x00000001},
    {PIVARIETY_REG_END,    0x00000000},
};

static const pivariety_reginfo_t pivariety_mipi_stream_off[] = {
    {STREAM_ON,            0x00000000},
    {PIVARIETY_REG_END,    0x00000000},
};

static const pivariety_reginfo_t pivariety_MIPI_2lane_raw10_1920x1080_30fps[] = {
    {PIXFORMAT_INDEX_REG,  0x00000000},
    {RESOLUTION_INDEX_REG, 0x00000000},
    {STREAM_ON,            0x00000001},
    {PIVARIETY_REG_END,    0x00000000},
};

static const pivariety_reginfo_t pivariety_MIPI_2lane_raw10_1600x1200_30fps[] = {
    {PIXFORMAT_INDEX_REG,  0x00000000},
    {RESOLUTION_INDEX_REG, 0x00000001},
    {STREAM_ON,            0x00000001},
    {PIVARIETY_REG_END,    0x00000000},
};

static const pivariety_reginfo_t pivariety_MIPI_2lane_raw10_1280x720_60fps[] = {
    {PIXFORMAT_INDEX_REG,  0x00000000},
    {RESOLUTION_INDEX_REG, 0x00000002},
    {STREAM_ON,            0x00000001},
    {PIVARIETY_REG_END,    0x00000000},
};

static const pivariety_reginfo_t pivariety_MIPI_2lane_raw10_1024x600_60fps[] = {
    {PIXFORMAT_INDEX_REG,  0x00000000},
    {RESOLUTION_INDEX_REG, 0x00000003},
    {STREAM_ON,            0x00000001},
    {PIVARIETY_REG_END,    0x00000000},
};

static const pivariety_reginfo_t pivariety_MIPI_2lane_raw10_640x480_120fps[] = {
    {PIXFORMAT_INDEX_REG,  0x00000000},
    {RESOLUTION_INDEX_REG, 0x00000004},
    {STREAM_ON,            0x00000001},
    {PIVARIETY_REG_END,    0x00000000},
};







#ifdef __cplusplus
}
#endif
