/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "hal/mipi_csi_ll.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIPI_CSI_HAL_LAYER_VERSION        0x1

/**
 * @brief MIPI CSI HAL driver context
 */
typedef struct {
    csi_host_dev_t *host_dev;
    csi_brg_dev_t *bridge_dev;
    uint32_t version;
} mipi_csi_hal_context_t;

/**
 * @brief MIPI CSI HAL driver configuration
 */
typedef struct {
    uint8_t lanes_num;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t in_bits_per_pixel;
    int mipi_clk;
    uint32_t out_bits_per_pixel;
    size_t vc_channel_num;
    uint32_t dma_req_interval;
} mipi_csi_hal_config_t;

/**
 * @brief MIPI CSI HAL layer initialization
 *
 * @note Caller should malloc the memory for the hal context
 *
 * @param hal Pointer to the HAL driver context
 * @param config Pointer to the HAL driver configuration
 */
void mipi_csi_hal_init(mipi_csi_hal_context_t *hal, const mipi_csi_hal_config_t *config);

#ifdef __cplusplus
}
#endif
