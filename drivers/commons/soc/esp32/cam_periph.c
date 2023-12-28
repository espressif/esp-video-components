/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/cam_periph.h"

const cam_signal_conn_t cam_periph_signals[SOC_CAM_PERIPH_NUM] = {
    {
        .module = PERIPH_I2S0_MODULE,
        .irq_id = ETS_I2S0_INTR_SOURCE,
        .data_sigs = {
            I2S0I_DATA_IN0_IDX,  I2S0I_DATA_IN1_IDX,  I2S0I_DATA_IN2_IDX,  I2S0I_DATA_IN3_IDX,
            I2S0I_DATA_IN4_IDX,  I2S0I_DATA_IN5_IDX,  I2S0I_DATA_IN6_IDX,  I2S0I_DATA_IN7_IDX,
            I2S0I_DATA_IN8_IDX,  I2S0I_DATA_IN9_IDX,  I2S0I_DATA_IN10_IDX, I2S0I_DATA_IN11_IDX,
            I2S0I_DATA_IN12_IDX, I2S0I_DATA_IN13_IDX, I2S0I_DATA_IN14_IDX, I2S0I_DATA_IN15_IDX,
        },
        .hsync_sig = I2S0I_H_SYNC_IDX,
        .vsync_sig = I2S0I_V_SYNC_IDX,
        .pclk_sig = I2S0I_WS_IN_IDX,
        .href_sig = I2S0I_H_ENABLE_IDX,
    },
    {
        .module = PERIPH_I2S1_MODULE,
        .irq_id = ETS_I2S1_INTR_SOURCE,
        .data_sigs = {
            I2S1I_DATA_IN0_IDX,  I2S1I_DATA_IN1_IDX,  I2S1I_DATA_IN2_IDX,  I2S1I_DATA_IN3_IDX,
            I2S1I_DATA_IN4_IDX,  I2S1I_DATA_IN5_IDX,  I2S1I_DATA_IN6_IDX,  I2S1I_DATA_IN7_IDX,
            I2S1I_DATA_IN8_IDX,  I2S1I_DATA_IN9_IDX,  I2S1I_DATA_IN10_IDX, I2S1I_DATA_IN11_IDX,
            I2S1I_DATA_IN12_IDX, I2S1I_DATA_IN13_IDX, I2S1I_DATA_IN14_IDX, I2S1I_DATA_IN15_IDX,
        },
        .hsync_sig = I2S1I_H_SYNC_IDX,
        .vsync_sig = I2S1I_V_SYNC_IDX,
        .pclk_sig = I2S1I_WS_IN_IDX,
        .href_sig = I2S1I_H_ENABLE_IDX,
    }
};
