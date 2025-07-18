/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * BF3901 camera sensor register type definition.
 */
typedef struct {
    uint8_t reg;
    uint8_t val;
} bf3901_reginfo_t;

#ifdef __cplusplus
}
#endif
