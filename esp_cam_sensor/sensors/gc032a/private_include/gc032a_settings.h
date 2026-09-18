/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "gc032a_regs.h"
#include "gc032a_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_CAMERA_GC032A_DVP_YUV422_YUYV_640X480_7FPS
#include "gc032a_dvp_8bit_20Minput_640x480_yuv422_7fps.h"
#endif
#if CONFIG_CAMERA_GC032A_DVP_RGB565_640X480_30FPS
#include "gc032a_dvp_8bit_24Minput_640x480_rgb565_30fps.h"
#endif

#ifdef __cplusplus
}
#endif
