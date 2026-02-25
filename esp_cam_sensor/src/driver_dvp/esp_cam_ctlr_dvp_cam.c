/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_cam_ctlr_dvp_ext.h"

#if ESP_CAM_CTRL_DVP_ENABLE

#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_cam_ctlr_types.h"
#include "esp_cam_ctlr_interface.h"
#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "hal/cam_ll.h"
#include "hal/color_hal.h"
#include "hal/cam_hal.h"
#include "hal/misc.h"
#include "hal/dma_types.h"
#include "soc/lcd_cam_reg.h"
#include "soc/lcd_cam_struct.h"
#include "soc/gdma_struct.h"
#include "soc/gpio_sig_map.h"
#include "esp_private/gdma.h"

#define LCD_CAM_PERIPH_NUM                  (1)

#define DVP_CAM_TASK_STACK_SIZE             (CONFIG_CAM_CTRL_DVP_TASK_STACK_SIZE)
#define DVP_CAM_TASK_PRIORITY               (configMAX_PRIORITIES - 2)
#define DVP_CAM_TASK_NAME                   "dvp_task"
#define DVP_CAM_EVENT_QUEUE_SIZE            (3)

#define DVP_CAM_BUFFER_COUNT                (2)

#define DVP_CAM_JPEG_DMA_DESC_SIZE          (512)

#define DVP_CAM_DMA_SRAM_TRANS_BURST_SIZE   (32)

#define DVP_CAM_UP_ALIGN(v, a)              (((v) + ((a) - 1)) & (~((a) - 1)))
#define DVP_CAM_DOWN_ALIGN(v, a)            ((v) & (~((a) - 1)))

#define DVP_CAM_CUR_BUF(d)                  (&((d)->dma_buffer[(d)->dma_desc_index * (d)->dma_buffer_hsize]))
#define DVP_CAM_CUR_LLDESC(d)               (&((d)->dma_desc[(d)->dma_desc_index * (d)->dma_desc_hcnt]))

#define DVP_CAM_DMA_DESC_BUFFER_SIZE        DVP_CAM_DOWN_ALIGN(DMA_DESCRIPTOR_BUFFER_MAX_SIZE, 4)

#define DVP_CAM_DMA_BUFFER_SIZE             (CONFIG_CAM_CTRL_DVP_DMA_BUFFER_SIZE)

#define DVP_CAM_BUS_IO_NUM                  (8)

#if CONFIG_CAM_CTRL_DVP_LOG_ENABLE
/**
 * Use "printf" and "esp_rom_printf" to print log, this is faster than "ESP_LOG",
 * so that the log's effect to the performance of the camera receive process will be decreased.
 */
#define DVP_CAM_ERROR(fmt, ...)             printf("E:" fmt "\n", ##__VA_ARGS__)
#define DVP_CAM_INFO(fmt, ...)              printf("I:" fmt "\n", ##__VA_ARGS__)
#define DVP_CAM_ERROR_ISR(fmt, ...)         esp_rom_printf("E:" fmt "\n", ##__VA_ARGS__)
#define DVP_CAM_INFO_ISR(fmt, ...)          esp_rom_printf("I:" fmt "\n", ##__VA_ARGS__)
#else
#define DVP_CAM_ERROR(fmt, ...)
#define DVP_CAM_INFO(fmt, ...)
#define DVP_CAM_ERROR_ISR(fmt, ...)
#define DVP_CAM_INFO_ISR(fmt, ...)
#endif

/**
 * @brief DVP CAM event type
 */
typedef enum dvp_cam_event_type {
    DVP_CAM_EVENT_SYNC_END = 0,            /*!< DVP has received V-Sync end signal, and DVP has received one complete frame */
    DVP_CAM_EVENT_RECV_DATA = 1,           /*!< DVP has received a block of data, but not completed one frame */
} dvp_cam_event_type_t;

/**
 * @brief DVP CAM finite rx_state machine
 */
typedef enum dvp_cam_fsm {
    DVP_CAM_FSM_INIT = 1,                  /*!< DVP CAM initialization rx_state, and next rx_state is "enabled" */
    DVP_CAM_FSM_STARTED,                   /*!< DVP CAM started rx_state, and next rx_state is "init" or "enabled" */
    DVP_CAM_FSM_RXING,
} dvp_cam_fsm_t;

/**
 * @brief DVP CAM event
 */
typedef struct dvp_cam_event {
    dvp_cam_event_type_t type;
} dvp_cam_event_t;

/**
 * @brief DVP device object data
 */
