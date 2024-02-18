/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/param.h>
#include "rom/cache.h"
#include "hal/gpio_ll.h"
#include "driver/gpio.h"
#include "esp_private/periph_ctrl.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "soc/cam_periph.h"
#include "dvp.h"

#define DVP_TASK_STACK_SIZE                 CONFIG_DVP_TASK_STACK_SIZE
#define DVP_DMA_DIV                         2
#define DVP_EVENT_QUEUE_SIZE                2
#define DVP_DMA_DESC_JPEG_SIZE              512

#define DVP_PORT_MAX                        (SOC_CAM_PERIPH_NUM - 1)

#define DVP_CUR_BUF(d)                      (&(d)->buffer[(d)->dma_desc_index * (d)->hsize])
#define DVP_CUR_LLDESC(d)                   (&(d)->dma_desc[(d)->dma_desc_index * (d)->dma_desc_hcnt])
#define FRAME_CUR_BUF(f)                    (&(f)->buffer[(f)->size])

#define DVP_UP_ALIGN(v, a)                  (((v) + ((a) - 1)) & (~((a) - 1)))

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
 * @brief Config DVP DMA description list
 *
 * @param dma_desc  DVP DMA description pointer
 * @param desc_size DVP DMA description node max size
 * @param buffer    DVP receive buffer pointer
 * @param size      DMA buffer size
 * @param next      DVP DMA description next pointer of this list
 *
 * @return None
 */
static void dvp_config_dma_desc(dvp_dma_desc_t *dma_desc, uint32_t desc_size, uint8_t *buffer, uint32_t size, dvp_dma_desc_t *next)
{
    int n = 0;

    while (size) {
        uint32_t dma_node_size = DVP_UP_ALIGN(MIN(size, desc_size), 4);

        dma_desc[n].dw0.size = dma_node_size;
        dma_desc[n].dw0.length = 0;
        dma_desc[n].dw0.err_eof = 0;
        dma_desc[n].dw0.suc_eof = 0;
        dma_desc[n].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        dma_desc[n].buffer = (uint8_t *)buffer;
        dma_desc[n].next = &dma_desc[n + 1];

        size -= dma_node_size;
        buffer += dma_node_size;
        n++;
    }

    dma_desc[n - 1].dw0.suc_eof = 1;
    dma_desc[n - 1].next = next;
}

/**
 * @brief Get DMA valid size in DMA description list
 *
 * @param dvp                DVP device handle
 * @param next_dma_desc_addr DVP next DMA description address
 *
 * @return DMA valid size
 */
