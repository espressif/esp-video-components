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

#define DVP_CONFIG_INPUT_PIN(pin, sig)                      \
{                                                           \
    ret = dvp_config_input_gpio(pin, sig);                  \
    if (ret != ESP_OK) {                                    \
        ESP_LOGE(TAG, "failed to configure pin=%d sig=%d",  \
                 pin, sig);                                 \
        return ret;                                         \
    }                                                       \
}

static const char *TAG = "dvp";

/**
 * @brief Calculate DMA buffer half size
 *
 * @param buffer_size Raw DMA buffer size
 * @param align_size  DMA buffer align size
 * @param frame_size  Frame buffer maximum size
 * @param jpeg        Frame data format is JPEG
 *
 * @return Actual DMA buffer half size.
 */
static uint32_t dvp_get_dma_buffer_hsize(uint32_t buffer_size, uint32_t align_size, uint32_t frame_size, bool jpeg)
{
    uint32_t hsize = (buffer_size / DVP_DMA_DIV + align_size - 1) & (~(align_size - 1));

    if (!jpeg) {
        while ((frame_size % hsize) != 0) {
            hsize -= align_size;
        }
    }

    return hsize;
}

/**
 * @brief Free all DVP frames
 *
 * @param dvp DVP object data pointer
 *
 * @return None
 */
static void dev_free_frame(dvp_device_t *dvp)
{
    dvp_frame_t *frame, *node_tmp;

    if (dvp->cur_frame) {
        dvp->free_buf_cb(dvp->cur_frame->buffer, dvp->priv);
        heap_caps_free(dvp->cur_frame);
        dvp->cur_frame = NULL;
    }

    SLIST_FOREACH_SAFE(frame, &dvp->frame_list, node, node_tmp) {
        SLIST_REMOVE(&dvp->frame_list, frame, dvp_frame, node);
        dvp->free_buf_cb(frame->buffer, dvp->priv);
        heap_caps_free(frame);
    }
}

/**
 * @brief Check JPEG file and return JPEG frame actual size
 *
 * @param buffer JPEG buffer pointer
 * @param size   JPEG buffer size
 *
 * @return JPEG frame actual size if success or 0 if failed
 */
static uint32_t esp_dvp_calculate_jpeg_size(const uint8_t *buffer, uint32_t size)
{
    /* Check JPEG header TAG: ff:d8 */

    if (buffer[0] != 0xff || buffer[1] != 0xd8) {
        return 0;
    }

    for (uint32_t off = size - 2; off > 0; off--) {
        /* Check JPEG tail TAG: ff:d9 */

        if (buffer[off] == 0xff && buffer[off + 1] == 0xd9) {
            return off + 2;
        }
    }

    return 0;
}

/**
 * @brief Clear DMA receive data size
 *
 * @param dvp DVP object data pointer
 *
 * @return None
 */
static void dvp_clear_dma_recv_size(dvp_device_handle_t dvp)
{
    lldesc_t *lldesc = DVP_CUR_LLDESC(dvp);

    for (int i = 0; i < dvp->lldesc_hcnt; i++) {
        lldesc[i].length = 0;
    }
}

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

    /* Although lldesc.length is equal to 0, but this DMA node can also have received data */

    return size + LLDESC_MAX_NUM_PER_DESC;
}

/**
 * @brief Start DVP hardware capturing stream
 *
 * @param dvp DVP device handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_start_capturing(dvp_device_t *dvp)
{
    esp_err_t ret = ESP_OK;

#if CONFIG_SOC_GDMA_SUPPORTED
    ret = dvp_dma_reset(&dvp->dma);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to reset GDMA");
    }

    ret = dvp_dma_start(&dvp->dma, (dma_descriptor_t *)dvp->lldesc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start GDMA");
    }
#endif
    cam_hal_start_streaming(&dvp->hal, dvp->lldesc, dvp->hsize);

    return ret;
}

/**
 * @brief Stop DVP hardware capturing stream
 *
 * @param dvp DVP device handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_stop_capturing(dvp_device_t *dvp)
{
    esp_err_t ret = ESP_OK;

    cam_hal_stop_streaming(&dvp->hal);
#if CONFIG_SOC_GDMA_SUPPORTED
    ret = dvp_dma_stop(&dvp->dma);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to stop GDMA");
    }
#endif

    return ret;
}

/**
 * @brief Start DVP receiving V-SYNC signal from camera sensor
 *
 * @param dvp DVP object data pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_start_receiving_vsync(dvp_device_t *dvp)
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

    /* Start capturing stream when receiving V-SYNC start  */

    event.type = DVP_EVENT_VSYNC_END;

    ret = xQueueSendFromISR(dvp->event_queue, &event, &need_switch);
    if (ret == pdPASS) {
        if (need_switch == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    } else {
        ESP_EARLY_LOGE(TAG, "failed to send vsync event type=%d", event.type);
    }
}

