/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "bf3901_regs.h"
#include "bf3901_types.h"

#define G3LB5  0x05
#define RGB    0x04

#define YUV422_UYVY 0x02
#define YUV422_YUYV 0x00

#ifdef __cplusplus
extern "C" {
#endif

#include "bf3901_spi_1bit_20Minput_120x160_yuv422_uyvy_5fps.h"
#include "bf3901_spi_1bit_20Minput_120x160_yuv422_yuyv_5fps.h"

#include "bf3901_spi_1bit_20Minput_240x320_yuv422_uyvy_12fps.h"
#include "bf3901_spi_1bit_20Minput_240x320_yuv422_yuyv_12fps.h"

#include "bf3901_spi_1bit_24Minput_120x160_yuv422_uyvy_10fps.h"
#include "bf3901_spi_1bit_24Minput_120x160_yuv422_yuyv_10fps.h"

#include "bf3901_spi_1bit_24Minput_240x240_yuv422_uyvy_10fps.h"
#include "bf3901_spi_1bit_24Minput_240x240_yuv422_yuyv_10fps.h"

#include "bf3901_spi_1bit_24Minput_240x320_rgb565_le_15fps.h"

#include "bf3901_spi_1bit_24Minput_240x320_yuv422_uyvy_15fps.h"
#include "bf3901_spi_1bit_24Minput_240x320_yuv422_yuyv_15fps.h"

#include "bf3901_spi_2bit_24Minput_240x320_yuv422_uyvy_20fps.h"
#include "bf3901_spi_2bit_24Minput_240x320_yuv422_yuyv_20fps.h"

#include "bf3901_spi_2bit_24Minput_240x320_yuv422_uyvy_18fps.h"
#include "bf3901_spi_2bit_24Minput_240x320_yuv422_yuyv_18fps.h"
#ifdef __cplusplus
}
#endif
