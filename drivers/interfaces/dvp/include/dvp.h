/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <sys/queue.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "hal/cam_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_DVP_DATA_BUS_WIDTH_8BITS
#define DVP_INTF_DATA_PIN_NUM    8
#elif CONFIG_DVP_DATA_BUS_WIDTH_16BITS
#define DVP_INTF_DATA_PIN_NUM    16
#endif

/**
 * @brief DVP event type
 */
typedef enum dvp_int_event_type {
    DVP_EVENT_VSYNC_START = 0,                  /*!< DVP has received V-Sync start signal, and DVP will start receiving data */
    DVP_EVENT_DATA_RECVED = 1,                  /*!< DVP has received a block of data, but not completed one frame */
    DVP_EVENT_VSYNC_END   = 2,                  /*!< DVP has received V-Sync end signal, and DVP has receive one full frame */
} dvp_int_event_type_t;

/**
 * @brief DVP receive callback function return code
 */
typedef enum dvp_rx_cb_ret {
    DVP_RXCB_DONE = 0,                          /*!< Return this code if user's callback function finishes processing
                                                     data in buffer, this means the buffer can be used by DVP driver again,
                                                     so DVP driver will insert it into receive buffer queue. */

    DVP_RX_CB_CACHED = 1,                       /*!< Return this code if user's callback function just cache data buffer
                                                     instead of processing it, this means the buffer can't be used by DVP driver
                                                     again, so DVP driver will drop(not free) this buffer. */
} dvp_rx_cb_ret_t;

/**
 * @brief DVP receive result passing to receive callback function
 */
typedef enum dvp_rx_ret {
    DVP_RX_SUCCESS  = 0,                        /*!< Successfully to receive one completed frame */
    DVP_RX_OVERFLOW = 1,                        /*!< Error triggers, received data length is larger than given buffer length */
} dvp_rx_ret_t;

/**
 * @brief DVP device current state
 */
typedef enum dvp_state {
    DVP_DEV_IDLE  = 0,                          /*!< DVP is idle state */
    DVP_DEV_WAIT  = 1,                          /*!< DVP wait V-Sync start signal */
    DVP_DEV_RXING = 2,                          /*!< DVP is receiving data but not one completed frame */
    DVP_DEV_RXED  = 3,                          /*!< DVP has one completed frame */
    DVP_DEV_BLOCK = 4,                          /*!< DVP blocks to receive buffer due to no buffer */
} dvp_state_t;

/**
 * @brief DVP frame list
 */
typedef SLIST_HEAD(dvp_frame_list, dvp_frame) dvp_frame_list_t;

/**
 * @brief DVP receive callback function, this function is called in high priority task once one frame is received, so it should be:
 *
 *        1. cost time should be less
 *        2. don't call blocking function, such as mutex, semaphore and so on
 *
 * @param rx_ret DVP receive result, only DVP_RX_SUCCESS means receive is success
 * @param buffer Buffer pointer which is added by API "dvp_device_add_buffer"
 * @param size   Received data size actually
 * @param priv   Private data
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
typedef dvp_rx_cb_ret_t (*dvp_rx_cb_t)(dvp_rx_ret_t rx_ret, uint8_t *buffer, size_t size, void *priv);

/**
 * @brief DVP free buffer callback function, all buffer is added by API "dvp_device_add_buffer".
 *
 * @param buffer Buffer pointer which is added by API "dvp_device_add_buffer"
 * @param priv   Private data
 *
 * @return None
 */
typedef void (*dvp_free_buf_cb_t)(uint8_t *buffer, void *priv);

/**
 * @brief DVP interrupt event
 */
typedef struct dvp_int_event {
    dvp_int_event_type_t type;                      /*!< DVP interrupt event type */
} dvp_int_event_t;

/**
 * @brief DVP Pin configuration
 */
typedef struct dvp_pin_config {
    uint8_t data_pin[DVP_INTF_DATA_PIN_NUM];    /*!< DVP data pin number */
    uint8_t vsync_pin;                          /*!< DVP V-Sync pin number */
    uint8_t href_pin;                           /*!< DVP HREF pin number */
    uint8_t pclk_pin;                           /*!< DVP PCLK pin number */

#ifdef CONFIG_DVP_SUPPORT_H_SYNC
    /**
      * Todo: AEG-1110
      */

    uint8_t hsync_pin;                          /*!< DVP H-Sync pin number */
#endif

#ifdef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
    uint8_t xclk_pin;                           /*!< DVP Output clock pin number */
    uint32_t xclk_freq;                         /*!< DVP Output clock value in HZ */
#endif
} dvp_pin_config_t;

