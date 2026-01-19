/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * mira220 camera sensor register type definition.
 */
typedef struct {
    uint16_t reg;
    uint8_t val;
} mira220_reginfo_t;

#ifdef __cplusplus
}
#endif
