/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************
Copyright (C), 2026, MetaSilicon Tech. Co., Ltd.
All rights reserved.
****************************************************/

#pragma once

#include <stdint.h>
#include <sdkconfig.h>
#include "mit245_regs.h"
#include "mit245_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
#if CONFIG_CAMERA_MIT245_MIPI_RAW8_1920X1080_30FPS
#include "mit245_mipi_2lane_24Minput_1920x1080_raw8_30fps.h"
#endif
#if CONFIG_CAMERA_MIT245_MIPI_RAW10_1920X1080_30FPS
#include "mit245_mipi_2lane_24Minput_1920x1080_raw10_30fps.h"
#endif
#endif

#ifdef __cplusplus
}
#endif
