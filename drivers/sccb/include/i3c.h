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

int i3c_write(uint32_t addr, size_t len, uint8_t *data);

int i3c_read(uint32_t addr, size_t len, uint8_t *data);

int i3c_write_mem(uint32_t addr, uint8_t reg, size_t len, uint8_t *data);

int i3c_read_mem(uint32_t addr, uint8_t reg, size_t len, uint8_t *data);

int i3c_write_mem16(uint32_t addr, uint16_t reg, size_t len, uint8_t *data);

int i3c_read_mem16(uint32_t addr, uint16_t reg, size_t len, uint8_t *data);

int i3c_write_reg(uint32_t addr, uint8_t reg, uint32_t len, ...);

int i3c_read_reg(uint32_t addr, uint8_t reg, uint32_t len, ...);

int i3c_write_reg16(uint32_t addr, uint16_t reg, uint32_t len, ...);

int i3c_read_reg16(uint32_t addr, uint16_t reg, uint32_t len, ...);

int i3c_prob();

void i3c_init(int rate, int io_scl, int io_sda);

#ifdef __cplusplus
}
#endif