/**
 * @brief DVP device interface configuration
 */
typedef struct dvp_device_interface_config {
    /**
     * DVP port number, descrption is as follows
     *  - ESP32:    port = 0(I2S)
     *  - ESP32-S2: port = 0(I2S)
     *  - ESP32-S3: port = 0(LCD_CAM)
     */
    uint8_t port;

    dvp_pin_config_t pin;                       /*!< DVP Pin configuration */

    dvp_rx_cb_t rx_cb;                          /*!< DVP receive frame done callback function */
    dvp_free_buf_cb_t free_buf_cb;              /*!< DVP free buffer callback function */
    void *priv;                                 /*!< DVP callback function private data */

    size_t size;                                /*!< DVP cache buffer size */
    uint8_t buffer_align_size;                  /*!< DVP cache buffer align size */

    uint16_t width;                             /*!< Camera picture width */
    uint16_t height;                            /*!< Camera picture height */
} dvp_device_interface_config_t;

/**
 * @brief DVP buffer
 */
typedef struct dvp_frame {
    SLIST_ENTRY(dvp_frame) node;                /*!< Frame list node */

    uint8_t *buffer;                            /*!< Frame buffer pointer */
    size_t length;                              /*!< Frame buffer length */
    size_t size;                                /*!< Frame actually receive bytes */
} dvp_frame_t;

/**
 * @brief DVP device object data
 */
typedef struct dvp_device {
    uint8_t port;                               /*!< DVP port number */
    cam_hal_context_t hal;                      /*!< DVP hardware interface object data */

    dvp_state_t state;                          /*!< DVP device current state */
    SemaphoreHandle_t mutex;                    /*!< DVP mutex */

    dvp_frame_list_t frame_list;                /*!< DVP receive buffer list */
    dvp_frame_t *cur_frame;                     /*!< DVP current buffer pointer */

    uint8_t vsync_pin;                          /*!< DVP V-Sync pin number */
    intr_handle_t intr_handle;                  /*!< DVP DMA receive interrupt handle */
    QueueHandle_t event_queue;                  /*!< DVP event queue */
    TaskHandle_t task_handle;                   /*!< DVP task handle */

    uint8_t *buffer;                            /*!< DVP cache buffer */
    size_t size;                                /*!< DVP cache buffer size */
    size_t hsize;                               /*!< DVP cache buffer half size */
    size_t item_size;                           /*!< DVP cache receive data item size */
    lldesc_t *lldesc;                           /*!< DVP cache buffer DMA description */
    size_t lldesc_cnt;                          /*!< DVP cache buffer DMA description count */
    size_t lldesc_hcnt;                         /*!< DVP cache buffer DMA description half count */
    size_t lldesc_index;                        /*!< DVP cache buffer DMA description index */

    dvp_rx_cb_t rx_cb;                          /*!< DVP receive frame done callback function */
    dvp_free_buf_cb_t free_buf_cb;              /*!< DVP free buffer callback function */
    void *priv;                                 /*!< DVP callback function private data */
} dvp_device_t;

/**
 * @brief Handle for a camera interface device
 */
typedef dvp_device_t *dvp_device_handle_t;

/**
 * @brief Create DVP device by given configuration
 *
 * @param handle DVP device handle pointer
 * @param config DVP configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_create(dvp_device_handle_t *handle, const dvp_device_interface_config_t *config);

/**
 * @brief Destroy DVP object created by "dvp_device_create", and all buffer added by
 *        API "dvp_device_add_buffer" will be freed by registered callback function "free_buf_cb".
 *
 * @param dvp DVP device handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_destroy(dvp_device_handle_t handle);

/**
 * @brief Start DVP capturing data from camera
 *
 * @param dvp DVP device handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_start(dvp_device_handle_t handle);

/**
 * @brief Stop DVP capturing data from camera
 *
 * @param dvp DVP device handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_stop(dvp_device_handle_t handle);

/**
 * @brief Add buffer to DVP, and then DVP driver will bind this buffer to DMA.
 *
 * @param handle DVP device handle
 * @param buffer Data receive buffer, this buffer address shoule be 2^N(N = 2, 4, 8)
 *               bytes align based on SoC's DMA requirement
 * @param size   Buffer size
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_add_buffer(dvp_device_handle_t handle, uint8_t *buffer, size_t size);

#ifdef __cplusplus
}
#endif