typedef struct dvp_cam_ctlr {
    esp_cam_ctlr_t base;                                /*!< Camera controller base object data */
    esp_cam_ctlr_evt_cbs_t cbs;                         /*!< Camera controller callback functions */
    void *cbs_user_data;                                /*!< Camera controller callback private data */

    gpio_num_t vsync_pin;                               /*!< DVP V-Sync pin number */
    gdma_channel_handle_t dma_chan;                     /*!< DVP DMA channel handle */
    int dma_chan_id;                                    /*!< DVP DMA channel ID */
    cam_hal_context_t hal;                              /*!< DVP CAM hardware interface object data */

    dvp_cam_fsm_t dvp_fsm;                              /*!< DVP CAM finite rx_state machine */

    portMUX_TYPE spinlock;                              /*!< DVP CAM spinlock */
    QueueHandle_t event_queue;                          /*!< DVP event queue */
    TaskHandle_t task_handle;                           /*!< DVP task handle */

    esp_cam_ctlr_trans_t trans;                         /*!< DVP transaction */

    uint8_t *dma_buffer;                                /*!< DVP cache buffer */
    size_t dma_buffer_size;                             /*!< DVP cache buffer size */
    size_t dma_buffer_hsize;                            /*!< DVP cache buffer half size */

    dma_descriptor_t *dma_desc;                         /*!< DVP cache buffer DMA description */
    size_t dma_desc_size;                               /*!< DVP cache buffer DMA description receive data size per node */
    size_t dma_desc_hcnt;                               /*!< DVP cache buffer DMA description half count */
    size_t dma_desc_index;                              /*!< DVP cache buffer DMA description index */

    size_t fb_size_in_bytes;                            /*!< DVP frame buffer size in bytes */

    struct {
        uint32_t pic_format_jpeg : 1;                   /*!< Input picture format is JPEG, if set this flag and "input_data_color_type" will be ignored */
    };
} dvp_cam_ctlr_t;

static const char *TAG = "dvp_ext";
static gpio_num_t s_vsync_io = GPIO_NUM_NC;

/**
 * @brief Extended function based on "cam_hal_init", plus setting the cam_vs_eof_en field to be 0.
 *
 * @param hal    CAM object data pointer
 * @param config CAM configuration
 */
static void cam_hal_init_ext(cam_hal_context_t *hal, const cam_hal_config_t *config)
{
    cam_hal_init(hal, config);
    hal->hw->cam_ctrl.cam_vs_eof_en = 0;
}

/**
 * @brief Extended function based on "cam_hal_start_streaming", plus setting the cam_rec_data_bytelen field to be size - 1.
 *
 * @param hal    CAM object data pointer
 * @param config CAM configuration
 */
static void cam_hal_start_streaming_ext(cam_hal_context_t *hal, size_t size)
{
    cam_ll_reset(hal->hw);
    cam_ll_fifo_reset(hal->hw);

    HAL_FORCE_MODIFY_U32_REG_FIELD(hal->hw->cam_ctrl1, cam_rec_data_bytelen, size - 1);

    cam_ll_start(hal->hw);
}

/**
 * @brief Deinitialize DVP DMA
 *
 * @param gdma_chan DVP DMA channel handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
*/
static esp_err_t dvp_dma_deinit(gdma_channel_handle_t gdma_chan)
{
    if (gdma_chan) {
        ESP_RETURN_ON_ERROR(gdma_disconnect(gdma_chan), TAG, "disconnect dma channel failed");
        ESP_RETURN_ON_ERROR(gdma_del_channel(gdma_chan), TAG, "delete dma channel failed");
    }

    return ESP_OK;
}

/**
 * @brief Initialize DVP DMA
 *
 * @param gdma_chan DVP DMA channel handle pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_dma_init(gdma_channel_handle_t *gdma_chan)
{
    esp_err_t ret = ESP_OK;
    gdma_channel_alloc_config_t rx_alloc_config = {0};

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
    rx_alloc_config.direction = GDMA_CHANNEL_DIRECTION_RX;

    ESP_RETURN_ON_ERROR(gdma_new_ahb_channel(&rx_alloc_config, gdma_chan), TAG, "new DVP DMA channel failed");
#else /* (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)) */
    ESP_RETURN_ON_ERROR(gdma_new_ahb_channel(&rx_alloc_config, NULL, gdma_chan), TAG, "new DVP DMA channel failed");
