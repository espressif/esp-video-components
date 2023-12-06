/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_SCCB_BASED_I3C_ENABLED
#include "sccb_i2c.h"
#define sccb_init sccb_i2c_init

#define sccb_deinit sccb_i2c_deinit

#define sccb_read_reg8_val8 i2c_read_reg8_val8

#define sccb_write_reg8_val8 i2c_write_reg8_val8

#define sccb_read_reg16_val8 i2c_read_reg16_val8

#define sccb_write_reg16_val8 i2c_write_reg16_val8

#define sccb_read_reg16_val16 i2c_read_reg16_val16

#define sccb_write_reg16_val16 i2c_write_reg16_val16

#define sccb_read_reg8_val16 i2c_read_reg8_val16

#define sccb_write_reg8_val16 i2c_write_reg8_val16

#else
#include "sccb_i3c.h"
#define sccb_init sccb_i3c_init

#define sccb_deinit sccb_i3c_deinit

#define sccb_read_reg8_val8 i3c_read_reg8_val8

#define sccb_write_reg8_val8 i3c_write_reg8_val8

#define sccb_read_reg16_val8 i3c_read_reg16_val8

#define sccb_write_reg16_val8 i3c_write_reg16_val8

#define sccb_read_reg16_val16 i3c_read_reg16_val16

#define sccb_write_reg16_val16 i3c_write_reg16_val16

#define sccb_read_reg8_val16 i3c_read_reg8_val16

#define sccb_write_reg8_val16 i3c_write_reg8_val16
#endif

#ifdef __cplusplus
}
#endif
