/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/i3c_master.h"
#include "esp_sccb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SCCB I3C configuration
 *
 * @note The device is attached as a legacy I2C device on the I3C bus.
 *       Only a 7-bit address is supported, and the slave must not stretch the clock.
 */
typedef struct {
    uint8_t device_address;     ///< 7-bit SCCB address, without the read/write bit
    uint32_t scl_speed_hz;      ///< SCL frequency for this device
} sccb_i3c_config_t;

/**
 * @brief Create an SCCB IO handle on an existing I3C master bus
 *
 * The bus itself is created by the caller with `i3c_new_master_bus()`.
 * This function only attaches one legacy I2C device and fills the SCCB IO vtable.
 *
 * @note Only a synchronous I3C master bus is supported. Create the bus without
 *       `flags.enable_async_trans`. An asynchronous bus is rejected.
 *
 * @note Delete the handle with `esp_sccb_del_i3c_io()`. That call must be
 *       externally serialized with all transmit/receive on the same handle;
 *       see the contract on `esp_sccb_del_i3c_io()`.
 *
 * @param[in]  bus_handle  I3C master bus handle
 * @param[in]  config      SCCB I3C device configuration
 * @param[out] io_handle   SCCB IO handle
 *
 * @return
 *        - ESP_OK: On success
 *        - ESP_ERR_INVALID_ARG: Invalid argument
 *        - ESP_ERR_NO_MEM: Out of memory
 *        - ESP_ERR_NOT_SUPPORTED: The I3C bus is asynchronous
 */
esp_err_t sccb_new_i3c_io(i3c_master_bus_handle_t bus_handle, const sccb_i3c_config_t *config, esp_sccb_io_handle_t *io_handle);

#ifdef __cplusplus
}
#endif