#endif /* (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)) */

    ESP_GOTO_ON_ERROR(gdma_connect(*gdma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_CAM, 0)), fail0, TAG, "connect failed");

    gdma_strategy_config_t strategy_config = {
        .auto_update_desc = false,
        .owner_check = false
    };
    ESP_GOTO_ON_ERROR(gdma_apply_strategy(*gdma_chan, &strategy_config), fail0, TAG, "apply strategy failed");

    gdma_transfer_config_t ability = {
        .max_data_burst_size = DVP_CAM_DMA_SRAM_TRANS_BURST_SIZE,
        .access_ext_mem = false,
    };
    ESP_GOTO_ON_ERROR(gdma_config_transfer(*gdma_chan, &ability), fail0, TAG, "set trans ability failed");

    return ESP_OK;

fail0:
    dvp_dma_deinit(*gdma_chan);
    return ret;
}

/**
 * @brief Get next DMA description address
 *
 * @param dvp                DVP device handle
 *
 * @return Next DMA description address
 */
static uint32_t get_next_dma_desc_addr(dvp_cam_ctlr_t *dvp)
{
    return GDMA.channel[dvp->dma_chan_id].in.dscr_bf0;
}

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
    uint32_t hsize = (buffer_size / DVP_CAM_BUFFER_COUNT + align_size - 1) & (~(align_size - 1));

    if (!jpeg) {
        while (((frame_size % hsize) != 0) && (hsize > align_size)) {
            hsize -= align_size;
        }
    }

    if (hsize < align_size) {
        return 0;
    }

    return hsize;
}

/**
 * @brief Check JPEG file and return JPEG frame actual size
 *
 * @param buffer JPEG buffer pointer
 * @param size   JPEG buffer size
 *
 * @return JPEG frame actual size if success or 0 if failed
 */
static uint32_t dvp_calculate_jpeg_size(const uint8_t *buffer, uint32_t size)
{
    if (size < 16) {
        DVP_CAM_ERROR("JPEG size");
        return 0;
    }

    /* Check JPEG header TAG: ff:d8 */

    if (buffer[0] != 0xff || buffer[1] != 0xd8) {
        DVP_CAM_ERROR("NO-SOI");
        return 0;
    }

    for (uint32_t off = size - 2; off > 0; off--) {
        /* Check JPEG tail TAG: ff:d9 */

        if (buffer[off] == 0xff && buffer[off + 1] == 0xd9) {
            return off + 2;
        }
    }

    DVP_CAM_ERROR("NO-EOI");

    return 0;
}

/**
 * @brief CAM DVP calculate frame receive buffer size.
 *
 * @param config  CAM DVP controller configurations
 * @param p_size  CAM DVP frame receive buffer size buffer pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_get_frame_size(const esp_cam_ctlr_dvp_config_t *config, size_t *p_size)
{
    esp_err_t ret = ESP_OK;

    if (config->pic_format_jpeg) {
        *p_size = config->h_res * config->v_res;
    } else {
        color_space_pixel_format_t pixel_format = {
            .color_type_id = config->input_data_color_type
        };
        uint32_t depth = color_hal_pixel_format_get_bit_depth(pixel_format);
        if (!depth) {
            return ESP_ERR_INVALID_ARG;
        }

        *p_size = config->h_res * config->v_res * depth / 8;
    }

    return ret;
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
static void dvp_config_dma_desc(dma_descriptor_t *dma_desc, uint32_t desc_size, uint8_t *buffer, uint32_t size, dma_descriptor_t *next)
{
    int n = 0;

    ESP_LOGD(TAG, "dma_desc=%p, desc_size=%d, buffer=%p, size=%d, next=%p", dma_desc, (int)desc_size, buffer, (int)size, next);

    while (size) {
        uint32_t dma_node_size = DVP_CAM_DOWN_ALIGN(MIN(size, desc_size), 4);

        dma_desc[n].dw0.size = dma_node_size;
        dma_desc[n].dw0.length = 0;
        dma_desc[n].dw0.err_eof = 0;
        dma_desc[n].dw0.suc_eof = 0;
        dma_desc[n].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        dma_desc[n].buffer = buffer;
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
static uint32_t dvp_get_dma_valid_size(dvp_cam_ctlr_t *dvp, uint32_t next_dma_desc_addr)
{
    uint32_t size;

    if ((next_dma_desc_addr == (uint32_t)&dvp->dma_desc[0]) ||
            (next_dma_desc_addr == (uint32_t)&dvp->dma_desc[dvp->dma_desc_hcnt])) {
        size = dvp->dma_buffer_hsize;
    } else {
        uint32_t count = (next_dma_desc_addr - (uint32_t)&dvp->dma_desc[0]) / sizeof(dma_descriptor_t);

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
 * @brief DVP receive V-Sync interrupt interrupt callback function
 *
 * @param arg This pointer is DVP object data pointer
 *
 * @return None
 */
