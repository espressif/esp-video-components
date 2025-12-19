/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "sc030iot_regs.h"
#include "sc030iot_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED

#define SC030IOT_YUV422_FMT_UYVY 0xa8
#define SC030IOT_YUV422_FMT_YUYV 0x88

#include "sc030iot_dvp_8bit_20Minput_640x480_raw8_26fps.h"

#include "sc030iot_dvp_8bit_20Minput_640x480_yuv422_uyvy_26fps.h"
#include "sc030iot_dvp_8bit_20Minput_640x480_yuv422_yuyv_26fps.h"

#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED

#include "sc030iot_mipi_1lane_24Minput_640x480_raw8_60fps.h"

#include "sc030iot_mipi_1lane_24Minput_640x480_yuv422_uyvy_25fps.h"

#include "sc030iot_mipi_1lane_24Minput_640x480_yuv422_uyvy_50fps.h"

#endif

#ifdef __cplusplus
}
#endif
