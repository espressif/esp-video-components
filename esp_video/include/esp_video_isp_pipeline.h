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
#endif /* CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER */

#ifdef __cplusplus
}
#endif
