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

#if CONFIG_ESP_IPA_DETECT_METHOD_DYNAMIC_LINK
/**
 * @brief Define an IPA detect function which can be executed automatically, in application layer.
 *
 * @param f  function name (identifier)
 * @param n  IPA name
 * @param (varargs)  optional, additional attributes for the function declaration (such as IRAM_ATTR)
 *
 * There should be at lease one undefined symble to be added in the IPA driver in order to avoid
 * the optimization of the linker. Because otherwise the linker will ignore IPA driver as it has
 * no other files depending on any symbols in it.
 *
 * Some thing like this should be added in the CMakeLists.txt of the IPA driver:
 *  target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_ian")
 */
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
#else /* CONFIG_ESP_IPA_DETECT_METHOD_STATIC_STORE */
/**
 * @brief Define a public IPA detect function.
 *
 * @param f  function name (identifier)
 * @param n  IPA name
 * @param (varargs)  optional, additional attributes for the function declaration (such as IRAM_ATTR)
 */
#define ESP_IPA_DETECT_FN(f, n, ...)   \
    esp_ipa_t *__esp_ipa_detect_fn_##f(const esp_ipa_config_t *config)
#endif /* CONFIG_ESP_IPA_DETECT_METHOD_STATIC_STORE */

/**
 * @brief Get the array of IPA detect functions.
 *
 * @param array_start_ptr Pointer to the start of the array.
 * @param array_end_ptr Pointer to the end of the array.
 */
void esp_ipa_detect_get_array(esp_ipa_detect_t **array_start_ptr, esp_ipa_detect_t **array_end_ptr);

#ifdef __cplusplus
}
#endif
