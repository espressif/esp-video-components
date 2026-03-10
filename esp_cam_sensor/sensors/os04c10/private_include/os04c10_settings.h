/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "os04c10_regs.h"
#include "os04c10_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SOC_MIPI_CSI_SUPPORTED

#include "os04c10_mipi_1lane_24Minput_810x1080_raw10_30fps.h"

#include "os04c10_mipi_1lane_24Minput_960x1280_raw10_30fps.h"

#endif

#ifdef __cplusplus
}
#endif
