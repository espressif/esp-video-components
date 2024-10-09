/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_ipa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Image process algorithm pipeline object handle
 */
typedef esp_ipa_pipeline_t *esp_ipa_pipeline_handle_t;

/**
 * @brief Enable or disable image process algorithm pipeline debug log.
 *
 * @param enable    true: enable debug log, false: disable debug log
 *
 * @return None
 */
void esp_ipa_pipeline_set_log(bool enable);

/**
 * @brief Print image process algorithm pipeline information.
 *
 * @param handle    Image process algorithm pipeline object handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_ipa_pipeline_print(esp_ipa_pipeline_handle_t handle);

/**
 * @brief Create image process algorithm pipeline.
 *
 * @param handle    Image process algorithm pipeline object handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_ipa_pipeline_create(uint8_t ipa_nums, const char **ipa_names, esp_ipa_pipeline_handle_t *handle);

/**
 * @brief Initialize image process algorithm pipeline and get initialization ISP/Camera parameters,
 *        These parameters should be set to ISP/Camera before processing IPA pipeline.
 *
 * @param handle    Image process algorithm pipeline object handle
 * @param sensor    Sensor's current information
 * @param metadata  Meta data calculated by image process algorithm pipeline
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_ipa_pipeline_init(esp_ipa_pipeline_handle_t handle, esp_ipa_sensor_t *sensor, esp_ipa_metadata_t *metadata);

/**
 * @brief Put image statistics and sensor information into the image process algorithm
 *        pipeline and process it. The image process algorithm pipeline will calculate
 *        the meta data used to reconfigure ISP to improve image quality.
 *
 *
 * @param handle    Image process algorithm pipeline object handle
 * @param stats     Image statistics information
 * @param sensor    Sensor's current information
 * @param metadata  Meta data calculated by image process algorithm pipeline
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_ipa_pipeline_process(esp_ipa_pipeline_handle_t handle, const esp_ipa_stats_t *stats, const esp_ipa_sensor_t *sensor, esp_ipa_metadata_t *metadata);

/**
 * @brief Destroy image process algorithm pipeline.
 *
 * @param handle    Image process algorithm pipeline object handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_ipa_pipeline_destroy(esp_ipa_pipeline_handle_t handle);

#ifdef __cplusplus
}
#endif
