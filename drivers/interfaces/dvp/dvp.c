/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal/gpio_ll.h"
#include "driver/gpio.h"
#include "soc/lldesc.h"
#include "esp_private/periph_ctrl.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "soc/cam_periph.h"
#include "dvp.h"

#define DVP_TASK_STACK_SIZE                 CONFIG_DVP_TASK_STACK_SIZE
#define DVP_DMA_DIV                         2
#define DVP_EVENT_QUEUE_SIZE                2

#define DVP_PORT_MAX                        (SOC_CAM_PERIPH_NUM - 1)

#define DVP_CUR_BUF(d)                      (&(d)->buffer[(d)->lldesc_index * (d)->hsize])
#define DVP_CUR_LLDESC(d)                   (&(d)->lldesc[(d)->lldesc_index * (d)->lldesc_hcnt])
#define FRAME_CUR_BUF(f)                    (&(f)->buffer[(f)->size])

#define CONFIG_GPIO(pin, sig)                               \
{                                                           \
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);  \
    ret = gpio_set_direction(pin, GPIO_MODE_INPUT);         \
    if (ret != ESP_OK) {                                    \
        goto errout_disable_intr;                           \
    }                                                       \
    ret = gpio_set_pull_mode(pin, GPIO_FLOATING);           \
    if (ret != ESP_OK) {                                    \
        goto errout_disable_intr;                           \
    }                                                       \
    esp_rom_gpio_connect_in_signal(pin, sig, false);        \
}

static const char *TAG = "dvp";

/**
 * @brief Get and clear DMA receive data size
 *
 * @param dvp DVP object data pointer
 *
 * @return DMA receive data size
 */
static size_t get_and_clear_dma_recv_size(dvp_device_handle_t dvp)
{
    size_t size = 0;
    lldesc_t *lldesc = DVP_CUR_LLDESC(dvp);

    for (int i = 0; i < dvp->lldesc_hcnt; i++) {
        size += lldesc[i].length;
        lldesc[i].length = 0;
    }

    return size;
}

/**
 * @brief Start DVP capturing data from camera
 *
 * @param dvp DVP object data pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_start_receive(dvp_device_t *dvp)
{
    esp_err_t ret = ESP_ERR_NOT_FOUND;

    if (!SLIST_EMPTY(&dvp->frame_list)) {
        ret = gpio_intr_enable(dvp->vsync_pin);
        if (ret == ESP_OK) {
            dvp_frame_t *frame = SLIST_FIRST(&dvp->frame_list);
            SLIST_REMOVE(&dvp->frame_list, frame, dvp_frame, node);
            dvp->cur_frame = frame;
        }
    }

    return ret;
}

/**
 * @brief DVP receive V-Sync interrupt interrupt callback function
 *
 * @param arg This pointer is DVP object data pointer
 *
 * @return None
 */
