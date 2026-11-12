/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************
Copyright (C), 2026, MetaSilicon Tech. Co., Ltd.
All rights reserved.
****************************************************/

#pragma once

/* Register address is 16-bit and register data is 16-bit. */
#define MAT130A_REG_DELAY              0xfffe
#define MAT130A_REG_CHIP_ID_RPT        0x0000
#define MAT130A_CHIP_ID_RPT_VALUE      0x01300200UL
#define MAT130A_REG_MIRROR             0x08e4
#define MAT130A_REG_TEST_PATTERN       0x0950
#define MAT130A_REG_STREAM             0x0954
#define MAT130A_REG_FW_VERSION         0x0958
#define MAT130A_REG_SOFTWARE_RESET     0x0960
#define MAT130A_REG_I2C_ADDR_CFG       0x0964

#define MAT130A_DVP_STREAM_OFF         0x0000
#define MAT130A_DVP_STREAM_ON          0x0001
