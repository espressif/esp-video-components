/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdlib.h>
#include "esp_check.h"
#include "esp_ipa.h"
#include "esp_ipa_detect.h"

#define ESP_IPA_NAME    "esp_ipa_customized_0"

static esp_err_t esp_ipa_customized_0_init(struct esp_ipa *ipa,
        const esp_ipa_sensor_t *sensor,
        esp_ipa_metadata_t *metadata)
{
    esp_ipa_set_int32(ipa, "esp_ipa_customized_0_val", 0);

    return ESP_OK;
}

static void esp_ipa_customized_0_process(struct esp_ipa *ipa,
        const esp_ipa_stats_t *stats,
        const esp_ipa_sensor_t *sensor,
        esp_ipa_metadata_t *metadata)
{
    int32_t val = esp_ipa_get_int32(ipa, "esp_ipa_customized_0_val");

    esp_ipa_set_int32(ipa, "esp_ipa_customized_0_val", val + 1);
}

static void esp_ipa_customized_0_destroy(struct esp_ipa *ipa)
{
    free(ipa);
}

static const esp_ipa_ops_t s_esp_ipa_customized_0_ops = {
    .init = esp_ipa_customized_0_init,
    .process = esp_ipa_customized_0_process,
    .destroy = esp_ipa_customized_0_destroy
};

ESP_IPA_DETECT_FN(esp_ipa_customized_0, ESP_IPA_NAME)
{
    esp_ipa_t *ipa;

    ipa = calloc(1, sizeof(esp_ipa_t));
    if (ipa) {
        ipa->name = ESP_IPA_NAME;
        ipa->ops  = &s_esp_ipa_customized_0_ops;
    }

    return ipa;
}
