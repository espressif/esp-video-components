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
 * SP0A39 camera sensor register type definition.
 */
typedef struct {
    uint8_t reg;
    uint8_t val;
} sp0a39_reginfo_t;

#ifdef __cplusplus
}
#endif
