/*
 * SPDX-FileCopyrightText: 2026 Arducam Electronic Technology (Nanjing) CO LTD
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

static const pivariety_reginfo_t pivariety_mipi_PIVARIETY_REG_STREAM_ON[] = {
    {PIVARIETY_REG_STREAM_ON,       0x00000001},
    {PIVARIETY_REG_END,             0x00000000},
};

static const pivariety_reginfo_t pivariety_mipi_stream_off[] = {
    {PIVARIETY_REG_STREAM_ON,       0x00000000},
    {PIVARIETY_REG_END,             0x00000000},
};
#ifdef __cplusplus
}
#endif
