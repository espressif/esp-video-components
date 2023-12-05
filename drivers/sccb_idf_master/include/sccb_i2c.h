/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Init an I2C driver used for SCCB bus
 *
 * @param port I2C port number
 * @param pin_sda GPIO number for I2C sda signal
 * @param pin_scl GPIO number for I2C scl signal
 *
 * @return
 *     - ESP_OK   Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Driver installation error
 */
esp_err_t sccb_i2c_init(int port, int pin_sda, int pin_scl);

/**
 * @brief Delete I2C driver used by SCCB bus
 *
 * @note This function does not guarantee thread safety.
 *       Please make sure that no thread will continuously hold semaphores before calling the delete function.
 *
 * @param port I2C port to delete
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t sccb_i2c_deinit(int port);

/**
 * @brief Perform a read to a device connected to a particular I2C port.
 *        The size of the register's address is 8bit, and the size of the register's value is 8bit.
 *
 * @param port I2C port number to perform the transfer on
 * @param slv_addr I2C device's 7-bit address
 * @param reg Register address of the slave to be accessed
 *
 * @return
 *     - Value of the register
 *     - -1 Parameter error
 */
uint8_t i2c_read_reg8_val8(int port, uint8_t slv_addr, uint8_t reg);

/**
 * @brief Perform a write to a device connected to a particular I2C port.
 *        The size of the register's address is 8bit, and the size of the register's value is 8bit.
 *
 * @param port I2C port number to perform the transfer on
 * @param slv_addr I2C device's 7-bit address
 * @param reg Register address of the slave to be accessed
 * @param data Data to send on the port
 *
 * @return
 *     - 0 Success
 *     - -1 Parameter error
 */
int i2c_write_reg8_val8(int port, uint8_t slv_addr, uint8_t reg, uint8_t data);

/**
 * @brief Perform a read to a device connected to a particular I2C port.
 *        The size of the register's address is 16bit, and the size of the register's value is 8bit.
 *
 * @param port I2C port number to perform the transfer on
 * @param slv_addr I2C device's 7-bit address
 * @param reg Register address of the slave to be accessed
 *
 * @return
 *     - Value of the register
 *     - -1 Parameter error
 */
uint8_t i2c_read_reg16_val8(int port, uint8_t slv_addr, uint16_t reg);

/**
 * @brief Perform a read to a device connected to a particular I2C port.
 *        The size of the register's address is 16bit, and the size of the register's value is 8bit.
 *
 * @param port I2C port number to perform the transfer on
 * @param slv_addr I2C device's 7-bit address
 * @param reg Register address of the slave to be accessed
 * @param data Data to send on the port
 *
 * @return
 *     - 0 Success
 *     - -1 Parameter error
 */
int i2c_write_reg16_val8(int port, uint8_t slv_addr, uint16_t reg, uint8_t data);

/**
 * @brief Perform a read to a device connected to a particular I2C port.
 *        The size of the register's address is 16bit, and the size of the register's value is 16bit.
 *
 * @param port I2C port number to perform the transfer on
 * @param slv_addr I2C device's 7-bit address
 * @param reg Register address of the slave to be accessed
 *
 * @return
 *     - Value of the register
 *     - -1 Parameter error
 */
uint16_t i2c_read_reg16_val16(int port, uint8_t slv_addr, uint16_t reg);

/**
 * @brief Perform a read to a device connected to a particular I2C port.
 *        The size of the register's address is 16bit, and the size of the register's value is 16bit.
 *
 * @param port I2C port number to perform the transfer on
 * @param slv_addr I2C device's 7-bit address
 * @param reg Register address of the slave to be accessed
 * @param data Data to send on the port
 *
 * @return
 *     - 0 Success
 *     - -1 Parameter error
 */
int i2c_write_reg16_val16(int port, uint8_t slv_addr, uint16_t reg, uint16_t data);

/**
 * @brief Perform a read to a device connected to a particular I2C port.
 *        The size of the register's address is 8bit, and the size of the register's value is 16bit.
 *
 * @param port I2C port number to perform the transfer on
 * @param slv_addr I2C device's 7-bit address
 * @param reg Register address of the slave to be accessed
 *
 * @return
 *     - Value of the register
 *     - -1 Parameter error
 */
uint16_t i2c_read_reg8_val16(int port, uint8_t slv_addr, uint8_t reg);

/**
 * @brief Perform a write to a device connected to a particular I2C port.
 *        The size of the register's address is 8bit, and the size of the register's value is 16bit.
 *
 * @param port I2C port number to perform the transfer on
 * @param slv_addr I2C device's 7-bit address
 * @param reg Register address of the slave to be accessed
 * @param data Data to send on the port
 *
 * @return
 *     - 0 Success
 *     - -1 Parameter error
 */
int i2c_write_reg8_val16(int port, uint8_t slv_addr, uint8_t reg, uint16_t data);

#ifdef __cplusplus
}
#endif
