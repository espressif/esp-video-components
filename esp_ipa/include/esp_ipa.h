/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_ipa_types.h"
#include "esp_ipa_cmd.h"

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
 * @brief Rebuild IPA modules from a new configuration and initialize them.
 *
 * Creates a temporary pipeline (IPA modules + map), runs init into @p metadata,
 * then swaps the live @p handle's module/map/config pointers with the temporary
 * pipeline and destroys the temporary object (which now owns the old modules).
 * On failure the temporary pipeline is destroyed and @p handle is unchanged.
 *
 * @note This function is not thread-safe. The caller must ensure it does not
 *       run concurrently with init/process/ioctl/destroy.
 * @note `config->version` must match the current pipeline. `config->nums` and
 *       `config->names[i]` must match the modules loaded at create time, and
 *       each built-in module must have its sub-configuration present.
 *
 * @param[in]  handle   Pipeline handle (pointer value stays the same)
 * @param[in]  config   New IPA configuration
 * @param[in]  sensor   Sensor information for init
 * @param[out] metadata Init metadata from the new modules
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_ipa_pipeline_set_config(esp_ipa_pipeline_handle_t handle,
                                      const esp_ipa_config_t *config,
                                      const esp_ipa_sensor_t *sensor,
                                      esp_ipa_metadata_t *metadata);

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
 * @brief Set fixed color temperature for AWB model 3 (fixed_ct).
 *
 * @note CT must exactly match an entry in `awb.fixed_ct.presets[]`.
 *       Call esp_ipa_pipeline_process() afterwards to apply WBG and ACC tables.
 *
 * @param[in] handle Pipeline handle
 * @param[in] ct     Color temperature in Kelvin
 *
 * @return
 * - ESP_OK On success
 * - ESP_ERR_INVALID_ARG handle is NULL, pipeline has no AWB fixed_ct module, or ct not in presets
 */
esp_err_t esp_ipa_awb_set_fixed_ct(esp_ipa_pipeline_handle_t handle, uint32_t ct);

/**
 * @brief Check if IPA contains of this variable.
 *
 * @note Returns false if `ipa->pipeline` is NULL. Do not call from detect-only
 *       `destroy` — that path must not access the pipeline at all.
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
 * @note Requires `ipa->pipeline` bound (after successful `esp_ipa_pipeline_create`).
 *       Not safe in detect-only `destroy` (`ipa->pipeline` is NULL).
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
 * @note Requires `ipa->pipeline` bound (after successful `esp_ipa_pipeline_create`).
 *       Not safe in detect-only `destroy` (`ipa->pipeline` is NULL).
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
 * @note Requires `ipa->pipeline` bound (after successful `esp_ipa_pipeline_create`).
 *       Not safe in detect-only `destroy` (`ipa->pipeline` is NULL).
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
 * @note Requires `ipa->pipeline` bound (after successful `esp_ipa_pipeline_create`).
 *       Not safe in detect-only `destroy` (`ipa->pipeline` is NULL).
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
 * @note Requires `ipa->pipeline` bound (after successful `esp_ipa_pipeline_create`).
 *       Not safe in detect-only `destroy` (`ipa->pipeline` is NULL).
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
 * @note Requires `ipa->pipeline` bound (after successful `esp_ipa_pipeline_create`).
 *       Not safe in detect-only `destroy` (`ipa->pipeline` is NULL).
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
 *         If one sensor has multiple configurations, this function returns the first matching entry in JSON input file order.
 *         Use esp_ipa_pipeline_enum_configs and compare `description` to select another configuration.
 */
const esp_ipa_config_t *esp_ipa_pipeline_get_config(const char *name);

/**
 * @brief Enumerate IPA configurations of the same sensor in JSON input file order.
 *
 * @param sensor_name  Sensor name
 * @param index        Zero-based index among this sensor's configurations; compare `description` to identify the entry
 *
 * @return IPA configuration pointer if found, or NULL if sensor is not supported or index is out of range
 */
const esp_ipa_config_t *esp_ipa_pipeline_enum_configs(const char *sensor_name, int index);

/**
 * @brief Send command to IPA.
 *
 * @param ipa   Image process algorithm object
 * @param cmd   Command ID
 * @param data  Command data
 *
 * @return ESP_OK on success, otherwise an error code
 */
esp_err_t esp_ipa_pipeline_ioctl(esp_ipa_pipeline_handle_t handle, uint32_t cmd, void *data);

#ifdef __cplusplus
}
#endif
