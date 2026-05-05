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
 * sc121at camera sensor register type definition.
 */
typedef struct {
    uint16_t reg;
    uint8_t val;
} sc121at_reginfo_t;

#ifdef __cplusplus
}
#endif
