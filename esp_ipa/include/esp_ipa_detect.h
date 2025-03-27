/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_attr.h"
#include "esp_ipa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Description of automatically detecting IPA
 */
typedef struct esp_ipa_detect {
    const char *name;                       /*!< IPA name */
    esp_ipa_t *(*detect)(const esp_ipa_config_t *config);   /*!< Pointer to the IPA detect function */
} esp_ipa_detect_t;

#define ESP_IPA_DETECT_FN(f, n, ...)                                                        \
    esp_ipa_t * __VA_ARGS__ __esp_ipa_detect_fn_##f(const esp_ipa_config_t *config);        \
    static __attribute__((used)) _SECTION_ATTR_IMPL(".esp_ipa_detect", __COUNTER__)         \
        esp_ipa_detect_t esp_ipa_detect_##f = {                                             \
            .detect = ( __esp_ipa_detect_fn_##f),                                           \
            .name = (n)                                                                     \
        };                                                                                  \
    esp_ipa_t *__esp_ipa_detect_fn_##f(const esp_ipa_config_t *config)

/**
 * @brief IPA auto detect function array start.
 */
extern esp_ipa_detect_t __esp_ipa_detect_array_start;

/**
 * @brief IPA auto detect function array end.
 */
extern esp_ipa_detect_t __esp_ipa_detect_array_end;

#ifdef __cplusplus
}
#endif
