/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Minimal consumer of esp_ipa via its namespaced name (espressif__esp_ipa).
 * The point of this app is the CMake configure step, not runtime: if esp_ipa's
 * prebuilt library re-introduces a require that only resolves for the bare
 * short name, configure fails here with "Failed to resolve component
 * 'espressif__esp_ipa'". Including the public header keeps the dependency real.
 */
#include "esp_ipa.h"

void app_main(void)
{
    esp_ipa_pipeline_create(NULL, NULL);
    esp_ipa_pipeline_print(NULL);
    esp_ipa_pipeline_init(NULL, NULL, NULL);
    esp_ipa_pipeline_process(NULL, NULL, NULL, NULL);
    esp_ipa_pipeline_destroy(NULL);
}
