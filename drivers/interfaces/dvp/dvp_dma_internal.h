/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "hal/dma_types.h"
#include "esp_private/gdma.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gdma_channel_handle_t   gdma_chan;
} dvp_dma_t;

/**
 * @brief Create DVP DMA object
 *
 * @param[out] dvp_dma Returned DVP DMA object pointer
 *
 * @return ESP_OK on success, otherwise see esp_err_t
 */
esp_err_t dvp_dma_init(dvp_dma_t *dvp_dma);

/**
 * @brief Delete DVP DMA object
 *
 * @param[in] dvp_dma DVP DMA object pointer
 *
 * @return ESP_OK on success, otherwise see esp_err_t
 */
esp_err_t dvp_dma_deinit(dvp_dma_t *dvp_dma);

/**
 * @brief Set DVP DMA descriptor address and start engine
 *
 * @param[in] dvp_dma DVP DMA object pointer
 *
 * @return ESP_OK on success, otherwise see esp_err_t
 */
esp_err_t dvp_dma_start(dvp_dma_t *dvp_dma, dma_descriptor_t *addr);

/**
 * @brief Stop DVP DMA engine
 *
 * @param[in] dvp_dma DVP DMA object pointer
 *
 * @return ESP_OK on success, otherwise see esp_err_t
 */
esp_err_t dvp_dma_stop(dvp_dma_t *dvp_dma);

/**
 * @brief Reset DVP DMA FIFO and internal finite state machine
 *
 * @param[in] dvp_dma DVP DMA object pointer
 *
 * @return ESP_OK on success, otherwise see esp_err_t
 */
esp_err_t dvp_dma_reset(dvp_dma_t *dvp_dma);

#ifdef __cplusplus
}
#endif
