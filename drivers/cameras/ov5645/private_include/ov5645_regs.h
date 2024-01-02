/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define OV5645_REG_END      0xffff
#define OV5645_REG_DELAY    0xfffe

/* OV5645 registers */
#define OV5645_REG_SENSOR_ID_H             0x300a
#define OV5645_REG_SENSOR_ID_L             0x300b
#define OV5645_MIPI_CONTROL00              0x300e

/* output format control registers */
#define FORMAT_CTRL     0x501f // Format select
// Bit[2:0]:
//  000: YUV422
//  001: RGB
//  010: Dither
//  011: RAW after DPC
//  101: RAW after CIP
