/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "rom/lldesc.h"
#ifdef CONFIG_IDF_TARGET_ESP32
#include "hal/i2s_ll.h"
#endif

#ifdef CONFIG_IDF_TARGET_ESP32
typedef i2s_dev_t cam_dev_t;
#define CAM_RX_INT_MASK I2S_LL_RX_EVENT_MASK
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CAM hardware interface object data
 */
typedef struct cam_hal_context {
    cam_dev_t *hw;                          /*!< Beginning address of the CAM peripheral registers. */
} cam_hal_context_t;

/**
 * @brief Initialize CAM hardware
 *
 * @param hal    CAM object data pointer
 * @param port   CAM port
 *
 * @return None
 */
void cam_hal_init(cam_hal_context_t *hal, uint8_t port);

/**
 * @brief De-initialize CAM hardware
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_deinit(cam_hal_context_t *hal);

/**
 * @brief Start CAM to receive frame data, and active driver to send "CAM_EVENT_DATA_RECVED" event
 *
 * @param hal CAM object data pointer
 * @param lldesc CAM DMA description pointer
 * @param size   CAM DMA receive size to trigger interrupt
 *
 * @return None
 */
void cam_hal_start_streaming(cam_hal_context_t *hal, lldesc_t *lldesc, size_t size);

/**
 * @brief Disable CAM receiving frame data, and deactive driver sending "CAM_EVENT_DATA_RECVED" event
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_stop_streaming(cam_hal_context_t *hal);

/**
 * @brief CAM copy data from source memory to destination memory, this can't be called in interrupt.
 *
 * @param hal  CAM object data pointer
 * @param dst  Destination memory pointer
 * @param src  Source memory pointer
 * @param size Data size in byte
 *
 * @return CAM sample data size
 */
void cam_hal_memcpy(cam_hal_context_t *hal, uint8_t *dst, const uint8_t *src, size_t size);

/**
 * @brief Get CAM sample data size, ESP32 has special sample data size with different receive data format
 *
 * @param hal CAM object data pointer
 *
 * @return CAM sample data size
 */
size_t cam_hal_get_sample_data_size(cam_hal_context_t *hal);

/**
 * @brief Get CAM interrupt status
 *
 * @param hal CAM object data pointer
 *
 * @return CAM interrupt status
 */
uint32_t cam_hal_get_int_status(cam_hal_context_t *hal);

/**
 * @brief Clear CAM interrupt status
 *
 * @param hal CAM object data pointer
 *
 * @return None
 */
void cam_hal_clear_int_status(cam_hal_context_t *hal, uint32_t status);

/**
 * @brief Get DMA buffer align size
 *
 * @param hal CAM object data pointer
 *
 * @return DMA buffer align size
 */
uint32_t cam_hal_dma_align_size(cam_hal_context_t *hal);

#ifdef __cplusplus
}
#endif
