/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * BF3045 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* bf3045 registers */
#define BF3045_REG_DELAY               0xff
#define BF3045_REG_DLY                 0x01
#define BF3045_REGLIST_TAIL            0xff

#define BF3045_REG_SOFTWARE_STANDBY    0x09
#define BF3045_REG_COM7                0x12
#define BF3045_REG_CHIP_ID_H           0xfc
#define BF3045_REG_CHIP_ID_L           0xfd
#define BF3045_REG_TEST_MODE           0xb9
#define BF3045_REG_AE_TAR1             0x24
#define BF3045_REG_AE_TAR2             0x97
#define BF3045_REG_BRIGHT              0x55
#define BF3045_REG_Y_CONTRAST          0x4d
#define BF3045_REG_AE_MODE             0x80
#define BF3045_REG_TSLB                0x3a
#define BF3045_REG_AE_LOCK2            0x25
#define BF3045_REG_HSTART              0x17
#define BF3045_REG_HSTOP               0x18
#define BF3045_REG_VSTART              0x19
#define BF3045_REG_VSTOP               0x1a
#define BF3045_REG_CLKRC               0x11
#define BF3045_REG_MVFP                0x1e

#ifdef __cplusplus
}
#endif
