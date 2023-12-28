/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "hal/misc.h"
#include "hal/assert.h"
#include "soc/mipi_csi_bridge_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CSI data endianness order in bytes.
 */
typedef enum {
    CSI_BRG_DATA_BYTE_ENDIAN_NORMAL = 0,
    CSI_BRG_DATA_BYTE_FOR_LEGACY,
} csi_brg_data_byte_endian_t;

/**
 * @brief CSI data endianness order in bits.
 */
typedef enum {
    CSI_BRG_DATA_BIT_ENDIAN_BIG = 0,
    CSI_BRG_DATA_BIT_ENDIAN_LITTLE,
} csi_brg_data_bit_endian_t;

// Todo extern
extern csi_brg_dev_t MIPI_CSI_BRIDGE;

// CSI Bridge
#define MIPI_CSI_BRIDGE_LL_GET_HW(num) (((num) == 0) ? (&MIPI_CSI_BRIDGE) : NULL)

/**
 * @brief Set the width and height of the received frame for the MIPI CSI bridge
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param frame_width Active width
 * @param frame_height Active height
 */
static inline void mipi_csi_brg_ll_set_frame_size(csi_brg_dev_t *dev, size_t frame_width, size_t frame_height)
{
    dev->frame_cfg.hadr_num = frame_width;
    dev->frame_cfg.vadr_num = frame_height;
}

/**
 * @brief Set the buffer almost full threshold for the MIPI CSI bridge
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param afull_thrd full threshold
 */
static inline void mipi_csi_brg_ll_set_flow_ctl_buf_afull_thrd(csi_brg_dev_t *dev, size_t afull_thrd)
{
    dev->buf_flow_ctl.csi_buf_afull_thrd = afull_thrd;
}

/**
 * @brief Set the DMA burst length for the MIPI CSI bridge
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param len Number of 64-bit words in one dma burst transfer
 */
static inline void mipi_csi_brg_ll_set_req_dma_burst_len(csi_brg_dev_t *dev, size_t len)
{
    dev->dma_req_cfg.dma_burst_len = len;
}

/**
 * @brief Set the frame data whether contain hsync
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param en 0: frame data doesn't contain hsync. 1: frame data contains hsync.
 */
static inline void mipi_csi_brg_ll_enable_has_hsync(csi_brg_dev_t *dev, bool en)
{
    dev->frame_cfg.has_hsync_e = en;
}

/**
 * @brief Set the min value of data type used for pixel filter.
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param type_min The min data type allowed by bridge's pixel filter.
 * The data type specifie the format and the content of the payload data.
 */
static inline void mipi_csi_brg_ll_set_data_type_min(csi_brg_dev_t *dev, uint16_t type_min)
{
    dev->data_type_cfg.data_type_min = type_min;
}

/**
 * @brief Set the the max value of data type used for pixel filter.
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param type_min The max data type allowed by bridge's pixel filter.
 * The data type specifie the format and the content of the payload data.
 */
static inline void mipi_csi_brg_ll_set_data_type_max(csi_brg_dev_t *dev, uint16_t type_max)
{
    dev->data_type_cfg.data_type_max = type_max;
}

/**
 * @brief Set the DMA interval configuration.
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param interval 16'b1: 1 cycle. 16'b11: 2 cycle. ... ... 16'hFFFF: 16 cycle.
 */
static inline void mipi_csi_brg_ll_set_dma_req_interval(csi_brg_dev_t *dev, uint16_t interval)
{
    HAL_FORCE_MODIFY_U32_REG_FIELD(dev->dma_req_interval, dma_req_interval, interval);
}

/**
 * @brief Set the data endianness order in bytes
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param mode csi bridge byte endian mode
 */
static inline void mipi_csi_brg_ll_set_byte_endian(csi_brg_dev_t *dev, csi_brg_data_byte_endian_t mode)
{
    switch (mode) {
    case CSI_BRG_DATA_BYTE_ENDIAN_NORMAL:
        dev->endian_mode.byte_endian_order = 0;
        break;
    case CSI_BRG_DATA_BYTE_FOR_LEGACY:
        dev->endian_mode.byte_endian_order = 1;
        break;
    default:
        abort();
    }
}

/**
 * @brief Set the data endianness order in bits
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param mode csi bridge bit endian mode
 */
static inline void mipi_csi_brg_ll_set_bit_endian(csi_brg_dev_t *dev, csi_brg_data_bit_endian_t mode)
{
    switch (mode) {
    case CSI_BRG_DATA_BIT_ENDIAN_BIG:
        dev->endian_mode.bit_endian_order = 0;
        break;
    case CSI_BRG_DATA_BIT_ENDIAN_LITTLE:
        dev->endian_mode.bit_endian_order = 1;
        break;
    default:
        abort();
    }
}

/**
 * @brief Enable the CSI bridge
 *
 * @param dev Pointer to the CSI bridge controller register base address
 * @param en True to enable, false to disable
 */
static inline void mipi_csi_brg_ll_enable(csi_brg_dev_t *dev, bool en)
{
    dev->csi_en.val = en;
}

#ifdef __cplusplus
}
#endif