static uint32_t dvp_get_dma_valid_size(dvp_device_t *dvp, uint32_t next_dma_desc_addr)
{
    uint32_t size;

    if ((next_dma_desc_addr == (uint32_t)&dvp->dma_desc[0]) ||
            (next_dma_desc_addr == (uint32_t)&dvp->dma_desc[dvp->dma_desc_hcnt])) {
        size = dvp->hsize;
    } else {
        uint32_t count = (next_dma_desc_addr - (uint32_t)&dvp->dma_desc[0]) / sizeof(dvp_dma_desc_t);

        if (dvp->dma_desc_index) {
            count -= dvp->dma_desc_hcnt;
        }

        /* This means hardware issue triggers, and the frame should be dropped */

        if (count > dvp->dma_desc_hcnt) {
            return 0;
        }

        size = count * dvp->dma_desc_size;
    }

    return size;
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

    ret = dvp_dma_start(&dvp->dma, dvp->dma_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start GDMA");
    }
#endif

    cam_hal_start_streaming(&dvp->hal, (uint32_t)dvp->dma_desc, dvp->hsize);

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

                /* Calculate received data size and check if frame left space is enough */

                if ((frame->size + frame_size) < frame->length) {
                    /* Decode received data and update receive state data */

                    cam_hal_memcpy(&dvp->hal, FRAME_CUR_BUF(frame), DVP_CUR_BUF(dvp), data_size);
                    frame->size += frame_size;
                    dvp->dma_desc_index = (dvp->dma_desc_index + 1) % DVP_DMA_DIV;
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
                dvp_frame_t *frame;
                dvp_rx_cb_ret_t ret;
                uint32_t next_dma_desc_addr = 0;

                if (dvp->jpeg) {
#if CONFIG_SOC_GDMA_SUPPORTED
                    next_dma_desc_addr = dvp_dma_get_next_dma_desc_addr(&dvp->dma);
#else
                    next_dma_desc_addr = cam_hal_get_next_dma_desc_addr(&dvp->hal);
#endif
                }

                /* Stop DVP receive, and then no event will be sent */

                frame = dvp->cur_frame;
                dvp->state = DVP_DEV_RXED;

                dvp_stop_capturing(dvp);
                gpio_intr_disable(dvp->vsync_pin);

                /* Mark current frame is NULL, this shows no receive is started */

                dvp->cur_frame = NULL;

                /* Get rest received data size and check if frame left space is enough */

                if (dvp->jpeg) {
                    data_size = dvp_get_dma_valid_size(dvp, next_dma_desc_addr);
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
                    dvp->dma_desc_index = 0;
                    dvp_start_capturing(dvp);
                }
            } else if (dvp->state == DVP_DEV_WAIT) {
                dvp_frame_t *frame = dvp->cur_frame;

                dvp->state = DVP_DEV_RXING;

                frame->size = 0;
                dvp->dma_desc_index = 0;
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
    const cam_signal_conn_t *signal_conn = &cam_periph_signals[port];

    DVP_CONFIG_INPUT_PIN(pin->vsync_pin, signal_conn->vsync_sig);
    ret = gpio_set_intr_type(pin->vsync_pin, GPIO_INTR_NEGEDGE);
    if (ret != ESP_OK) {
        return ret;
    }

    DVP_CONFIG_INPUT_PIN(pin->pclk_pin,  signal_conn->pclk_sig);
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
    ret = gpio_set_direction(pin->xclk_pin, GPIO_MODE_OUTPUT);
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

#ifndef CONFIG_IDF_TARGET_ESP32P4
    periph_module_enable(cam_periph_signals[port].module);
#endif

    ret = cam_hal_config_port_xclk(port, xclk_freq);
    if (ret != ESP_OK) {
#ifndef CONFIG_IDF_TARGET_ESP32P4
        periph_module_disable(cam_periph_signals[port].module);
#endif
        ESP_LOGE(TAG, "failed to config LCD_CAM xclock");
    }

    return ret;
}
#endif

/**
 * @brief Enable DVP clock. When select DVP_ENABLE_OUTPUT_CLOCK, initialize DVP device
 *        will not enable DVP clock, because the clock is enabled for sensor.
 *
 * @param port DVP port
 *
 * @return None
 */
static void dvp_enable_clock(int port)
{
    /* ESP32-P4 has not supported this feature, clock is enabled in function "cam_hal_config_port_xclk" */

#ifndef CONFIG_IDF_TARGET_ESP32P4
#ifndef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
    periph_module_enable(cam_periph_signals[port].module);
#endif
#endif
}

/**
 * @brief Disable DVP clock. When select DVP_ENABLE_OUTPUT_CLOCK, de-initialize DVP device
 *        will not disable DVP clock, because the clock should be kept enabled for sensor.
 *
 * @param port DVP port
 *
 * @return None
 */
static void dvp_disable_clock(int port)
{
    /* ESP32-P4 has not supported this feature, clock is enabled in function "cam_hal_config_port_xclk" */

#ifndef CONFIG_IDF_TARGET_ESP32P4
#ifndef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
    periph_module_disable(cam_periph_signals[port].module);
#endif
#endif
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

    dvp_enable_clock(config->port);
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
    dvp_disable_clock(config->port);
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

    /* Step 5: disable and reset DVP */

    dvp_disable_clock(handle->port);

    /* Step 6: free buffer in frame if needed */

    dev_free_frame(handle);

    /* Step 7: Free memory resocurce */

    vSemaphoreDelete(handle->mutex);
    if (handle->dma_desc) {
        heap_caps_free(handle->dma_desc);
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

    if (handle->buffer && handle->dma_desc) {
        if (handle->state == DVP_DEV_IDLE) {
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
    uint32_t dma_desc_size;
    dvp_device_t *dvp = handle;
    uint32_t buffer_align_size = cam_hal_dma_align_size(&dvp->hal);

    if (!handle || !frame_size || (frame_size % buffer_align_size)) {
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

    if (dvp->dma_desc) {
        heap_caps_free(dvp->dma_desc);
        dvp->dma_desc = NULL;
    }

    dvp->frame_size = 0;

    /**
     * Calculate actually receive frame size, when DVP hardware has received half size of data,
     * DVP DMA triggers interrupt to send event "DVP_EVENT_DATA_RECVED" to DVP task, and DVP task
     * need copy half size of data from DMA receive buffer to user buffer which is added by
     * API "dvp_device_add_buffer".
     */

    dvp->hsize = dvp_get_dma_buffer_hsize(dvp->dma_buffer_max_size, buffer_align_size, frame_size, jpeg);
    dvp->size = dvp->hsize * DVP_DMA_DIV;

    dvp->buffer = heap_caps_aligned_alloc(buffer_align_size, dvp->size, MALLOC_CAP_DMA);
    if (!dvp->buffer) {
        goto errout_malloc_buffer;
    }

    /* JPEG format uses smaller DMA description to reduce copying and decoding invalid data */

    dvp->dma_desc_size = jpeg ? DVP_DMA_DESC_JPEG_SIZE : DVP_DMA_DESC_BUFFER_MAX_SIZE;
    dvp->dma_desc_hcnt = (dvp->hsize + dvp->dma_desc_size - 1) / dvp->dma_desc_size;

    dma_desc_size = DVP_UP_ALIGN(DVP_DMA_DIV * dvp->dma_desc_hcnt * sizeof(dvp_dma_desc_t), buffer_align_size);
    dvp->dma_desc = heap_caps_aligned_alloc(buffer_align_size, dma_desc_size, MALLOC_CAP_DMA);
    if (!dvp->dma_desc) {
        goto errout_malloc_lldesc;
    } else {
        dvp_config_dma_desc(dvp->dma_desc, dvp->dma_desc_size, dvp->buffer, dvp->hsize, &dvp->dma_desc[dvp->dma_desc_hcnt]);
        dvp_config_dma_desc(&dvp->dma_desc[dvp->dma_desc_hcnt], dvp->dma_desc_size, &dvp->buffer[dvp->hsize], dvp->hsize, dvp->dma_desc);

#if CONFIG_IDF_TARGET_ESP32P4
        /* Flush data from cache to RAM so that DMA engine can get this configuration */

        Cache_WriteBack_Addr(CACHE_MAP_L2_CACHE | CACHE_MAP_L1_DCACHE, (uint32_t)dvp->dma_desc, dma_desc_size);
#endif
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
