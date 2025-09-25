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
#include "driver/parlio_rx.h"
#include "esp_cam_ctlr_interface.h"
#include "esp_cam_ctlr_spi.h"
#include "esp_cam_sensor_types.h"

#if CONFIG_CAM_CTLR_SPI_DISABLE_BACKUP_BUFFER
#define CAM_CTLR_SPI_HAS_BACKUP_BUFFER 0
#else /* CONFIG_CAM_CTLR_SPI_DISABLE_BACKUP_BUFFER */
#define CAM_CTLR_SPI_HAS_BACKUP_BUFFER 1
#endif /* CONFIG_CAM_CTLR_SPI_DISABLE_BACKUP_BUFFER */

#if CONFIG_CAM_CTLR_SPI_DISABLE_AUTO_DECODE
#define CAM_CTLR_SPI_HAS_AUTO_DECODE 0
#else /* CONFIG_CAM_CTLR_SPI_DISABLE_AUTO_DECODE */
#define CAM_CTLR_SPI_HAS_AUTO_DECODE 1
#endif /* CONFIG_CAM_CTLR_SPI_DISABLE_AUTO_DECODE */

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

    esp_cam_ctlr_spi_cam_intf_t intf;                   /*!< SPI CAM interface type */

    /**
     * Peripheral interface private data
     */
    union {
#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
        struct {
            parlio_rx_unit_handle_t rx_unit;            /*!< PARLIO RX unit */
            parlio_rx_delimiter_handle_t rx_delimiter;  /*!< PARLIO delimiter */

            QueueHandle_t ms_queue;                     /*!< Message queue */
            TaskHandle_t ms_task;                       /*!< Message task handle */

            uint32_t frame_size;                        /*!< Frame size without cache alignment */
        } parlio;
#endif /* CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO */

        struct {
            spi_host_device_t port;                     /*!< SPI port */
        } spi;
    };

    gpio_num_t spi_cs_pin;                              /*!< SPI CS pin */
    spi_slave_transaction_t spi_trans;                  /*!< SPI transaction, parlio also uses this transaction to reduce repetitive processing code */

    const esp_cam_sensor_spi_frame_info *frame_info;    /*!< Frame information */

    esp_cam_ctlr_spi_cam_fsm_t fsm;                     /*!< SPI CAM finite state machine */

    uint8_t drop_frame_count;                           /*!< Drop frame count after start SPI sensor */
    uint8_t dropped_frame_count;                        /*!< Dropped frame count after start SPI sensor */

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
    TaskHandle_t spi_task_handle;                       /*!< SPI task handle, this is used when auto_decode_dis=0 */
    QueueHandle_t spi_recv_queue;                       /*!< SPI receive queue, this is used when auto_decode_dis=0 */
#endif

#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
    uint8_t *frame_buffer;                              /*!< SPI sensor image buffer, size is fb_size_in_bytes */

    bool bk_buffer_exposed;                             /*!< status of if back_buffer is exposed to users */

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
    uint8_t *backup_buffer;                             /*!< SPI sensor backup buffer, size is bf_size_in_bytes, this is used when auto_decode_dis=0 */
#endif
#else
    uint8_t *spi_ll_buffer;                             /*!< SPI sensor low level buffer */
    uint32_t spi_ll_buffer_size;                        /*!< SPI sensor low level buffer size */
#endif

    uint32_t fb_lines;                                  /*!< Input vertical resolution, i.e. the number of lines in a frame */
    uint32_t fb_size_in_bytes;                          /*!< SPI sensor frame buffer size with frame header and line header, this is cache aligned */
    uint32_t bf_size_in_bytes;                          /*!< SPI sensor backup buffer size without frame header and line header, this is used when auto_decode_dis=0 */

    struct {
#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
        uint32_t bk_buffer_dis  : 1;                    /*!< Disable backup buffer */
#else
        uint32_t setup_ll_buffer : 1;                   /*!< Setup low level buffer */
#endif

#if CONFIG_SPIRAM
        uint32_t bk_buffer_sram : 1;                    /*!< Use SRAM for backup buffer */
#endif

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
        uint32_t auto_decode_dis : 1;                   /*!< 1: disable auto decode, letting applications decode the image frame; 0: enable auto decode, applications receive the decoded image frame */
#endif

        uint32_t decode_check_dis : 1;                  /*!< 1: disable checking the image frame header and line header, just copy the image data to the destination buffer; 0: enable checking the image frame header and line header before decoding the raw image frame */
    };
} esp_cam_ctlr_spi_cam_t;

#ifdef __cplusplus
}
#endif
