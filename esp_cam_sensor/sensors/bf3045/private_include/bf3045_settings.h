/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "bf3045_regs.h"
#include "bf3045_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BF3045_AEC_TARGET_MAX        0x6f
#define BF3045_AEC_TARGET_MIN        0x28
#define BF3045_AEC_TARGET_DEFAULT    0x50

#include "bf3045_dvp_8bit_20Minput_640x480_rgb565_be_15fps.h"

#ifdef __cplusplus
}
#endif
