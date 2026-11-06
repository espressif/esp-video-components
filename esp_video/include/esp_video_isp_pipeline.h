/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_video_isp_ioctl.h"
#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#include "esp_ipa.h"
#include "esp_ipa_state.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER

#define ESP_VIDEO_ISP_AF_STATS_WIN              (1 << 0)    /*!< AF statistics window */
#define ESP_VIDEO_ISP_AWB_STATS_WIN             (1 << 1)    /*!< AWB statistics window */
#define ESP_VIDEO_ISP_AE_STATS_WIN              (1 << 2)    /*!< AE statistics window */
#define ESP_VIDEO_ISP_HIST_STATS_WIN            (1 << 3)    /*!< Hist statistics window */

/**
 * @brief AGC status.
 */
typedef enum esp_video_isp_pipeline_agc_status {
    ESP_VIDEO_ISP_PIPELINE_AGC_DISABLE  = 0, /**< AGC disabled */
    ESP_VIDEO_ISP_PIPELINE_AGC_ENABLE   = 1, /**< AGC enabled */
} esp_video_isp_pipeline_agc_status_t;

/**
 * @brief Set AGC status.
 *
 * @param status AGC status
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_set_agc_status(esp_video_isp_pipeline_agc_status_t status);

/**
 * @brief Get AGC status.
 *
 * @param status Pointer to store AGC status
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_get_agc_status(esp_video_isp_pipeline_agc_status_t *status);

/**
 * @brief Create a queue to dump ISP statistics.
 *
 * @note The queue will be created in the internal memory of the ISP controller.
 * @note This function will decrease the ISP pipeline performance, so if not necessary, please don't call this function.
 *       Please call esp_video_isp_pipeline_stop_dump_stats() to stop dumping ISP statistics.
 *
 * @param queue_size Queue size
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_start_dump_stats(uint32_t queue_size);

/**
 * @brief Stop dumping ISP statistics to a queue.
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_stop_dump_stats(void);

/**
 * @brief Dump ISP statistics to a queue.
 *
 * @param stats Pointer to store ISP statistics
 * @param timeout_ms Timeout in milliseconds
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_dump_stats(esp_video_isp_stats_t *stats, uint32_t timeout_ms);

/**
 * @brief Set statistics window.
 *
 * @param target_windows Target windows masks, which can be a combination of the following:
 *      - ESP_VIDEO_ISP_AF_STATS_WIN
 *      - ESP_VIDEO_ISP_AWB_STATS_WIN
 *      - ESP_VIDEO_ISP_AE_STATS_WIN
 *      - ESP_VIDEO_ISP_HIST_STATS_WIN
 * @param left Left-up X coordinate of the window
 * @param top Left-up Y coordinate of the window
 * @param width Window width
 * @param height Window height
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_set_statistics_window(uint32_t target_windows, uint32_t left, uint32_t top, uint32_t width, uint32_t height);

/**
 * @brief Set AGC maximum exposure time.
 *
 * @note The exposure time is in microseconds. The value must be within the
 *       sensor exposure range [min, max] and will be aligned to the exposure step.
 *
 * @param exposure_us Maximum exposure time in microseconds
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if exposure is out of range
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_set_agc_max_exposure(uint32_t exposure_us);

/**
 * @brief Get AGC maximum exposure time.
 *
 * @param exposure_us Pointer to store maximum exposure time in microseconds
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_get_agc_max_exposure(uint32_t *exposure_us);

/**
 * @brief Set AGC minimum exposure time.
 *
 * @note The exposure time is in microseconds. The value must be within the
 *       sensor exposure range [min, max] and will be aligned to the exposure step.
 *
 * @param exposure_us Minimum exposure time in microseconds
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if exposure is out of range
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_set_agc_min_exposure(uint32_t exposure_us);

/**
 * @brief Get AGC minimum exposure time.
 *
 * @param exposure_us Pointer to store minimum exposure time in microseconds
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_get_agc_min_exposure(uint32_t *exposure_us);

/**
 * @brief Replace the runtime AE luma weight table.
 *
 * @param weight Per-block weights, index = x * ISP_AE_BLOCK_Y_NUM + y.
 *               An all-zero table is rejected.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if weight is NULL or all zeros
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_set_agc_meter_luma_weight(const uint8_t weight[ISP_AE_REGIONS]);

/**
 * @brief Read the runtime AE luma weight table.
 *
 * @param weight Output table, index = x * ISP_AE_BLOCK_Y_NUM + y
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if weight is NULL
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_get_agc_meter_luma_weight(uint8_t weight[ISP_AE_REGIONS]);

/**
 * @brief Restore the JSON AE weight table.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_reset_agc_meter_weight(void);

/**
 * @brief Restore the JSON AF windows and weights, then restart the focus scan.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_SUPPORTED if this pipeline has no AF
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_reset_af(void);

/**
 * @brief Replace the runtime AF windows and restart the focus scan.
 *
 * Coordinates are pixels of the ISP statistics window. ``(0, 0)`` is the
 * top-left. Each edge must be less than 4095. ``top_left`` must not be past
 * ``btm_right``. This API does not check against the current sensor frame
 * size; the application must keep windows inside the active format
 * (for example 640x480 or 1280x720). The next IPA process writes the
 * windows to the ISP.
 *
 * @param windows Three AF sampling windows
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if windows is NULL, a window has top-left past
 *        bottom-right, or an edge is out of the 0..4094 range
 *      - ESP_ERR_NOT_SUPPORTED if this pipeline has no AF
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline controller is not initialized
 */
