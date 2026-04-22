/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include "sdkconfig.h"
#include "driver/gpio.h"

#if CONFIG_CAMERA_XCLK_USE_LEDC
#include "driver/ledc.h"
#endif
#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
#include "esp_clock_output.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief xclk generator handle type
 */
typedef void *esp_cam_sensor_xclk_handle_t;

/**
 * @brief Enumerates the possible sources that can generate XCLK
 */
typedef enum {
#if CONFIG_CAMERA_XCLK_USE_LEDC
    ESP_CAM_SENSOR_XCLK_LEDC = 0,
#endif
#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
    ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER = 1,
#endif
} esp_cam_sensor_xclk_source_t;

/**
 * @brief Camera sensor xclk controller configurations
 */
typedef struct esp_cam_sensor_xclk_config {
    union {
#if CONFIG_CAMERA_XCLK_USE_LEDC
        struct {
            ledc_timer_t timer;         ///< The timer source of channel
            ledc_clk_cfg_t clk_cfg;     ///< LEDC source clock from ledc_clk_cfg_t
            ledc_channel_t channel;     ///< LEDC channel used for XCLK (0-7)
            uint32_t xclk_freq_hz;      ///< XCLK output frequency (Hz)
            gpio_num_t xclk_pin;        ///< the XCLK output gpio_num, if you want to use gpio16, xclk_pin = 16
        } ledc_cfg;
#endif
#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
        struct {
            gpio_num_t xclk_pin;        ///< GPIO number to be mapped soc_root_clk signal source
            uint32_t xclk_freq_hz;      ///< XCLK output frequency (Hz)
        } esp_clock_router_cfg;
#endif
    };
} esp_cam_sensor_xclk_config_t;

/**
 * @brief Allocate an XCLK generator context for the given source.
 *
 * @param[in] source Peripheral source used to generate XCLK.
 * @param[out] ret_handle Pointer to receive the allocated XCLK control handle.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_NO_MEM: Not enough memory to allocate the XCLK context
 *      - ESP_ERR_INVALID_ARG: ret_handle is NULL or source is not supported
 */
esp_err_t esp_cam_sensor_xclk_allocate(esp_cam_sensor_xclk_source_t source, esp_cam_sensor_xclk_handle_t *ret_handle);

/**
 * @brief Configure the clock source and start XCLK output.
 *
 * @param[in] xclk_handle XCLK control handle returned by esp_cam_sensor_xclk_allocate().
 * @param[in] config Configuration for the backend selected at allocate time (LEDC or clock router).
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle, NULL config, or invalid fields in config
 *      - ESP_ERR_NOT_SUPPORTED: The backend does not implement start
 *      - ESP_FAIL or other codes: Returned by LEDC or esp_clock_output, depending on the XCLK source
 */
esp_err_t esp_cam_sensor_xclk_start(esp_cam_sensor_xclk_handle_t xclk_handle, const esp_cam_sensor_xclk_config_t *config);

/**
 * @brief Stop XCLK output.
 *
 * @param[in] xclk_handle XCLK control handle returned by esp_cam_sensor_xclk_allocate().
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *      - ESP_ERR_NOT_SUPPORTED: The backend does not implement stop
 */
esp_err_t esp_cam_sensor_xclk_stop(esp_cam_sensor_xclk_handle_t xclk_handle);

/**
 * @brief Free the XCLK generator context.
 *
 * @param[in] xclk_handle XCLK control handle returned by esp_cam_sensor_xclk_allocate().
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *      - ESP_ERR_NOT_SUPPORTED: The backend does not implement free
 */
esp_err_t esp_cam_sensor_xclk_free(esp_cam_sensor_xclk_handle_t xclk_handle);

#ifdef __cplusplus
}
#endif
