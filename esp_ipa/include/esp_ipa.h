/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
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
 * @param config    Image process algorithm configuration
 * @param handle    Image process algorithm pipeline object handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_ipa_pipeline_create(const esp_ipa_config_t *config, esp_ipa_pipeline_handle_t *handle);

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
esp_err_t esp_ipa_pipeline_init(esp_ipa_pipeline_handle_t handle, const esp_ipa_sensor_t *sensor, esp_ipa_metadata_t *metadata);

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

/**
 * @brief Check if IPA contains of this variable.
 *
 * @param ipa   Image process algorithm object
 * @param name  Variable name
 *
 * @return true if IPA has global variable of this name; false true if IPA has no global variable of this name
 */
bool esp_ipa_has_var(esp_ipa_t *ipa, const char *name);

/**
 * @brief Set int32_t type global variable.
 *
 * @param ipa   Image process algorithm object
 * @param name  Variable name
 * @param val   int32_t type global variable
 *
 * @return None
 */
void esp_ipa_set_int32(esp_ipa_t *ipa, const char *name, int32_t val);

/**
 * @brief Get int32_t type global variable.
 *
 * @param ipa   Image process algorithm object
 * @param name  Variable name
 *
 * @return int32_t type global variable
 */
int32_t esp_ipa_get_int32(esp_ipa_t *ipa, const char *name);

/**
 * @brief Set float type global variable.
 *
 * @param ipa   Image process algorithm object
 * @param name  Variable name
 * @param val   Float type global variable
 *
 * @return None
 */
void esp_ipa_set_float(esp_ipa_t *ipa, const char *name, float val);

/**
 * @brief Get float type global variable.
 *
 * @param ipa   Image process algorithm object
 * @param name  Variable name
 *
 * @return Float type global variable
 */
float esp_ipa_get_float(esp_ipa_t *ipa, const char *name);

/**
 * @brief Set pointer type global variable.
 *
 * @param ipa   Image process algorithm object
 * @param name  Variable name
 * @param ptr   Pointer type global variable
 *
 * @return None
 */
void esp_ipa_set_ptr(esp_ipa_t *ipa, const char *name, const void *ptr);

/**
 * @brief Get pointer type global variable.
 *
 * @param ipa   Image process algorithm object
 * @param name  Variable name
 *
 * @return Pointer type global variable
 */
const void *esp_ipa_get_ptr(esp_ipa_t *ipa, const char *name);

/**
 * @brief Get IPA configuration pointer by target name
 *
 * @param name Target name
 *
 * @return Target IPA configuration pointer if sensor is supported or null if target is not supported.
 */
const esp_ipa_config_t *esp_ipa_pipeline_get_config(const char *name);

#ifdef __cplusplus
}
#endif