esp_err_t esp_video_isp_pipeline_set_af_windows(const isp_window_t windows[ISP_AF_WINDOW_NUM]);

/**
 * @brief Read the runtime AF windows.
 *
 * @param windows Output windows
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if windows is NULL
 *      - ESP_ERR_NOT_SUPPORTED if this pipeline has no AF
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_get_af_windows(isp_window_t windows[ISP_AF_WINDOW_NUM]);

/**
 * @brief Replace the runtime AF window weights.
 *
 * Scoring uses definition * weight[i] and luminance * weight[i].
 * JSON weight_table is unchanged until reset.
 *
 * @param weight Per-window weights, length ISP_AF_WINDOW_NUM
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if weight is NULL
 *      - ESP_ERR_NOT_SUPPORTED if this pipeline has no AF
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_set_af_window_weight(const uint8_t weight[ISP_AF_WINDOW_NUM]);

/**
 * @brief Read the runtime AF window weights.
 *
 * @param weight Output weights, length ISP_AF_WINDOW_NUM
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if weight is NULL
 *      - ESP_ERR_NOT_SUPPORTED if this pipeline has no AF
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_get_af_window_weight(uint8_t weight[ISP_AF_WINDOW_NUM]);

/**
 * @brief Hold AWB at a color temperature.
 *
 * Model 3 reads fixed_ct.presets[]. Model 2 reads ref_points[] and stops using
 * statistics until esp_video_isp_pipeline_set_awb_reset(). An exact table entry
 * is used as stored. A Kelvin value between two entries interpolates R/G and B/G
 * in mired. A value outside the table is clamped to the nearest entry.
 *
 * @param ct Color temperature in Kelvin. 0 is rejected.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if ct is 0 or this AWB has no color-temperature table
 *      - ESP_ERR_NOT_SUPPORTED if this pipeline has no AWB
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_set_awb_ct(uint32_t ct);

/**
 * @brief Restore the JSON AWB mode.
 *
 * Model 2 resumes statistics. Model 3 stays on the fixed-CT table and
 * restores fixed_ct.default_ct.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if this AWB is not model 2 or model 3
 *      - ESP_ERR_NOT_SUPPORTED if this pipeline has no AWB
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_set_awb_reset(void);

/**
 * @brief Add the same luma shift to JSON target, target_low, and target_high.
 *
 * AE still chooses exposure and gain. Positive values brighten the
 * image and negative values darken it. JSON target plus offset is
 * clamped to 1..255. NaN and infinity are rejected.
 *
 * @param offset Luma shift added to luma_adjust.target / target_low / target_high
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if offset is NaN or infinity
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_set_agc_meter_target_offset(float offset);

/**
 * @brief Get the luma shift added to JSON target, target_low, and target_high.
 *
 * @param offset Pointer to store the current shift
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if offset is NULL
 *      - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_get_agc_meter_target_offset(float *offset);

/**
 * @brief Sensor exposure and gain last applied by AGC.
 */
