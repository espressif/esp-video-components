/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "sc1346_regs.h"
#include "sc1346_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_LENGTH_30FPS        900
#define FRAME_LENGTH_25FPS       1080

#include "sc1346_mipi_1lane_24Minput_720p_raw10_25fps.h"
#include "sc1346_mipi_1lane_24Minput_720p_raw10_30fps.h"
#include "sc1346_mipi_1lane_24Minput_720p_raw8_30fps.h"

#ifdef __cplusplus
}
#endif
