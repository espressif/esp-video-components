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
 * GC4053 camera sensor register type definition.
 */
typedef struct {
    uint16_t reg;
    uint8_t val;
} gc4053_reginfo_t;

#ifdef __cplusplus
}
#endif