typedef struct esp_video_isp_pipeline_sensor_ae {
    uint32_t exposure_us;       /*!< Current exposure in microseconds */
    uint32_t min_exposure_us;   /*!< Sensor minimum exposure in microseconds */
    uint32_t max_exposure_us;   /*!< Sensor maximum exposure in microseconds */
    float gain;                 /*!< Current gain; 1.0 is the sensor minimum */
    float min_gain;             /*!< Sensor minimum gain */
    float max_gain;             /*!< Sensor maximum gain */
} esp_video_isp_pipeline_sensor_ae_t;

/**
 * @brief Get the exposure and gain last applied to the sensor
 *
 * These are the values AGC wrote, not the JSON target. If exposure and
 * gain sit at the sensor maximum, raising the target cannot brighten more.
 *
 * @param[out] ae Pointer to store the current exposure and gain
 *
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if ae is NULL
 * - ESP_ERR_INVALID_STATE if the ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_get_sensor_ae(esp_video_isp_pipeline_sensor_ae_t *ae);

/**
 * @brief Get int32_t type IPA environment variable.
 *
 * @param name  Environment variable name
 * @param val   Pointer to store int32_t type environment variable
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if the variable does not exist
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_get_env_int32(const char *name, int32_t *val);

/**
 * @brief Get float type IPA environment variable.
 *
 * @param name  Environment variable name
 * @param val   Pointer to store float type environment variable
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if the variable does not exist
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_get_env_float(const char *name, float *val);

/**
 * @brief Set float type IPA environment variable.
 *
 * Creates the variable when it does not exist yet. The IR node reads
 * "ir_env_luma" from here when the sensor JSON sets luma_env to that name.
 *
 * @param name  Environment variable name
 * @param val   Float value
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG name is NULL
 *      - ESP_ERR_INVALID_STATE ISP pipeline is not initialized
 */
esp_err_t esp_video_isp_pipeline_set_env_float(const char *name, float val);

/**
 * @brief Enumerate IPA configurations of the same sensor.
 *
 * @param sensor_name Sensor name
 * @param index       Zero-based index among configurations of this sensor
 *
 * @return IPA configuration pointer if found, or NULL if sensor is not supported or index is out of range
 */
const esp_ipa_config_t *esp_video_isp_pipeline_enum_ipa_configs(const char *sensor_name, int index);

/**
 * @brief Rebuild IPA modules from a new configuration and apply init metadata to ISP/camera.
 *
 * @note This function serializes with the ISP pipeline task. `config->nums` and
 *       `config->names[i]` must match the modules loaded at create time.
 * @note ESP_OK only means the JSON configuration was switched and parameters were
 *       issued to hardware. Whether the hardware is correctly programmed with the
 *       new parameters must be confirmed from runtime logs.
 *
 * @param config New IPA configuration
 *
 * @return
 *      - ESP_OK if JSON was switched and parameters were issued to hardware
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_set_ipa_config(const esp_ipa_config_t *config);

/**
 * @brief Read the computed IPA state.
 *
 * Call while the ISP pipeline is initialized. On ESP32-P4 the ISP is bound to
 * MIPI-CSI, so VIDIOC_STREAMOFF stops both and the state no longer changes.
 * @p state must be the same object later passed to esp_video_isp_pipeline_set_ipa_state().
 * The IPA configuration, width and height must stay the same between the two calls.
 *
 * @param state State filled on success. The caller owns this buffer.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p state is NULL
 *      - ESP_ERR_INVALID_STATE if the pipeline is not initialized
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_get_ipa_state(esp_ipa_state_t *state);

/**
 * @brief Restore a saved IPA state and write it to the ISP and camera.
 *
 * Call after the pipeline is initialized, before VIDIOC_STREAMON of the frame
 * that must use the restored parameters. @p state must come from
 * esp_video_isp_pipeline_get_ipa_state() with the same IPA configuration.
 *
 * @param state State previously returned by esp_video_isp_pipeline_get_ipa_state()
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p state is NULL
 *      - ESP_ERR_INVALID_STATE if the pipeline is not initialized, or @p state
 *        does not match this pipeline
 *      - ESP_ERR_INVALID_VERSION if @p state->version does not match
 *      - ESP_ERR_INVALID_CRC if @p state->crc32 does not match
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_set_ipa_state(const esp_ipa_state_t *state);
#endif /* CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER */

#ifdef __cplusplus
}
#endif
