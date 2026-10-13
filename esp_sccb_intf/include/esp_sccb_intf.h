/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_sccb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Perform a write transaction for 8-bit reg_addr and 8-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[in] reg_val  Data to send on the sccb bus.
 * @return
 *      - ESP_OK: sccb transmit success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_reg_a8v8(esp_sccb_io_handle_t io_handle, uint8_t reg_addr, uint8_t reg_val);

/**
 * @brief Perform a write transaction for 16-bit reg_addr and 8-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[in] reg_val  Data to send on the sccb bus.
 * @return
 *      - ESP_OK: sccb transmit success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_reg_a16v8(esp_sccb_io_handle_t io_handle, uint16_t reg_addr, uint8_t reg_val);

/**
 * @brief Perform a write transaction for 8-bit reg_addr and 16-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[in] reg_val  Data to send on the sccb bus.
 * @return
 *      - ESP_OK: sccb transmit success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_reg_a8v16(esp_sccb_io_handle_t io_handle, uint8_t reg_addr, uint16_t reg_val);

/**
 * @brief Perform a write transaction for 16-bit reg_addr and 16-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[in] reg_val  Data to send on the sccb bus.
 * @return
 *      - ESP_OK: sccb transmit success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_reg_a16v16(esp_sccb_io_handle_t io_handle, uint16_t reg_addr, uint16_t reg_val);

/**
 * @brief Perform a write transaction for 16-bit reg_addr and 32-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[in] reg_val  Data to send on the sccb bus.
 * @return
 *      - ESP_OK: sccb transmit success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_reg_a16v32(esp_sccb_io_handle_t io_handle, uint16_t reg_addr, uint32_t reg_val);

/**
 * @brief Perform a write transaction for 16-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[in] reg_val  Data to send on the sccb bus.
 * @param[in] reg_val_size size of the data to send on the sccb bus.
 * @return
 *      - ESP_OK: sccb transmit success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_reg_a16(esp_sccb_io_handle_t io_handle, uint16_t reg_addr, const void *reg_val, size_t reg_val_size);

/**
 * @brief Perform a write-read transaction for 8-bit reg_addr and 8-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[out] reg_val Data bytes received from sccb bus.
 * @return
 *      - ESP_OK: sccb transmit-receive success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_receive_reg_a8v8(esp_sccb_io_handle_t io_handle, uint8_t reg_addr, uint8_t *reg_val);

/**
 * @brief Perform a write-read transaction for 16-bit reg_addr and 8-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[out] reg_val Data bytes received from sccb bus.
 * @return
 *      - ESP_OK: sccb transmit-receive success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_receive_reg_a16v8(esp_sccb_io_handle_t io_handle, uint16_t reg_addr, uint8_t *reg_val);

/**
 * @brief Perform a write-read transaction for 8-bit reg_addr and 16-bit reg_val.
 *
 * @param[in]  handle   SCCB IO handle
 * @param[in]  reg_addr address to send on the sccb bus.
 * @param[out] reg_val  Data bytes received from sccb bus.
 * @return
 *      - ESP_OK: sccb transmit-receive success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_receive_reg_a8v16(esp_sccb_io_handle_t io_handle, uint8_t reg_addr, uint16_t *reg_val);

/**
 * @brief Perform a write-read transaction for 16-bit reg_addr and 16-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[out] reg_val Data bytes received from sccb bus.
 * @return
 *      - ESP_OK: sccb transmit-receive success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_receive_reg_a16v16(esp_sccb_io_handle_t io_handle, uint16_t reg_addr, uint16_t *reg_val);

/**
 * @brief Perform a write-read transaction for 16-bit reg_addr and 32-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[out] reg_val Data bytes received from sccb bus.
 * @return
 *      - ESP_OK: sccb transmit-receive success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_receive_reg_a16v32(esp_sccb_io_handle_t io_handle, uint16_t reg_addr, uint32_t *reg_val);

/**
 * @brief Perform a write-read transaction for 16-bit reg_addr and 16-bit reg_val.
 *
 * @param[in] handle SCCB IO handle
 * @param[in] reg_addr address to send on the sccb bus.
 * @param[out] reg_val Data bytes received from sccb bus.
 * @param[in] reg_val_size size of the data to receive from the sccb bus.
 * @return
 *      - ESP_OK: sccb transmit-receive success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_receive_reg_a16(esp_sccb_io_handle_t io_handle, uint16_t reg_addr, void *reg_val, size_t reg_val_size);

/**
 * @brief Perform a write transaction for 16-bit val.
 *
 * @param[in]  handle   SCCB IO handle
 * @param[in] val  Data to send on the sccb bus.
 * @return
 *      - ESP_OK: sccb transmit success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_transmit_v16(esp_sccb_io_handle_t io_handle, uint16_t val);

/**
 * @brief Perform a read transaction for 16-bit val.
 *
 * @param[in] handle SCCB IO handle
 * @param[out] val Data bytes received from sccb bus.
 * @return
 *      - ESP_OK: sccb receive success
 *      - ESP_ERR_INVALID_ARG: sccb transmit parameter invalid.
 */
esp_err_t esp_sccb_receive_v16(esp_sccb_io_handle_t io_handle, uint16_t *val);

/**
 * @brief Delete sccb I2C IO handle
 *
 * @param[in] handle SCCB IO handle
 * @return
 *        - ESP_OK: If controller is successfully deleted.
 */
esp_err_t esp_sccb_del_i2c_io(esp_sccb_io_handle_t io_handle);

/**
 * @brief Delete an SCCB IO handle created by sccb_new_i3c_io()
 *
 * Thread-safety contract (external serialization with IO):
 * - Concurrent transmit/receive on the same handle are serialized internally.
 * - This delete must not run concurrently with any IO on `io_handle`.
 * - Every other thread must have returned from transmit/receive before delete
 *   is called, and no thread may start IO on `io_handle` until delete returns.
 * - Do not use `io_handle` after this function returns.
 *
 * Object-local reference counting cannot close the window where one thread has
 * entered an IO call but not yet taken the internal lock while another thread
 * frees the object; callers must uphold the contract above.
 *
 * @param[in] io_handle SCCB IO handle
 * @return
 *        - ESP_OK: If controller is successfully deleted.
 *        - ESP_ERR_INVALID_ARG: io_handle is NULL.
 *        - ESP_ERR_NOT_SUPPORTED: delete function is not implemented.
 */
esp_err_t esp_sccb_del_i3c_io(esp_sccb_io_handle_t io_handle);

#ifdef __cplusplus
}
#endif
