/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * MT9D111 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* mt9d111 registers */
#define MT9D111_REG_DELAY                    0xfe
#define MT9D111_REG_TEST_MODE                0x48
#define MT9D111_REG_PAGE0_SOFTWARE_STANDBY   0xfd // R13:0[2]=1 to soft standby
#define MT9D111_REG_SOFTWARE_STANDBY         0x12
#define MT9D111_REG_WRITE_PAGE               0xf0
#define MT9D111_REG_CLOCK_REG                0x65
#define MT9D111_REG_UC_BOOT_MODE_REG         0xc3
#define MT9D111_REG_RESET_REG                0x0d

/* mt9d111 register vals */
#define MT9D111_PAGE0                        0x0000 // Sensor and PLL control
#define MT9D111_PAGE1                        0x0001 // SOC1, image processing and MUC control
#define MT9D111_PAGE2                        0x0002 // SOC2, JPEG control

#ifdef __cplusplus
}
#endif
