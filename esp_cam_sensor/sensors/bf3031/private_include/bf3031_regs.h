/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * BF3031 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* bf3031 registers */
#define BF3031_REG_DELAY               0xff
#define BF3031_REG_PAGE_SELECT         0xfe
#define BF3031_REG_CHIP_ID_H           0xfc
#define BF3031_REG_CHIP_ID_L           0xfd
#define BF3031_REG_F2                  0xf2

#ifdef __cplusplus
}
#endif
