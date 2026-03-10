/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "sp0a39_regs.h"
#include "sp0a39_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
#if CONFIG_CAMERA_SP0A39_DVP_GRAY_640X480_30FPS
#include "sp0a39_dvp_8bit_24Minput_gray_640x480_30fps.h"
#endif
#if CONFIG_CAMERA_SP0A39_DVP_GRAY_200X200_30FPS
#include "sp0a39_dvp_8bit_24Minput_gray_200x200_30fps.h"
#endif
#endif

#if CONFIG_CAMERA_SP0A39_SPI_2BIT_GRAY_640X480_15FPS
#include "sp0a39_spi_2bit_24Minput_gray_640x480_15fps.h"
#endif
#if CONFIG_CAMERA_SP0A39_SPI_1BIT_GRAY_640X480_4FPS
#include "sp0a39_spi_1bit_24Minput_gray_640x480_4fps.h"
#endif
#if CONFIG_CAMERA_SP0A39_SPI_4BIT_YUV422_640X480_15FPS
#include "sp0a39_spi_4bit_24Minput_yuv422_640x480_15fps.h"
#endif

#ifdef __cplusplus
}
#endif
