/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * LT6911 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* lt6911 registers */
#define LT6911_REG_DELAY               0xfe
#define LT6911_REG_BANK                0xff

#define LT6911_REG_CHIP_ID_H           0x00
#define LT6911_REG_CHIP_ID_L           0x01
#define LT6911_REG_SYS_CTLR            0xee
#define LT6911_REG_STREAM_CTLR         0xb0

#ifdef __cplusplus
}
#endif