static IRAM_ATTR void dvp_vsync_isr(void *arg)
{
    BaseType_t ret;
    dvp_cam_event_t event;
    BaseType_t need_switch = pdFALSE;
    dvp_cam_ctlr_t *dvp = (dvp_cam_ctlr_t *)arg;

    /* Start capturing stream when receiving V-SYNC start  */

    event.type = DVP_CAM_EVENT_SYNC_END;

    ret = xQueueSendFromISR(dvp->event_queue, &event, &need_switch);
    if (ret == pdPASS) {
        if (need_switch == pdTRUE) {
            portYIELD_FROM_ISR();
        }

        /**
         * The VS-ISR is called too frequently, and it will cause the log to be flooded.
         * So we disable the log for now, open it if you want to trace the data flow.
         */
        // DVP_CAM_INFO_ISR("VS-ISR");
    } else {
        DVP_CAM_ERROR_ISR("VS-ISR OVF");
    }
}

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
    dvp_cam_event_t event;
    BaseType_t need_switch = pdFALSE;
    dvp_cam_ctlr_t *dvp = (dvp_cam_ctlr_t *)user_data;

    event.type = DVP_CAM_EVENT_RECV_DATA;
    ret = xQueueSendFromISR(dvp->event_queue, &event, &need_switch);
    if (ret == pdPASS) {
        if (need_switch == pdTRUE) {
            portYIELD_FROM_ISR();
        }

        /**
         * The D-ISR is called too frequently, and it will cause the log to be flooded.
         * So we disable the log for now, open it if you want to trace the data flow.
         */
        // DVP_CAM_INFO_ISR("D-ISR");
    } else {
        DVP_CAM_ERROR_ISR("D-ISR OVF");
    }

    return ret == pdPASS;
}

/**
 * @brief Start CAM DVP camera controller
 *
 * @param ctlr ESP CAM controller
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_start_capturing(dvp_cam_ctlr_t *ctlr)
{
    esp_err_t ret;

    ret = gdma_reset(ctlr->dma_chan);
    if (ret != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "failed to reset GDMA");
        return ret;
    }

    ret = gdma_start(ctlr->dma_chan, (intptr_t)ctlr->dma_desc);
    if (ret != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "failed to start GDMA");
        return ret;
    }

    cam_hal_start_streaming_ext(&ctlr->hal, ctlr->dma_buffer_hsize);

    /** This process is from esp32-camera and it is required */

    esp_rom_gpio_connect_in_signal(ctlr->vsync_pin, CAM_V_SYNC_IDX, false);
    esp_rom_delay_us(10);
    esp_rom_gpio_connect_in_signal(ctlr->vsync_pin, CAM_V_SYNC_IDX, true);

    return ret;
}

/**
 * @brief Stop CAM DVP camera controller
 *
 * @param ctlr ESP CAM controller
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_stop_capturing(dvp_cam_ctlr_t *ctlr)
{
    esp_err_t ret;

    cam_hal_stop_streaming(&ctlr->hal);

    ret = gdma_stop(ctlr->dma_chan);
    if (ret != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "failed to stop GDMA");
    }

    return ret;
}

/**
 * @brief Delete CAM DVP camera controller
 *
 * @param handle ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_cam_ctlr_del(esp_cam_ctlr_handle_t handle)
{
    dvp_cam_ctlr_t *ctlr = (dvp_cam_ctlr_t *)handle;

    /* Step 1: delete DVP receive task */

    vTaskDelete(ctlr->task_handle);

    /* Step 2: disable and free interrupt */

    gpio_intr_disable(ctlr->vsync_pin);

    /* Step 3: stop and de-initialize DVP receive */

    dvp_stop_capturing(ctlr);
    cam_hal_deinit(&ctlr->hal);

    /* Step 4: Free interrupt */

    gpio_isr_handler_remove(ctlr->vsync_pin);

    if (dvp_dma_deinit(ctlr->dma_chan) != ESP_OK) {
        ESP_LOGE(TAG, "failed to deinit GDMA");
    }

    /* Step 5: Free memory resource */

    heap_caps_free(ctlr->dma_desc);
    heap_caps_free(ctlr->dma_buffer);
    vQueueDelete(ctlr->event_queue);
    heap_caps_free(ctlr);

    return ESP_OK;
}

