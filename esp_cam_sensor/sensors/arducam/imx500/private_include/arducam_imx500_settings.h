/*
 * SPDX-FileCopyrightText: 2026 Arducam Electronic Technology (Nanjing) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "arducam_imx500_regs.h"
#include "arducam_imx500_types.h"

#ifdef __cplusplus
extern "C" {
#endif

static const imx500_reginfo_t imx500_mipi_IMX500_REG_STREAM_ON[] = {
    {IMX500_REG_STREAM_ON,       0x00000001},
    {IMX500_REG_END,             0x00000000},
};

static const imx500_reginfo_t imx500_mipi_stream_off[] = {
    {IMX500_REG_STREAM_ON,       0x00000000},
    {IMX500_REG_END,             0x00000000},
};
#ifdef __cplusplus
}
#endif
