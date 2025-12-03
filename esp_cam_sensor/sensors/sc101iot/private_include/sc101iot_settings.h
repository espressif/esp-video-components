/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "sc101iot_regs.h"
#include "sc101iot_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SC101IOT_YUV422_FMT_UYVY 0x02
#define SC101IOT_YUV422_FMT_YUYV 0x00

#include "sc101iot_dvp_8bit_20Minput_1280x720_yuv422_uyvy_15fps.h"
#include "sc101iot_dvp_8bit_20Minput_1280x720_yuv422_yuyv_15fps.h"

#include "sc101iot_dvp_8bit_20Minput_1280x720_yuv422_uyvy_25fps.h"
#include "sc101iot_dvp_8bit_20Minput_1280x720_yuv422_yuyv_25fps.h"

#ifdef __cplusplus
}
#endif