/**
 * @brief Register CAM DVP camera controller event callbacks
 *
 * @param handle        ESP CAM controller handle
 * @param cbs           ESP CAM controller event callbacks
 * @param user_data     ESP CAM controller event user data
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_cam_ctlr_register_event_callbacks(esp_cam_ctlr_handle_t handle, const esp_cam_ctlr_evt_cbs_t *cbs, void *user_data)
{
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    dvp_cam_ctlr_t *ctlr = (dvp_cam_ctlr_t *)handle;

    ESP_RETURN_ON_FALSE(handle && cbs, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle or cbs is null");
    ESP_RETURN_ON_FALSE(cbs->on_get_new_trans, ESP_ERR_INVALID_ARG, TAG, "invalid argument: on_get_new_trans is null");
    ESP_RETURN_ON_FALSE(cbs->on_trans_finished, ESP_ERR_INVALID_ARG, TAG, "invalid argument: on_trans_finished is null");

    if (ctlr->dvp_fsm == DVP_CAM_FSM_INIT) {
        ctlr->cbs.on_get_new_trans = cbs->on_get_new_trans;
        ctlr->cbs.on_trans_finished = cbs->on_trans_finished;
        ctlr->cbs_user_data = user_data;
        ret = ESP_OK;
    }

    return ret;
}

/**
 * @brief Start CAM DVP camera controller
 *
 * @param handle ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_cam_ctlr_start(esp_cam_ctlr_handle_t handle)
{
    esp_err_t ret;
    dvp_cam_ctlr_t *ctlr = (dvp_cam_ctlr_t *)handle;

    ctlr->dvp_fsm = DVP_CAM_FSM_STARTED;
    ret = gpio_intr_enable(ctlr->vsync_pin);
    if (ret != ESP_OK) {
        ctlr->dvp_fsm = DVP_CAM_FSM_INIT;
        ESP_EARLY_LOGE(TAG, "failed to enable vsync interrupt");
    }

    return ret;
}

/**
 * @brief Stop CAM DVP camera controller
 *
 * @param handle ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t dvp_cam_ctlr_stop(esp_cam_ctlr_handle_t handle)
{
    esp_err_t ret;
    dvp_cam_ctlr_t *ctlr = (dvp_cam_ctlr_t *)handle;

    ctlr->dvp_fsm = DVP_CAM_FSM_INIT;
    ret = gpio_intr_disable(ctlr->vsync_pin);
    if (ret == ESP_OK) {
        ret = dvp_stop_capturing(ctlr);
    }

    return ret;
}

/**
 * @brief Empty function, do nothing
 */
static esp_err_t dvp_cam_ctlr_enable(esp_cam_ctlr_handle_t handle)
{
    return ESP_OK;
}

/**
 * @brief Empty function, do nothing
 */
static esp_err_t dvp_cam_ctlr_disable(esp_cam_ctlr_handle_t handle)
{
    return ESP_OK;
}

/**
 * @brief DVP receive signal and data task, this function will call receive callback
 *        function if one complete frame is received or error triggers
 *
 * @param p This pointer is DVP object data pointer
 *
 * @return None
 *
 * @note This task is called in high frequency, so it should be as fast as possible.
 */
