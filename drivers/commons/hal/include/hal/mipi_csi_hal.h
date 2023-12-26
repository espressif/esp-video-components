/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// The HAL layer for MIPI-CSI

#pragma once

#include "hal/mipi_csi_ll.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIPI_CSI_HAL_LAYER_VERSION        0x1

/**
 * @brief Context of the HAL
 */
typedef struct {
    csi_host_dev_t *host_dev;
    csi_brg_dev_t *bridge_dev;
    uint32_t version;

    /**< these need to be configured by `mipi_csi_hal_config_t` via driver layer*/
    uint8_t lanes_num;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t in_bits_per_pixel;
    int mipi_clk;
    uint32_t out_bits_per_pixel;
    size_t vc_channel_num;
    uint32_t dma_req_interval;
} mipi_csi_hal_context_t;

/**
 * @brief  Init the MIPI-CSI Host-Controller and D-PHY.
 *
 * @param  hal Context of the HAL layer
 *
 * @return None
 */
void mipi_csi_hal_host_phy_initialization(mipi_csi_hal_context_t *hal);

/**
 * @brief  Init the MIPI-CSI Bridge.
 *
 * @param  hal Context of the HAL layer
 *
 * @return None
 */
void mipi_csi_hal_bridge_initialization(mipi_csi_hal_context_t *hal);

#ifdef __cplusplus
}
#endif
