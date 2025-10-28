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
 * MT9D111 camera sensor register type definition.
 */
typedef struct {
    uint8_t reg;
    uint16_t val;
} mt9d111_reginfo_t;

#ifdef __cplusplus
}
#endif