static IRAM_ATTR void dvp_task(void *p)
{
    dvp_cam_event_t event;
    dvp_cam_ctlr_t *ctlr = (dvp_cam_ctlr_t *)p;
    esp_cam_ctlr_trans_t *trans = &ctlr->trans;

    while (1) {
        if (xQueueReceive(ctlr->event_queue, &event, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "failed to receive message");
            continue;
        }

        switch (event.type) {
        case DVP_CAM_EVENT_RECV_DATA: {
            if (ctlr->dvp_fsm == DVP_CAM_FSM_RXING) {
                size_t frame_size = ctlr->dma_buffer_hsize;

                /* Calculate received data size and check if frame left space is enough */

                if ((trans->received_size + frame_size) < trans->buflen) {
                    memcpy(trans->buffer + trans->received_size, DVP_CAM_CUR_BUF(ctlr), frame_size);
                    trans->received_size += frame_size;
                    ctlr->dma_desc_index = (ctlr->dma_desc_index + 1) % DVP_CAM_BUFFER_COUNT;
                } else if ((trans->received_size + frame_size) == trans->buflen) {
                    /* Skip this event and let next "DVP_CAM_EVENT_SYNC_END" event process this */
                } else {
                    DVP_CAM_ERROR("RX-DA OVF");
                }
            }
            break;
        }
        case DVP_CAM_EVENT_SYNC_END: {
            if (ctlr->dvp_fsm == DVP_CAM_FSM_STARTED) {
                trans->buffer = NULL;
                portENTER_CRITICAL(&ctlr->spinlock);
                /* Use spinlock to protect the critical section from concurrent ISR access */
                ctlr->cbs.on_get_new_trans(&(ctlr->base), trans, ctlr->cbs_user_data);
                portEXIT_CRITICAL(&ctlr->spinlock);
                if (trans->buffer && trans->buflen > 0) {
                    trans->received_size = 0;

                    ctlr->dma_desc_index = 0;
                    ctlr->dvp_fsm = DVP_CAM_FSM_RXING;

                    dvp_start_capturing(ctlr);
                }
            } else if (ctlr->dvp_fsm == DVP_CAM_FSM_RXING) {
                size_t frame_size;
                uint32_t next_dma_desc_addr = 0;

                if (ctlr->pic_format_jpeg) {
                    next_dma_desc_addr = get_next_dma_desc_addr(ctlr);
                }

                /* Stop DVP receive, and then no event will be sent */

                dvp_stop_capturing(ctlr);
                gpio_intr_disable(ctlr->vsync_pin);

                /* Get rest received data size and check if frame left space is enough */

                if (ctlr->pic_format_jpeg) {
                    frame_size = dvp_get_dma_valid_size(ctlr, next_dma_desc_addr);
                } else {
                    frame_size = ctlr->dma_buffer_hsize;
                }

                /* JPEG format frame size is random value, and it may not fill all buffer space */

                if (ctlr->pic_format_jpeg && ((trans->received_size + frame_size) > trans->buflen)) {
                    frame_size = trans->buflen - trans->received_size;
                }

                if ((trans->received_size + frame_size) <= trans->buflen) {
                    /* Decode received data and update receive data */

                    memcpy(trans->buffer + trans->received_size, DVP_CAM_CUR_BUF(ctlr), frame_size);
                    trans->received_size += frame_size;

                    if (ctlr->pic_format_jpeg) {
                        trans->received_size = dvp_calculate_jpeg_size(trans->buffer, trans->received_size);
                    } else {
                        if (ctlr->fb_size_in_bytes != trans->received_size) {
                            DVP_CAM_ERROR("RX:%d-%d", (int)ctlr->fb_size_in_bytes, (int)trans->received_size);
                            trans->received_size = 0;
                        }
                    }
                } else {
                    DVP_CAM_ERROR("RX-SV OVF");
                }

                if (trans->received_size) {
                    portENTER_CRITICAL(&ctlr->spinlock);
                    /* Use spinlock to protect the critical section from concurrent ISR access */

                    ctlr->cbs.on_trans_finished(&ctlr->base, trans, ctlr->cbs_user_data);
                    portEXIT_CRITICAL(&ctlr->spinlock);

                    trans->buffer = NULL;
                    portENTER_CRITICAL(&ctlr->spinlock);
                    /* Use spinlock to protect the critical section from concurrent ISR access */

                    ctlr->cbs.on_get_new_trans(&(ctlr->base), trans, ctlr->cbs_user_data);
                    portEXIT_CRITICAL(&ctlr->spinlock);
                    if (trans->buffer && trans->buflen > 0) {
                        trans->received_size = 0;

                        ctlr->dma_desc_index = 0;
                        ctlr->dvp_fsm = DVP_CAM_FSM_RXING;

                        dvp_start_capturing(ctlr);
                    } else {
                        ctlr->dvp_fsm = DVP_CAM_FSM_STARTED;
                    }
                } else {
                    trans->received_size = 0;

                    ctlr->dma_desc_index = 0;
                    ctlr->dvp_fsm = DVP_CAM_FSM_RXING;
                    dvp_start_capturing(ctlr);
                }

                gpio_intr_enable(ctlr->vsync_pin);
            } else {
                ESP_LOGW(TAG, "invalid state %d\n", ctlr->dvp_fsm);
            }
            break;
        }
        default:
            ESP_LOGE(TAG, "unknown event type: %d", event.type);
            break;
        }
    }
}

/**
 * @brief New ESP CAM DVP controller
 *
 * @param config      DVP controller configurations
 * @param ret_handle  Returned CAM controller handle
 *
 * @return
 *        - ESP_OK on success
 *        - ESP_ERR_INVALID_ARG:   Invalid argument
 *        - ESP_ERR_NO_MEM:        Out of memory
 *        - ESP_ERR_NOT_SUPPORTED: Currently not support modes or types
 *        - ESP_ERR_NOT_FOUND:     DVP is registered already
 */