#if CONFIG_SOC_GDMA_SUPPORTED
/**
 * @brief DVP receive data interrupt callback function
 *
 * @param dma_chan   DMA channel handle
 * @param event_data Event data pointer
 * @param user_data  This pointer is DVP object data pointer
 *
 * @return true if success or false if failed.
 */
static IRAM_ATTR bool dvp_receive_isr(gdma_channel_handle_t dma_chan, gdma_event_data_t *event_data, void *user_data)
{
    BaseType_t ret;
    dvp_int_event_t event;
    dvp_device_t *dvp = (dvp_device_t *)user_data;

    event.type = DVP_EVENT_DATA_RECVED;
    ret = xQueueSendFromISR(dvp->event_queue, &event, NULL);
    if (ret != pdPASS) {
        ESP_EARLY_LOGE(TAG, "failed to send data received event");
    }

    return ret == pdPASS;
}
#else
/**
 * @brief DVP receive data interrupt callback function
 *
 * @param arg This pointer is DVP object data pointer
 *
 * @return None
 */
static IRAM_ATTR void dvp_receive_isr(void *arg)
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
#endif
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
        case DVP_EVENT_DATA_RECVED: {
            if (dvp->state == DVP_DEV_RXING) {
                /* DVP triggers this event only when completing receiving half size of DMA buffer */

                size_t data_size = dvp->hsize;
                size_t frame_size = data_size / dvp->item_size;
                dvp_frame_t *frame = dvp->cur_frame;

                /* Clear DMA description length field if frame format is JPEG */

                if (dvp->jpeg) {
                    dvp_clear_dma_recv_size(dvp);
                }

                /* Calculate received data size and check if frame left space is enough */

                if ((frame->size + frame_size) < frame->length) {
                    /* Decode received data and update receive state data */

                    cam_hal_memcpy(&dvp->hal, FRAME_CUR_BUF(frame), DVP_CUR_BUF(dvp), data_size);
                    frame->size += frame_size;
                    dvp->lldesc_index = (dvp->lldesc_index + 1) % DVP_DMA_DIV;
                } else if ((frame->size + frame_size) == frame->length) {
                    /* Skip this event and let next "DVP_EVENT_VSYNC_END" event process this */
                } else {
                    /* Call receive function with overflow error code */

                    dvp->rx_cb(DVP_RX_OVERFLOW, frame->buffer, frame->size, dvp->priv);

                    /* Stop receiving data and reset DVP state */

                    dvp_stop_capturing(dvp);
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

                dvp_stop_capturing(dvp);
                gpio_intr_disable(dvp->vsync_pin);

                /* Mark current frame is NULL, this shows no receive is started */

                dvp->cur_frame = NULL;

                /* Get rest received data size and check if frame left space is enough */

                if (dvp->jpeg) {
                    data_size = get_and_clear_dma_recv_size(dvp);
                } else {
                    data_size = dvp->hsize;
                }

                frame_size = data_size / dvp->item_size;

                /* JPEG format frame size is random value, and it may not fill all buffer space */

                if (dvp->jpeg && ((frame->size + frame_size) > frame->length)) {
                    frame_size = frame->length - frame->size;
                }

                if ((frame->size + frame_size) <= frame->length) {
                    /* Decode received data and update receive state data */

                    cam_hal_memcpy(&dvp->hal, FRAME_CUR_BUF(frame), DVP_CUR_BUF(dvp), data_size);
                    frame->size += frame_size;

                    if (dvp->jpeg) {
                        uint32_t jpeg_size = esp_dvp_calculate_jpeg_size(frame->buffer, frame->size);

                        if (jpeg_size) {
                            frame->size = jpeg_size;
                            ret = dvp->rx_cb(DVP_RX_SUCCESS, frame->buffer, frame->size, dvp->priv);
                        } else {
                            ret = dvp->rx_cb(DVP_RX_DATALOST, frame->buffer, frame->size, dvp->priv);
                        }
                    } else {
                        ret = dvp->rx_cb(DVP_RX_SUCCESS, frame->buffer, frame->size, dvp->priv);
                    }
                } else {
                    /* Call receive function with overflow error code */

                    ret = dvp->rx_cb(DVP_RX_OVERFLOW, frame->buffer, frame->size, dvp->priv);
                }

                if (ret == DVP_RX_CB_DONE) {
                    /* Insert frame to list, and the frame can be used again */


                    SLIST_INSERT_HEAD(&dvp->frame_list, frame, node);

                } else if (ret == DVP_RX_CB_CACHED) {
                    /* Free the frame */

                    heap_caps_free(frame);
                }

                /* Start DVP receive, this is successful only when there is frame in receive list */

                if (dvp_start_receiving_vsync(dvp) != ESP_OK) {
                    dvp->state = DVP_DEV_BLOCK;
                } else {
                    dvp->state = DVP_DEV_RXING;

                    /* Reset receive state data and start DVP receive */

                    dvp->cur_frame->size = 0;
                    dvp->lldesc_index = 0;
                    dvp_start_capturing(dvp);
                }
            } else if (dvp->state == DVP_DEV_WAIT) {
                dvp_frame_t *frame = dvp->cur_frame;

                dvp->state = DVP_DEV_RXING;

                frame->size = 0;
                dvp->lldesc_index = 0;
                dvp_start_capturing(dvp);
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
 * @brief Initialzie DVP input GPIO pin.
 *
 * @param pin    DVP pin number
 * @param signal DVP pin mapping signal
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_config_input_gpio(int pin, int signal)
{
    esp_err_t ret;

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
    ret = gpio_set_direction(pin, GPIO_MODE_INPUT);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = gpio_set_pull_mode(pin, GPIO_FLOATING);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_rom_gpio_connect_in_signal(pin, signal, false);

    return ESP_OK;
}

/**
 * @brief Initialzie DVP GPIO.
 *
 * @param port DVP port
 * @param pin  DVP pin configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_device_init_gpio(uint8_t port, const dvp_pin_config_t *pin)
{
    esp_err_t ret;
    gpio_config_t io_conf = {0};
    const cam_signal_conn_t *signal_conn = &cam_periph_signals[port];

    io_conf.intr_type    = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = 1ull << pin->vsync_pin;
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_up_en   = 1;
    io_conf.pull_down_en = 0;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    DVP_CONFIG_INPUT_PIN(pin->pclk_pin,  signal_conn->pclk_sig);
    DVP_CONFIG_INPUT_PIN(pin->vsync_pin, signal_conn->vsync_sig);
    for (int i = 0; i < DVP_INTF_DATA_PIN_NUM; i++) {
        DVP_CONFIG_INPUT_PIN(pin->data_pin[i], signal_conn->data_sigs[i]);
    }

#if CONFIG_SOC_LCDCAM_SUPPORTED
#if CONFIG_DVP_SUPPORT_H_SYNC
    DVP_CONFIG_INPUT_PIN(pin->hsync_pin,  signal_conn->hsync_sig);
#endif
    DVP_CONFIG_INPUT_PIN(pin->href_pin,  signal_conn->href_sig);
#else

    /* Fix connecting input 1 (0x38) to HREF signal */

    esp_rom_gpio_connect_in_signal(0x38, signal_conn->href_sig, false);
    DVP_CONFIG_INPUT_PIN(pin->href_pin,  signal_conn->hsync_sig);
#endif

#ifdef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1ull << pin->xclk_pin;
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en   = 0;
    io_conf.pull_down_en = 0;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_rom_gpio_connect_out_signal(pin->xclk_pin, signal_conn->clk_sig, false, false);
#endif

    return ESP_OK;
}

/**
 * @brief If target platform is ESP32-S3 or ESP32-P4, initialize LCD_CAM clock.
 *
 * @param port      DVP port
 * @param xclk_freq DVP output clock frequency in HZ
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#ifdef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
esp_err_t dvp_device_init_ouput_clock(uint8_t port, uint32_t xclk_freq)
{
    esp_err_t ret;

    periph_module_enable(cam_periph_signals[port].module);

    ret = cam_hal_config_port_xclk(port, xclk_freq);
    if (ret != ESP_OK) {
        periph_module_disable(cam_periph_signals[port].module);
        ESP_LOGE(TAG, "failed to config LCD_CAM xclock");
    }

    return ret;
}
#endif

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

    if (!config || !config->dma_buffer_max_size || !config->rx_cb || !config->free_buf_cb ||
            !handle || config->port > DVP_PORT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    dvp = heap_caps_calloc(1, sizeof(dvp_device_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!dvp) {
        ret = ESP_ERR_NO_MEM;
        goto errout_calloc_dvp;
    }

    dvp->port = config->port;
    dvp->vsync_pin = config->pin.vsync_pin;

    /* Ignore result if this calling fails, maybe users call this in previous step */

    gpio_install_isr_service(ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM);

    /* Initialize DVP controller */

#ifndef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
    periph_module_enable(cam_periph_signals[config->port].module);
#endif

    cam_hal_init(&dvp->hal, dvp->port);

    /* Initialize DVP V-SYNC interrupt */

    ret = gpio_isr_handler_add(dvp->vsync_pin, dvp_vsync_isr, dvp);
    if (ret != ESP_OK) {
        goto errout_add_gpioisr;
    }

    ret = gpio_intr_disable(dvp->vsync_pin);
    if (ret != ESP_OK) {
        goto errout_disable_intr;
    }

    /* Initialize DVP receive interrupt */

#if CONFIG_SOC_GDMA_SUPPORTED
    /* GDMA driver has special API to register DMA receive callback function */

    ret = dvp_dma_init(&dvp->dma);
    if (ret != ESP_OK) {
        goto errout_disable_intr;
    }

    gdma_rx_event_callbacks_t cbs = {
        .on_recv_eof = dvp_receive_isr
    };

    ret = gdma_register_rx_event_callbacks(dvp->dma.gdma_chan, &cbs, dvp);
    if (ret != ESP_OK) {
        dvp_dma_deinit(&dvp->dma);
        goto errout_disable_intr;
    }
#else
    ret = esp_intr_alloc(cam_periph_signals[config->port].irq_id,
                         ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM,
                         dvp_receive_isr,
                         dvp,
                         &dvp->intr_handle);
    if (ret != ESP_OK) {
        goto errout_disable_intr;
    }
#endif

    dvp->event_queue = xQueueCreate(DVP_EVENT_QUEUE_SIZE, sizeof(dvp_int_event_t));
    if (!dvp->event_queue) {
        ret = ESP_ERR_NO_MEM;
        goto errout_create_queue;
    }

    dvp->mutex = xSemaphoreCreateMutex();
    if (!dvp->mutex) {
        ret = ESP_ERR_NO_MEM;
        goto errout_create_mutex;
    }

    dvp->item_size = cam_hal_get_sample_data_size(&dvp->hal);
    dvp->dma_buffer_max_size = config->dma_buffer_max_size;
    dvp->rx_cb = config->rx_cb;
    dvp->free_buf_cb = config->free_buf_cb;
    dvp->priv = config->priv;
    dvp->state = DVP_DEV_IDLE;

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
    vQueueDelete(dvp->event_queue);
errout_create_queue:
#if CONFIG_SOC_GDMA_SUPPORTED
    dvp_dma_deinit(&dvp->dma);
#else
    esp_intr_free(dvp->intr_handle);
#endif
errout_disable_intr:
    gpio_isr_handler_remove(dvp->vsync_pin);
errout_add_gpioisr:
    cam_hal_deinit(&dvp->hal);
#ifndef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
    periph_module_disable(cam_periph_signals[config->port].module);
#endif
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

#ifndef CONFIG_SOC_GDMA_SUPPORTED
    esp_intr_disable(handle->intr_handle);
#endif
    gpio_intr_disable(handle->vsync_pin);

    /* Step 3: stop and de-initialize DVP receive */

    dvp_stop_capturing(handle);
    cam_hal_deinit(&handle->hal);

    /* Step 4: Free interrupt */

    gpio_isr_handler_remove(handle->vsync_pin);
#if CONFIG_SOC_GDMA_SUPPORTED
    if (dvp_dma_deinit(&handle->dma) != ESP_OK) {
        ESP_LOGE(TAG, "failed to deinit GDMA");
    }
#else
    esp_intr_free(handle->intr_handle);
#endif

#ifndef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
    /* Step 5: disable and reset DVP */

    periph_module_disable(cam_periph_signals[handle->port].module);
#endif

    /* Step 6: free buffer in frame if needed */

    dev_free_frame(handle);

    /* Step 7: Free memory resocurce */

    vSemaphoreDelete(handle->mutex);
    if (handle->lldesc) {
        heap_caps_free(handle->lldesc);
    }
    if (handle->buffer) {
        heap_caps_free(handle->buffer);
    }
    vQueueDelete(handle->event_queue);
    heap_caps_free(handle);

    return 0;
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

    if (handle->buffer && handle->lldesc) {
        if (handle->state == DVP_DEV_IDLE) {
            /* Clear DMA description receive length filed */

            for (int i = 0; i < handle->lldesc_cnt; i++) {
                handle->lldesc[i].length = 0;
            }

            ret = dvp_start_receiving_vsync(handle);
            if (ret == ESP_OK) {
                handle->state = DVP_DEV_WAIT;
            }
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
        gpio_intr_disable(handle->vsync_pin);
        dvp_stop_capturing(handle);
        dev_free_frame(handle);
        handle->state = DVP_DEV_IDLE;
        ret = ESP_OK;
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
        ret = dvp_start_receiving_vsync(handle);
        assert(ret == ESP_OK);
        handle->state = DVP_DEV_WAIT;
    }

    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Setup DMA receive buffer by given parameters.
 *
 * @param handle     DVP device handle
 * @param frame_size Frame size
 * @param jpeg       Frame data format is JPEG
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_setup_dma_receive_buffer(dvp_device_handle_t handle, uint32_t frame_size, bool jpeg)
{
    esp_err_t ret;
    uint32_t buffer_align_size;
    dvp_device_t *dvp = handle;

    if (!handle || !frame_size) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = xSemaphoreTake(dvp->mutex, portMAX_DELAY);
    if (ret != pdTRUE) {
        return ESP_FAIL;
    }

    if (dvp->frame_size == frame_size) {
        xSemaphoreGive(dvp->mutex);
        return ESP_OK;
    }

    /* Clear DMA buffer and configuration */

    if (dvp->buffer) {
        heap_caps_free(dvp->buffer);
        dvp->buffer = NULL;
    }

    if (dvp->lldesc) {
        heap_caps_free(dvp->lldesc);
        dvp->lldesc = NULL;
    }

    dvp->frame_size = 0;

    /**
     * Calculate actually receive frame size, when DVP hardware has received half size of data,
     * DVP DMA triggers interrupt to send event "DVP_EVENT_DATA_RECVED" to DVP task, and DVP task
     * need copy half size of data from DMA receive buffer to user buffer which is added by
     * API "dvp_device_add_buffer".
     */

    buffer_align_size = cam_hal_dma_align_size(&dvp->hal);
    dvp->hsize = dvp_get_dma_buffer_hsize(dvp->dma_buffer_max_size, buffer_align_size, frame_size, jpeg);
    dvp->size = dvp->hsize * DVP_DMA_DIV;

    dvp->buffer = heap_caps_aligned_alloc(buffer_align_size, dvp->size, MALLOC_CAP_DMA);
    if (!dvp->buffer) {
        goto errout_malloc_buffer;
    }

    dvp->lldesc_hcnt = (dvp->hsize + LLDESC_MAX_NUM_PER_DESC - 1) / LLDESC_MAX_NUM_PER_DESC;
    dvp->lldesc_cnt = dvp->lldesc_hcnt * DVP_DMA_DIV;
    dvp->lldesc = heap_caps_malloc(dvp->lldesc_cnt * sizeof(lldesc_t), MALLOC_CAP_DMA);
    if (!dvp->lldesc) {
        goto errout_malloc_lldesc;
    } else {
        lldesc_setup_link(dvp->lldesc, dvp->buffer, dvp->hsize, true);
        lldesc_setup_link(&dvp->lldesc[dvp->lldesc_hcnt], &dvp->buffer[dvp->hsize], dvp->hsize, true);
        dvp->lldesc[dvp->lldesc_hcnt - 1].qe.stqe_next = &dvp->lldesc[dvp->lldesc_hcnt];
        dvp->lldesc[dvp->lldesc_cnt - 1].qe.stqe_next = dvp->lldesc;
    }

    dvp->frame_size = frame_size;
    dvp->jpeg = jpeg;

    xSemaphoreGive(dvp->mutex);

    return ESP_OK;

errout_malloc_lldesc:
    heap_caps_free(dvp->buffer);
    dvp->buffer = NULL;
errout_malloc_buffer:
    xSemaphoreGive(dvp->mutex);
    return ESP_ERR_NO_MEM;
}

/**
 * @brief Get DVP frame buffer information.
 *
 * @param handle            DVP device handle
 * @param buffer_size       Frame buffer size pointer
 * @param buffer_align_size Frame buffer address align size pointer
 * @param buffer_caps       Frame buffer capbility pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t dvp_get_frame_buffer_info(dvp_device_handle_t handle, uint32_t *buffer_size, uint32_t *buffer_align_size, uint32_t *buffer_caps)
{
    if (!handle || !buffer_size || !buffer_align_size || !buffer_caps) {
        return ESP_ERR_INVALID_ARG;
    }

    *buffer_size = handle->frame_size;
    *buffer_align_size = 1;
    *buffer_caps = MALLOC_CAP_8BIT;

    return ESP_OK;
}
