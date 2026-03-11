/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * STI2250 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* sti2250 registers */
#define STI2250_REG_DELAY               0xf9
#define STI2250_REG_CHIP_ID_H           0xc6
#define STI2250_REG_CHIP_ID_L           0xc7
#define STI2250_REG_RESET_RELATED       0xf2
#define STI2250_REG_PAGE_SELECT         0xfd
#define STI2250_REG_SYSTEM              0xfe
#define STI2250_REG_EV_UPDATE           0x12 // page0
#define STI2250_REG_EXP_H               0x2d // page0
#define STI2250_REG_EXP_L               0x2c // page0
#define STI2250_REG_A_GAIN_FINE         0x8a // page0
#define STI2250_REG_A_GAIN_COARSE       0x2e // page0

#ifdef __cplusplus
}
#endif
