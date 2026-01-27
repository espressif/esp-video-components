/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STI2250 camera sensor register type definition.
 */
typedef struct {
    uint8_t reg;
    uint8_t val;
} sti2250_reginfo_t;

/*
 * STI2250 camera sensor register banks definition.
 */
typedef enum {
    STI2250_BANK0 = 0x00,
    STI2250_BANK1 = 0x01,
    STI2250_BANK2 = 0x02,
    STI2250_BANK3 = 0x03,
    STI2250_BANK_MAX
} sti2250_bank_t;

#ifdef __cplusplus
}
#endif
