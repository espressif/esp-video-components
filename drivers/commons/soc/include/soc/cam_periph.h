/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "soc/periph_defs.h"
#include "soc/soc_caps_extra.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const periph_module_t module;
    const int irq_id;
    const int data_sigs[SOC_CAM_DATA_WIDTH];
    const int hsync_sig;
    const int vsync_sig;
    const int pclk_sig;
    const int href_sig;
} cam_signal_conn_t;

extern const cam_signal_conn_t cam_periph_signals[SOC_CAM_PERIPH_NUM];

#ifdef __cplusplus
}
#endif
