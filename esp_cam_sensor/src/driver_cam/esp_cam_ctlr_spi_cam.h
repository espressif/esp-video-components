/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/spi_slave.h"
#include "esp_cam_ctlr_interface.h"
#include "esp_cam_ctlr_spi.h"
#include "esp_cam_sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SPI CAM finite state machine
 */
typedef enum esp_cam_ctlr_spi_cam_fsm {
    ESP_CAM_CTLR_SPI_CAM_FSM_INIT = 1,                  /*!< SPI CAM initialization state, and next state is "enabled" */
    ESP_CAM_CTLR_SPI_CAM_FSM_ENABLED,                   /*!< SPI CAM enabled state, and next state is "init" or "started" */
    ESP_CAM_CTLR_SPI_CAM_FSM_STARTED,                   /*!< SPI CAM started state, and next state is "init" or "enabled" */
} esp_cam_ctlr_spi_cam_fsm_t;

typedef struct esp_cam_ctlr_spi_cam {
    esp_cam_ctlr_t base;                                /*!< Camera controller base object data */
    esp_cam_ctlr_evt_cbs_t cbs;                         /*!< Camera controller callback functions */
    void *cbs_user_data;                                /*!< Camera controller callback private data */

    spi_host_device_t spi_port;                         /*!< SPI port */
    const esp_cam_sensor_spi_frame_info *frame_info;    /*!< Frame information */

    esp_cam_ctlr_spi_cam_fsm_t fsm;                     /*!< SPI CAM finite state machine */

    spi_slave_transaction_t spi_trans;                  /*!< SPI transaction */

    TaskHandle_t spi_task_handle;                       /*!< SPI task handle, this is used when auto_decode_dis=0 */
    QueueHandle_t spi_recv_queue;                       /*!< SPI receive queue, this is used when auto_decode_dis=0 */

    uint32_t fb_lines;                                  /*!< Input vertical resolution, i.e. the number of lines in a frame */
    uint32_t fb_size_in_bytes;                          /*!< SPI sensor frame buffer size with frame header and line header */
    uint8_t *frame_buffer;                              /*!< SPI sensor image buffer, size is fb_size_in_bytes */

    uint32_t bf_size_in_bytes;                          /*!< SPI sensor backup buffer size without frame header and line header, this is used when auto_decode_dis=0 */
    uint8_t *backup_buffer;                             /*!< SPI sensor backup buffer, size is bf_size_in_bytes, this is used when auto_decode_dis=0 */

    bool bk_buffer_exposed;                             /*!< status of if back_buffer is exposed to users */

    struct {
        uint32_t bk_buffer_dis  : 1;                    /*!< Disable backup buffer */
        uint32_t bk_buffer_sram : 1;                    /*!< Use SRAM for backup buffer */
        uint32_t auto_decode_dis : 1;                   /*!< 1: disable auto decode, letting applications decode the image frame; 0: enable auto decode, applications receive the decoded image frame */
        uint32_t decode_check_dis : 1;                  /*!< 1: disable checking the image frame header and line header, just copy the image data to the destination buffer; 0: enable checking the image frame header and line header before decoding the raw image frame */
    };
} esp_cam_ctlr_spi_cam_t;

#ifdef __cplusplus
}
#endif