static IRAM_ATTR void dvp_vsync_isr(void *arg)
{
    BaseType_t ret;
    dvp_int_event_t event;
    BaseType_t need_switch = pdFALSE;
    dvp_device_t *dvp = (dvp_device_t *)arg;

    if (gpio_ll_get_level(&GPIO, dvp->vsync_pin)) {
        event.type = DVP_EVENT_VSYNC_START;
    } else {
        event.type = DVP_EVENT_VSYNC_END;
    }

    ret = xQueueSendFromISR(dvp->event_queue, &event, &need_switch);
    if (ret == pdPASS) {
        if (need_switch == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    } else {
        ESP_EARLY_LOGE(TAG, "failed to send vsync event type=%d", event.type);
    }
}

/**
 * @brief DVP receive data interrupt callback function
 *
 * @param arg This pointer is DVP object data pointer
 *
 * @return None
 */
static IRAM_ATTR void dvp_dma_isr(void *arg)
{
    BaseType_t ret;
    dvp_int_event_t event;
    BaseType_t need_switch = pdFALSE;
    dvp_device_t *dvp = (dvp_device_t *)arg;
    uint32_t status = cam_hal_get_int_status(&dvp->hal);

    if (status) {
        cam_hal_clear_int_status(&dvp->hal, status);

        if (status & CAM_RX_INT_MASK) {
            event.type = DVP_EVENT_DATA_RECVED;
            ret = xQueueSendFromISR(dvp->event_queue, &event, &need_switch);
            if (ret == pdPASS) {
                if (need_switch == pdTRUE) {
                    portYIELD_FROM_ISR();
                }
            } else {
                ESP_EARLY_LOGE(TAG, "failed to send data received event");
            }
        }
    }
}

/**
 * @brief DVP receive signal and data task, this function will call receive callback
 *        function if one complete frame is received or error triggers
 *
 * @param p This pointer is DVP object data pointer
 *
 * @return None
 */
static void dvp_task(void *p)
{
    dvp_int_event_t event;
    dvp_device_t *dvp = (dvp_device_t *)p;

    while (1) {
        if (xQueueReceive(dvp->event_queue, &event, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "failed to receive message");
            continue;
        }

        /* Taking semaphore must not fail */

        BaseType_t ret = xSemaphoreTake(dvp->mutex, portMAX_DELAY);
        assert(ret == pdTRUE);

        /**
         * Todo: AEG-1174
         */
        switch (event.type) {
        case DVP_EVENT_VSYNC_START: {
            if (dvp->state == DVP_DEV_WAIT) {

                dvp_frame_t *frame = dvp->cur_frame;

                /* Reset receive state data and start DVP receive */

                frame->size = 0;
                dvp->lldesc_index = 0;
                cam_hal_start_streaming(&dvp->hal, dvp->lldesc, dvp->hsize);
                dvp->state = DVP_DEV_RXING;
            }
            break;
        }
        case DVP_EVENT_DATA_RECVED: {
            if (dvp->state == DVP_DEV_RXING) {
                size_t data_size = get_and_clear_dma_recv_size(dvp);
                size_t frame_size = data_size / dvp->item_size;
                dvp_frame_t *frame = dvp->cur_frame;

                /* Calculate received data size and check if frame left space is enough */

                if ((frame->size + frame_size) < frame->length) {
                    /* Decode received data and update receive state data */

                    cam_hal_memcpy(&dvp->hal, FRAME_CUR_BUF(frame), DVP_CUR_BUF(dvp), data_size);
                    frame->size += frame_size;
                    dvp->lldesc_index = (dvp->lldesc_index + 1) % DVP_DMA_DIV;
                } else {
                    /* Call receive function with overflow error code */

                    dvp->rx_cb(DVP_RX_OVERFLOW, frame->buffer, frame->size, dvp->priv);

                    /* Stop receiving data and reset DVP state */

                    cam_hal_stop_streaming(&dvp->hal);
                    dvp->state = DVP_DEV_WAIT;
                }
            }
            break;
        }
        case DVP_EVENT_VSYNC_END: {
            if (dvp->state == DVP_DEV_RXING) {
                size_t data_size;
                size_t frame_size;
                dvp_rx_cb_ret_t ret;
                dvp_frame_t *frame = dvp->cur_frame;

                /* Stop DVP receive, and then no event will be sent */

                dvp->state = DVP_DEV_RXED;
                cam_hal_stop_streaming(&dvp->hal);
                gpio_intr_disable(dvp->vsync_pin);

                /* Mark current frame is NULL, this shows no receive is started */

                dvp->cur_frame = NULL;

                /* Calculate received data size and check if frame left space is enough */

                data_size = get_and_clear_dma_recv_size(dvp);
                frame_size = data_size / dvp->item_size;
                if ((frame->size + frame_size) < frame->length) {
                    /* Decode received data and update receive state data */

                    cam_hal_memcpy(&dvp->hal, FRAME_CUR_BUF(frame), DVP_CUR_BUF(dvp), data_size);
                    frame->size += frame_size;

                    ret = dvp->rx_cb(DVP_RX_SUCCESS, frame->buffer, frame->size, dvp->priv);
                } else {
                    /* Call receive function with overflow error code */

                    ret = dvp->rx_cb(DVP_RX_OVERFLOW, frame->buffer, frame->size, dvp->priv);
                }

                if (ret == DVP_RXCB_DONE) {
                    /* Insert frame to list, and the frame can be used again */


                    SLIST_INSERT_HEAD(&dvp->frame_list, frame, node);

                } else if (ret == DVP_RX_CB_CACHED) {
                    /* Free the frame */

                    heap_caps_free(frame);
                }

                /* Start DVP receive, this is successful only when there is frame in receive list */

                if (dvp_start_receive(dvp) != ESP_OK) {
                    dvp->state = DVP_DEV_BLOCK;
                } else {
                    dvp->state = DVP_DEV_WAIT;
                }
            }

            break;
        }
        default:
            break;
        }

        xSemaphoreGive(dvp->mutex);
    }
}

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
esp_err_t dvp_device_create(dvp_device_handle_t *handle, const dvp_device_interface_config_t *config)
{
    esp_err_t ret;
    dvp_device_t *dvp;
    size_t n;
    const cam_signal_conn_t *signal_conn;
    gpio_config_t io_conf = {0};
    const dvp_pin_config_t *pin;

    if (!config || !config->size || !config->buffer_align_size ||
            !config->rx_cb || !handle || config->port > DVP_PORT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    dvp = heap_caps_calloc(1, sizeof(dvp_device_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!dvp) {
        ret = ESP_ERR_NO_MEM;
        goto errout_calloc_dvp;
    }

    pin = &config->pin;

    dvp->port = config->port;
    dvp->vsync_pin = pin->vsync_pin;

    signal_conn = &cam_periph_signals[dvp->port];

    /* Initialize DVP GPIO and its interrupt */

    io_conf.intr_type    = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = 1ull << pin->vsync_pin;
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_up_en   = 1;
    io_conf.pull_down_en = 0;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        goto errout_config_gpio;
    }

    /* Ignore result if this calling fails, maybe users call this in previous step */

    gpio_install_isr_service(ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM);

    ret = gpio_isr_handler_add(pin->vsync_pin, dvp_vsync_isr, dvp);
    if (ret != ESP_OK) {
        goto errout_config_gpio;
    }

    ret = gpio_intr_disable(pin->vsync_pin);
    if (ret != ESP_OK) {
        goto errout_disable_intr;
    }

    CONFIG_GPIO(pin->pclk_pin,  signal_conn->pclk_sig);
    CONFIG_GPIO(pin->vsync_pin, signal_conn->vsync_sig);
    for (int i = 0; i < DVP_INTF_DATA_PIN_NUM; i++) {
        CONFIG_GPIO(pin->data_pin[i], signal_conn->data_sigs[i]);
    }

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32_S3
    /* Fix connecting input 1 (0x38) to HREF signal */

    esp_rom_gpio_connect_in_signal(0x38, signal_conn->href_sig, false);
    CONFIG_GPIO(pin->href_pin,  signal_conn->hsync_sig);
#else
#if CONFIG_DVP_SUPPORT_H_SYNC
    CONFIG_GPIO(pin->hsync_pin,  signal_conn->hsync_sig);
#endif
    CONFIG_GPIO(pin->href_pin,  signal_conn->href_sig);
#endif

    /* Initialize DVP controller */

    periph_module_enable(signal_conn->module);
    cam_hal_init(&dvp->hal, dvp->port);

    /* Initialize DVP interrupt */

    ret = esp_intr_alloc(signal_conn->irq_id,
                         ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM,
                         dvp_dma_isr,
                         dvp,
                         &dvp->intr_handle);
    if (ret != ESP_OK) {
        goto errout_alloc_dma_intr;
    }

    dvp->event_queue = xQueueCreate(DVP_EVENT_QUEUE_SIZE, sizeof(dvp_int_event_t));
    if (!dvp->event_queue) {
        ret = ESP_ERR_NO_MEM;
        goto errout_create_queue;
    }

    /**
     * Calculate actually receive frame size, when DVP hardware has received half size of data,
     * DVP DMA triggers interrupt to send event "DVP_EVENT_DATA_RECVED" to DVP task, and DVP task
     * need copy half size of data from DMA receive buffer to user buffer which is added by
     * API "dvp_device_add_buffer".
     */

    dvp->item_size = cam_hal_get_sample_data_size(&dvp->hal);
    n = dvp->item_size * config->width;
    dvp->hsize = (config->size / DVP_DMA_DIV / n * n + config->buffer_align_size - 1) & (~(config->buffer_align_size - 1));
    dvp->size = dvp->hsize * DVP_DMA_DIV;

    dvp->buffer = heap_caps_aligned_alloc(config->buffer_align_size, dvp->size, MALLOC_CAP_DMA);
    if (!dvp->buffer) {
        ret = ESP_ERR_NO_MEM;
        goto errout_alloc_buffer;
    }

    dvp->lldesc_hcnt = (dvp->hsize + LLDESC_MAX_NUM_PER_DESC - 1) / LLDESC_MAX_NUM_PER_DESC;
    dvp->lldesc_cnt = dvp->lldesc_hcnt * DVP_DMA_DIV;
    dvp->lldesc = heap_caps_malloc(dvp->lldesc_cnt * sizeof(lldesc_t), MALLOC_CAP_DMA);
    if (!dvp->lldesc) {
        ret = ESP_ERR_NO_MEM;
        goto errout_malloc_dma_desc;
    } else {
        lldesc_setup_link(dvp->lldesc, dvp->buffer, dvp->hsize, true);
        lldesc_setup_link(&dvp->lldesc[dvp->lldesc_hcnt], &dvp->buffer[dvp->hsize], dvp->hsize, true);
        dvp->lldesc[dvp->lldesc_hcnt - 1].qe.stqe_next = &dvp->lldesc[dvp->lldesc_hcnt];
        dvp->lldesc[dvp->lldesc_cnt - 1].qe.stqe_next = dvp->lldesc;
    }

    dvp->mutex = xSemaphoreCreateMutex();
    if (!dvp->mutex) {
        ret = ESP_ERR_NO_MEM;
        goto errout_create_mutex;
    }

    dvp->state = DVP_DEV_IDLE;
    dvp->rx_cb = config->rx_cb;
    dvp->free_buf_cb = config->free_buf_cb;
    dvp->priv = config->priv;

    ret = xTaskCreate(dvp_task, "dvp_task", DVP_TASK_STACK_SIZE, dvp, configMAX_PRIORITIES - 1, &dvp->task_handle);
    if (ret != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto errout_create_task;
    }

    *handle = dvp;

    return ESP_OK;

errout_create_task:
    vSemaphoreDelete(dvp->mutex);
errout_create_mutex:
    heap_caps_free(dvp->lldesc);
errout_malloc_dma_desc:
    heap_caps_free(dvp->buffer);
errout_alloc_buffer:
    vQueueDelete(dvp->event_queue);
errout_create_queue:
    esp_intr_free(dvp->intr_handle);
errout_alloc_dma_intr:
    cam_hal_deinit(&dvp->hal);
    periph_module_disable(signal_conn->module);
errout_disable_intr:
    gpio_isr_handler_remove(dvp->vsync_pin);
errout_config_gpio:
    heap_caps_free(dvp);
errout_calloc_dvp:
    return ret;
}

/**
 * @brief Destroy DVP object created by "dvp_device_create", and all frame added by
 *        API "dvp_device_add_buffer" will be freed by registered callback function "free_buf_cb".
 *
 * @param dvp DVP device handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_destroy(dvp_device_handle_t handle)
{
    /* Step 1: delete DVP receive task */

    vTaskDelete(handle->task_handle);

    /* Step 2: disable and free interrupt */

    esp_intr_disable(handle->intr_handle);
    gpio_intr_disable(handle->vsync_pin);

    /* Step 3: stop and de-initialize DVP receive */

    cam_hal_stop_streaming(&handle->hal);
    cam_hal_deinit(&handle->hal);

    /* Step 4: Free interrupt */

    gpio_isr_handler_remove(handle->vsync_pin);
    esp_intr_free(handle->intr_handle);

    /* Step 5: disable and reset DVP */

    periph_module_disable(cam_periph_signals[handle->port].module);

    /* Step 6: free buffer in frame if needed */

    if (handle->free_buf_cb) {
        dvp_frame_t *frame;

        if (handle->cur_frame) {
            handle->free_buf_cb(handle->cur_frame->buffer, handle->priv);
            heap_caps_free(handle->cur_frame);
        }

        SLIST_FOREACH(frame, &handle->frame_list, node) {
            SLIST_REMOVE(&handle->frame_list, frame, dvp_frame, node);
            handle->free_buf_cb(frame->buffer, handle->priv);
            heap_caps_free(frame);
        }
    }

    /* Step 7: Free memory resocurce */

    vSemaphoreDelete(handle->mutex);
    heap_caps_free(handle->lldesc);
    heap_caps_free(handle->buffer);
    vQueueDelete(handle->event_queue);
    heap_caps_free(handle);

    return -1;
}

/**
 * @brief Start DVP capturing data from camera
 *
 * @param dvp DVP device handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_start(dvp_device_handle_t handle)
{
    esp_err_t ret = ESP_ERR_INVALID_STATE;

    ret = xSemaphoreTake(handle->mutex, portMAX_DELAY);
    if (ret != pdTRUE) {
        return ESP_FAIL;
    }

    if (handle->state == DVP_DEV_IDLE) {
        ret = dvp_start_receive(handle);
        if (ret == ESP_OK) {
            handle->state = DVP_DEV_WAIT;
        }
    }

    xSemaphoreGive(handle->mutex);

    return ret;
}

/**
 * @brief Stop DVP capturing data from camera
 *
 * @param dvp DVP device handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_stop(dvp_device_handle_t handle)
{
    esp_err_t ret = ESP_ERR_INVALID_STATE;

    ret = xSemaphoreTake(handle->mutex, portMAX_DELAY);
    if (ret != pdTRUE) {
        return ESP_FAIL;
    }

    if (handle->state != DVP_DEV_IDLE) {
        ret = gpio_intr_disable(handle->vsync_pin);
        if (ret == ESP_OK) {
            cam_hal_stop_streaming(&handle->hal);
            SLIST_INSERT_HEAD(&handle->frame_list, handle->cur_frame, node);
            handle->state = DVP_DEV_IDLE;
            handle->cur_frame = NULL;
        }
    }

    xSemaphoreGive(handle->mutex);

    return ret;
}

/**
 * @brief Add frame to DVP receive list
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
esp_err_t dvp_device_add_buffer(dvp_device_handle_t handle, uint8_t *buffer, size_t size)
{
    esp_err_t ret;
    dvp_frame_t *frame;

    frame = heap_caps_malloc(sizeof(dvp_frame_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }

    frame->buffer = buffer;
    frame->length = size;

    ret = xSemaphoreTake(handle->mutex, portMAX_DELAY);
    if (ret != pdTRUE) {
        heap_caps_free(frame);
        return ESP_FAIL;
    }

    SLIST_INSERT_HEAD(&handle->frame_list, frame, node);
    if (handle->state == DVP_DEV_BLOCK) {
        ret = dvp_start_receive(handle);
        assert(ret == ESP_OK);
        handle->state = DVP_DEV_WAIT;
    }

    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}
