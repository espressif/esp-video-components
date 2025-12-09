/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "ov2710_regs.h"
#include "ov2710_types.h"

#define BIT(nr)                                    (1UL << (nr))

#include "ov2710_mipi_1lane_24Minput_1280x720_raw10_25fps.h"

#include "ov2710_mipi_1lane_24Minput_1920x1080_raw10_25fps.h"

#ifdef __cplusplus
}
#endif