esp_err_t esp_cam_new_dvp_ctlr_ext(const esp_cam_ctlr_dvp_config_t *config, esp_cam_ctlr_handle_t *ret_handle)
{
    esp_err_t ret;
    dvp_cam_ctlr_t *ctlr;
    size_t buffer_align_size = 4; /**< ESP32-S3 SRAM is 4 bytes aligned */
    size_t fb_size_in_bytes;
    size_t dma_buffer_max_size = DVP_CAM_DMA_BUFFER_SIZE;

    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: config or ret_handle is null");
    ESP_RETURN_ON_FALSE(config->ctlr_id < LCD_CAM_PERIPH_NUM, ESP_ERR_INVALID_ARG, TAG, "invalid argument: ctlr_id >= %d", LCD_CAM_PERIPH_NUM);
    ESP_RETURN_ON_FALSE(config->pin_dont_init || config->pin, ESP_ERR_INVALID_ARG, TAG, "invalid argument: pin_dont_init is unset and pin is null");
    ESP_RETURN_ON_FALSE(config->external_xtal || config->pin_dont_init || config->pin->xclk_io != GPIO_NUM_NC, ESP_ERR_INVALID_ARG, TAG, "invalid argument: xclk_io is not set");
    ESP_RETURN_ON_FALSE(config->external_xtal || config->xclk_freq, ESP_ERR_INVALID_ARG, TAG, "invalid argument: xclk_freq is not set");

    ESP_RETURN_ON_ERROR(dvp_get_frame_size(config, &fb_size_in_bytes), TAG, "invalid argument: input frame pixel format is not supported");

    ctlr = heap_caps_calloc(1, sizeof(dvp_cam_ctlr_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_NO_MEM, TAG, "no mem for CAM DVP controller context");

    /**
     * Calculate actually receive frame size, when DVP hardware has received half size of data,
     * DVP DMA triggers interrupt to send event "DVP_CAM_EVENT_RECV_DATA" to DVP task, and DVP task
     * need copy half size of data from DMA receive buffer to user buffer which is added by
     * API "dvp_device_add_buffer".
     */

    ctlr->dma_buffer_hsize = dvp_get_dma_buffer_hsize(dma_buffer_max_size, buffer_align_size, fb_size_in_bytes, config->pic_format_jpeg);
    ESP_GOTO_ON_FALSE(ctlr->dma_buffer_hsize > 0, ESP_ERR_INVALID_ARG, fail0, TAG, "invalid argument: dma_buffer_hsize is 0");
    ctlr->dma_buffer_size = ctlr->dma_buffer_hsize * DVP_CAM_BUFFER_COUNT;

    ctlr->dma_buffer = heap_caps_aligned_alloc(buffer_align_size, ctlr->dma_buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_GOTO_ON_FALSE(ctlr->dma_buffer, ESP_ERR_NO_MEM, fail0, TAG, "no mem for CAM DVP DMA receive buffer");
    memset(ctlr->dma_buffer, 0, ctlr->dma_buffer_size);

    ctlr->dma_desc_size = config->pic_format_jpeg ? DVP_CAM_JPEG_DMA_DESC_SIZE : DVP_CAM_DMA_DESC_BUFFER_SIZE;
    ctlr->dma_desc_hcnt = (ctlr->dma_buffer_hsize + ctlr->dma_desc_size - 1) / ctlr->dma_desc_size;

    size_t dma_desc_buffer_size = DVP_CAM_UP_ALIGN(DVP_CAM_BUFFER_COUNT * ctlr->dma_desc_hcnt * sizeof(dma_descriptor_t), buffer_align_size);
    ctlr->dma_desc = heap_caps_aligned_alloc(buffer_align_size, dma_desc_buffer_size, MALLOC_CAP_DMA);
    ESP_GOTO_ON_FALSE(ctlr->dma_desc, ESP_ERR_NO_MEM, fail1, TAG, "no mem for CAM DVP DMA receive description");

    dvp_config_dma_desc(ctlr->dma_desc, ctlr->dma_desc_size, ctlr->dma_buffer, ctlr->dma_buffer_hsize, &ctlr->dma_desc[ctlr->dma_desc_hcnt]);
    dvp_config_dma_desc(&ctlr->dma_desc[ctlr->dma_desc_hcnt], ctlr->dma_desc_size, &ctlr->dma_buffer[ctlr->dma_buffer_hsize], ctlr->dma_buffer_hsize, ctlr->dma_desc);

    ctlr->vsync_pin = config->pin ? config->pin->vsync_io : s_vsync_io;
    ESP_GOTO_ON_FALSE(ctlr->vsync_pin != GPIO_NUM_NC, ESP_ERR_INVALID_ARG, fail2, TAG, "vsync_pin is not set");
    ctlr->pic_format_jpeg = config->pic_format_jpeg;
    ctlr->spinlock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    ctlr->fb_size_in_bytes = fb_size_in_bytes;
    ctlr->dvp_fsm = DVP_CAM_FSM_INIT;

    /* Ignore result if this calling fails, maybe users call this in previous step */

    gpio_install_isr_service(ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM);

    /* Initialize DVP V-SYNC interrupt */

    ESP_GOTO_ON_ERROR(gpio_set_intr_type(ctlr->vsync_pin, GPIO_INTR_NEGEDGE), fail2, TAG, "Failed to set V-SYNC GPIO interrupt type");
    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(ctlr->vsync_pin, dvp_vsync_isr, ctlr), fail2, TAG, "failed to add V-SYNC interrupt handler");
    ESP_GOTO_ON_ERROR(gpio_intr_disable(ctlr->vsync_pin), fail3, TAG, "failed to disable V-SYNC interrupt");

    /* Initialize DVP controller */

    cam_hal_config_t hal_config = {
        .port = config->ctlr_id,
        .cam_data_width = DVP_CAM_BUS_IO_NUM,
        .bit_swap_en = false,
        .byte_swap_en = false,
    };
    cam_hal_init_ext(&ctlr->hal, &hal_config);

    ESP_GOTO_ON_ERROR(dvp_dma_init(&ctlr->dma_chan), fail4, TAG, "failed to initialize CAM DVP DMA");
    ESP_GOTO_ON_ERROR(gdma_get_channel_id(ctlr->dma_chan, &ctlr->dma_chan_id), fail5, TAG, "failed to get DMA channel ID");

    gdma_rx_event_callbacks_t cbs = {
        .on_recv_eof = dvp_receive_isr
    };
    ESP_GOTO_ON_ERROR(gdma_register_rx_event_callbacks(ctlr->dma_chan, &cbs, ctlr), fail5, TAG, "failed to register DMA event callbacks");

    ctlr->event_queue = xQueueCreate(DVP_CAM_EVENT_QUEUE_SIZE, sizeof(dvp_cam_event_t));
    ESP_GOTO_ON_FALSE(ctlr->event_queue, ESP_ERR_NO_MEM, fail5, TAG, "failed to create event queue");

    ctlr->base.del = dvp_cam_ctlr_del;
    ctlr->base.enable = dvp_cam_ctlr_enable;
    ctlr->base.start = dvp_cam_ctlr_start;
    ctlr->base.stop = dvp_cam_ctlr_stop;
    ctlr->base.disable = dvp_cam_ctlr_disable;
    ctlr->base.register_event_callbacks = dvp_cam_ctlr_register_event_callbacks;
    ctlr->base.get_internal_buffer = NULL;
    ctlr->base.get_buffer_len = NULL;
    ctlr->base.alloc_buffer = NULL;
    ctlr->base.format_conversion = NULL;

    ret = xTaskCreate(dvp_task, DVP_CAM_TASK_NAME, DVP_CAM_TASK_STACK_SIZE, ctlr, DVP_CAM_TASK_PRIORITY, &ctlr->task_handle);
    ESP_GOTO_ON_FALSE(ret == pdPASS, ESP_ERR_NO_MEM, fail6, TAG, "failed to create DVP task");

    *ret_handle = (esp_cam_ctlr_handle_t)ctlr;

    ESP_LOGI(TAG, "DVP Extended camera controller driver is initialized");

    return ESP_OK;

fail6:
    vQueueDelete(ctlr->event_queue);
fail5:
    dvp_dma_deinit(ctlr->dma_chan);
fail4:
    cam_hal_deinit(&ctlr->hal);
fail3:
    gpio_isr_handler_remove(ctlr->vsync_pin);
fail2:
    heap_caps_free(ctlr->dma_desc);
fail1:
    heap_caps_free(ctlr->dma_buffer);
fail0:
    heap_caps_free(ctlr);
    return ret;
}

/**
 * @brief ESP CAM DVP initialize clock and GPIO.
 *
 * @param ctlr_id CAM DVP controller ID
 * @param clk_src CAM DVP clock source
 * @param pin     CAM DVP pin configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_cam_ctlr_dvp_init_ext(int ctlr_id, cam_clock_source_t clk_src, const esp_cam_ctlr_dvp_pin_config_t *pin)
{
    ESP_RETURN_ON_FALSE(pin, ESP_ERR_INVALID_ARG, TAG, "invalid argument: pin is null");

    /**
     * Save V-SYNC pin number for later use
     */
    s_vsync_io = pin->vsync_io;

    return esp_cam_ctlr_dvp_init(ctlr_id, clk_src, pin);
}
#endif /* ESP_CAM_CTRL_DVP_ENABLE */
