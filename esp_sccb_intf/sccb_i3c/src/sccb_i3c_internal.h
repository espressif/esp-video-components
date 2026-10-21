/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i3c_master_i2c.h"
#include "esp_sccb_io_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sccb_io_i3c_t sccb_io_i3c_t;

/**
 * @brief SCCB controller backed by a synchronous I3C legacy I2C device
 *
 * `lock` only serializes concurrent transmit/receive on this handle.
 * Destroy must not race with IO; see esp_sccb_del_i3c_io().
 */
struct sccb_io_i3c_t {
    i3c_master_i2c_device_handle_t i3c_device;
    struct esp_sccb_io_t base;
    uint8_t *tx_buf;
    size_t tx_buf_cap;
    uint8_t *rx_buf;
    size_t rx_buf_cap;
    SemaphoreHandle_t lock;
};

#ifdef __cplusplus
}
#endif
