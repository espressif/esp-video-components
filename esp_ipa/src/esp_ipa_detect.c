/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_ipa_detect.h"

#if CONFIG_ESP_IPA_DETECT_METHOD_STATIC_STORE

#define ESP_IPA_DETECT_ENTRY(f, n) \
    { .detect = __esp_ipa_detect_fn_##f, .name = (n) }

#define ESP_IPA_DETECT_DECLARE(f) \
    esp_ipa_t *__esp_ipa_detect_fn_##f(const esp_ipa_config_t *config)

#if CONFIG_ESP_IPA_IAN_ALGORITHM
ESP_IPA_DETECT_DECLARE(esp_ipa_ian);
#endif
#if CONFIG_ESP_IPA_ADN_ALGORITHM
ESP_IPA_DETECT_DECLARE(esp_ipa_adn);
#endif
#if CONFIG_ESP_IPA_AWB_ALGORITHM
ESP_IPA_DETECT_DECLARE(esp_ipa_awb);
#endif
#if CONFIG_ESP_IPA_ACC_ALGORITHM
ESP_IPA_DETECT_DECLARE(esp_ipa_acc);
#endif
#if CONFIG_ESP_IPA_AGC_ALGORITHM
ESP_IPA_DETECT_DECLARE(esp_ipa_agc);
#endif
#if CONFIG_ESP_IPA_AEN_ALGORITHM
ESP_IPA_DETECT_DECLARE(esp_ipa_aen);
#endif
#if CONFIG_ESP_IPA_ATC_ALGORITHM
ESP_IPA_DETECT_DECLARE(esp_ipa_atc);
#endif
#if CONFIG_ESP_IPA_EXT_CONFIG
ESP_IPA_DETECT_DECLARE(esp_ipa_ext);
#endif
#if CONFIG_ESP_IPA_AF_ALGORITHM
ESP_IPA_DETECT_DECLARE(esp_ipa_af);
#endif

static const esp_ipa_detect_t __esp_ipa_detect_array_start[] = {
#if CONFIG_ESP_IPA_IAN_ALGORITHM
    ESP_IPA_DETECT_ENTRY(esp_ipa_ian, "esp_ipa_ian"),
#endif
#if CONFIG_ESP_IPA_ADN_ALGORITHM
    ESP_IPA_DETECT_ENTRY(esp_ipa_adn, "esp_ipa_adn"),
#endif
#if CONFIG_ESP_IPA_AWB_ALGORITHM
    ESP_IPA_DETECT_ENTRY(esp_ipa_awb, "esp_ipa_awb"),
#endif
#if CONFIG_ESP_IPA_ACC_ALGORITHM
    ESP_IPA_DETECT_ENTRY(esp_ipa_acc, "esp_ipa_acc"),
#endif
#if CONFIG_ESP_IPA_AGC_ALGORITHM
    ESP_IPA_DETECT_ENTRY(esp_ipa_agc, "esp_ipa_agc"),
#endif
#if CONFIG_ESP_IPA_AEN_ALGORITHM
    ESP_IPA_DETECT_ENTRY(esp_ipa_aen, "esp_ipa_aen"),
#endif
#if CONFIG_ESP_IPA_ATC_ALGORITHM
    ESP_IPA_DETECT_ENTRY(esp_ipa_atc, "esp_ipa_atc"),
#endif
#if CONFIG_ESP_IPA_EXT_CONFIG
    ESP_IPA_DETECT_ENTRY(esp_ipa_ext, "esp_ipa_ext"),
#endif
#if CONFIG_ESP_IPA_AF_ALGORITHM
    ESP_IPA_DETECT_ENTRY(esp_ipa_af, "esp_ipa_af"),
#endif
};

#endif /* CONFIG_ESP_IPA_DETECT_METHOD_STATIC_STORE */

/**
 * @brief Get the array of IPA detect functions.
 *
 * @param array_start_ptr Pointer to the start of the array.
 * @param array_end_ptr Pointer to the end of the array.
 */
void __wrap_esp_ipa_detect_get_array(esp_ipa_detect_t **array_start_ptr, esp_ipa_detect_t **array_end_ptr)
{
#if CONFIG_ESP_IPA_DETECT_METHOD_STATIC_STORE
    int size = sizeof(__esp_ipa_detect_array_start) / sizeof(__esp_ipa_detect_array_start[0]);

    *array_start_ptr = (esp_ipa_detect_t *)__esp_ipa_detect_array_start;
    *array_end_ptr = (esp_ipa_detect_t *)(__esp_ipa_detect_array_start + size);
#else
    *array_start_ptr = (esp_ipa_detect_t *)&__esp_ipa_detect_array_start;
    *array_end_ptr = (esp_ipa_detect_t *)&__esp_ipa_detect_array_end;
#endif
}
