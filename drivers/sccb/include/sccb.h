/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "i2c.h"
#include "i3c.h"
#include "esp_log.h"
#include "esp_err.h"

#define TEST_I3C (1)

#if TEST_I3C
#define sccb_write i3c_write

#define sccb_read i3c_read

#define sccb_write_mem i3c_write_mem

#define sccb_read_mem i3c_read_mem

#define sccb_write_mem16 i3c_write_mem16

#define sccb_read_mem16 i3c_read_mem16

#define sccb_write_reg i3c_write_reg

#define sccb_read_reg i3c_read_reg

#define sccb_write_reg16 i3c_write_reg16

#define sccb_read_reg16 i3c_read_reg16

#define sccb_prob i3c_prob

#define sccb_init i3c_init
#else
#define sccb_write i2c_write

#define sccb_read i2c_read

#define sccb_write_mem i2c_write_mem

#define sccb_read_mem i2c_read_mem

#define sccb_write_mem16 i2c_write_mem16

#define sccb_read_mem16 i2c_read_mem16

#define sccb_write_reg i2c_write_reg

#define sccb_read_reg i2c_read_reg

#define sccb_write_reg16 i2c_write_reg16

#define sccb_read_reg16 i2c_read_reg16

#define sccb_prob i2c_prob

#define sccb_init i2c_init
#endif

// Todo, remove this API when IDF master support ESP32-P4 912
void sccb_bus_init(int pin_scl, int pin_sda);

// If pin_sccb_sda is -1, use the already configured I2C bus by number
esp_err_t sccb_i2c_init(int pin_sda, int pin_scl, int port);

// If pin_sccb_sda is -1, use the already configured I3C bus by number
esp_err_t sccb_i3c_init(int pin_sda, int pin_scl, int port);

#ifdef __cplusplus
}
#endif
