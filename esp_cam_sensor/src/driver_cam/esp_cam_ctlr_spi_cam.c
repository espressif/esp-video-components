/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #undef LOG_LOCAL_LEVEL
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "hal/color_hal.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "esp_memory_utils.h"
#include "esp_private/esp_cache_private.h"
#include "esp_private/spi_slave_internal.h"
#include "esp_cache.h"
#include "esp_cam_ctlr_types.h"
#include "../driver_spi/esp_cam_spi_slave.h"
#include "esp_cam_ctlr_spi_cam.h"
#include "esp_cam_ctlr_spi.h"

#if CONFIG_CAM_CTLR_SPI_ISR_CACHE_SAFE
#define SPI_CAM_ISR_ATTR            IRAM_ATTR
#else
#define SPI_CAM_ISR_ATTR
#endif

#define SPI_TASK_STACK_SIZE         4096
#define SPI_TASK_PRIORITY           10
#define SPI_TASK_NAME_BASE          "spi_cam%d"

#define ALIGN_UP_BY(num, align)     (((num) + ((align) - 1)) / (align) * (align))

/**
 * @brief SPI message type
 */
typedef enum spi_cam_msg_type {
    SPI_CAM_MSG_FRAME_RECVED = 0,    /*!< Frame received message */
    SPI_CAM_MSG_MAX,                 /*!< Maximum message type */
} spi_cam_msg_type_t;

/**
 * @brief SPI message
 */
typedef struct spi_cam_msg {
    spi_cam_msg_type_t type;         /*!< Message type */
    union {
        struct {
            uint8_t *buffer;         /*!< RX buffer pointer */
            uint32_t length;         /*!< RX buffer length in bytes */
        } recved_frame;             /*!< Frame received message payload */
    };
} spi_cam_msg_t;

static const char *TAG = "spi_cam";

/**
 * @brief Setup SPI transaction buffer
 *
 * @param ctlr ESP CAM controller handle
 * @param spi_trans SPI slave transaction
 *
 * @return None
 */
static void SPI_CAM_ISR_ATTR setup_trans_buffer(esp_cam_ctlr_spi_cam_t *ctlr, spi_slave_transaction_t *spi_trans)
{
    bool buffer_ready = false;
    esp_cam_ctlr_trans_t trans = {0};

    if (ctlr->cbs.on_get_new_trans) {
        ctlr->cbs.on_get_new_trans(&(ctlr->base), &trans, ctlr->cbs_user_data);
        if (trans.buffer && (trans.buflen >= ctlr->fb_size_in_bytes)) {
            spi_trans->rx_buffer = trans.buffer;
            spi_trans->length = trans.buflen * 8;
            spi_trans->user = ctlr;
            buffer_ready = true;
        }
    }

    if (!buffer_ready) {
        if (!ctlr->bk_buffer_dis) {
            spi_trans->rx_buffer = ctlr->frame_buffer;
            spi_trans->length = ctlr->fb_size_in_bytes * 8;
            spi_trans->user = ctlr;
            buffer_ready = true;
        } else {
            assert(false && "no new buffer, and no driver internal buffer");
        }
    }
}

/**
 * @brief SPI slave transaction done callback
 *
 * @param spi_trans SPI slave transaction
 *
 * @return None
 */
static void SPI_CAM_ISR_ATTR spi_cam_post_trans_cb(spi_slave_transaction_t *spi_trans)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)spi_trans->user;

    if ((spi_trans->rx_buffer != ctlr->frame_buffer) || ctlr->bk_buffer_exposed) {
        if (!ctlr->auto_decode_dis) {
            spi_cam_msg_t msg;
            BaseType_t xTaskWoken = 0;

            msg.type = SPI_CAM_MSG_FRAME_RECVED;
            msg.recved_frame.buffer = spi_trans->rx_buffer;
            msg.recved_frame.length = ctlr->bf_size_in_bytes;
            if (xQueueSendFromISR(ctlr->spi_recv_queue, &msg, &xTaskWoken) == pdPASS) {
                if (xTaskWoken) {
                    portYIELD_FROM_ISR(xTaskWoken);
                }
            } else {
                ESP_EARLY_LOGD(TAG, "failed to send frame received message");
            }
        } else {
            esp_cam_ctlr_trans_t trans = {
                .buffer = spi_trans->rx_buffer,
                .buflen = ctlr->fb_size_in_bytes,
                .received_size = ctlr->fb_size_in_bytes,
            };

            if (ctlr->cbs.on_trans_finished) {
                ctlr->cbs.on_trans_finished(&(ctlr->base), &trans, ctlr->cbs_user_data);
            }
        }
    }

    setup_trans_buffer(ctlr, spi_trans);
    if (esp_cam_spi_slave_queue_trans_isr(ctlr->spi_port, spi_trans) != ESP_OK) {
        assert(false && "SPI slave queue size is smaller than actual frame count");
    }
}

/**
 * @brief Initialize SPI slave interface
 *
 * @param ctlr        ESP CAM controller handle
 * @param config      SPI controller configurations
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_init_intf(esp_cam_ctlr_spi_cam_t *ctlr, const esp_cam_ctlr_spi_config_t *config)
{
    esp_err_t ret;
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->spi_data0_io_pin,
        .sclk_io_num = config->spi_sclk_pin,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ctlr->fb_size_in_bytes,
#if CONFIG_CAM_CTLR_SPI_ISR_CACHE_SAFE
        .intr_flags = ESP_INTR_FLAG_IRAM
#endif
    };

    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = config->spi_cs_pin,
        .queue_size = 1,
        .post_trans_cb = spi_cam_post_trans_cb,
        .flags = SPI_SLAVE_NO_RETURN_RESULT
    };

    ESP_RETURN_ON_ERROR(esp_cam_spi_slave_initialize(ctlr->spi_port, &buscfg, &slvcfg, SPI_DMA_CH_AUTO), TAG, "failed to initialize SPI slave");
    ESP_GOTO_ON_ERROR(esp_cam_spi_slave_disable(ctlr->spi_port), fail0, TAG, "failed to disable SPI slave");

    return ESP_OK;

fail0:
    esp_cam_spi_slave_free(ctlr->spi_port);
    return ret;
}

/**
 * @brief Deinitialize SPI interface
 *
 * @param ctlr ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_deinit_intf(esp_cam_ctlr_spi_cam_t *ctlr)
{
    ESP_RETURN_ON_ERROR(esp_cam_spi_slave_free(ctlr->spi_port), TAG, "failed to free SPI slave");

    return ESP_OK;
}

/**
 * @brief Decode frame, remove frame header and line header, then copy the image data to the destination buffer
 *
 * @note The source buffer and the destination buffer can be the same buffer
 *
 * @param ctlr ESP CAM controller handle
 * @param src Source buffer pointer
 * @param dst Destination buffer pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_decode(esp_cam_ctlr_spi_cam_t *ctlr, uint8_t *src, uint8_t *dst)
{
    bool decode_check_dis = ctlr->decode_check_dis;

    if (!decode_check_dis && (memcmp(src, ctlr->frame_info->frame_header_check, ctlr->frame_info->frame_header_check_size) != 0)) {
        ESP_LOGD(TAG, "invalid frame header");
        return ESP_FAIL;
    }
    src += ctlr->frame_info->frame_header_size;

    int line_data_size = ctlr->frame_info->line_size - ctlr->frame_info->line_header_size;

    for (uint32_t i = 0; i < ctlr->fb_lines; i++) {
        if (!decode_check_dis && (memcmp(src, ctlr->frame_info->line_header_check, ctlr->frame_info->line_header_check_size) != 0)) {
            ESP_LOGD(TAG, "invalid line header");
            return ESP_FAIL;
        }
        src += ctlr->frame_info->line_header_size;

        memcpy(dst, src, line_data_size);

        src += line_data_size;
        dst += line_data_size;
    }

    return ESP_OK;
}

/**
 * @brief Image decode task
 *
 * @param arg ESP CAM controller handle
 *
 * @return None
 */
static void spi_cam_task(void *arg)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)arg;
    spi_cam_msg_t msg;

    while (1) {
        if (xQueueReceive(ctlr->spi_recv_queue, &msg, portMAX_DELAY) == pdPASS) {
            if (msg.type == SPI_CAM_MSG_FRAME_RECVED) {
                esp_err_t ret;
                uint8_t *decoded_buffer;
                uint8_t *rx_buffer = msg.recved_frame.buffer;

                if (rx_buffer == ctlr->frame_buffer) {
                    ret = spi_cam_decode(ctlr, rx_buffer, ctlr->backup_buffer);
                    decoded_buffer = ctlr->backup_buffer;
                } else {
                    ret = spi_cam_decode(ctlr, rx_buffer, rx_buffer);
                    decoded_buffer = rx_buffer;
                }

                if (ret == ESP_OK) {
                    esp_cam_ctlr_trans_t trans = {0};

                    trans.buffer = decoded_buffer;
                    trans.buflen = ctlr->bf_size_in_bytes;
                    trans.received_size = trans.buflen;
                    if (ctlr->cbs.on_trans_finished) {
                        ctlr->cbs.on_trans_finished(&(ctlr->base), &trans, ctlr->cbs_user_data);
                    }
                    ESP_LOGD(TAG, "frame decoded");
                } else {
                    ESP_LOGD(TAG, "failed to decode frame");
                }
            }
        }
    }
}

/**
 * @brief Enable CAM SPI camera controller
 *
 * @param handle ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_enable(esp_cam_ctlr_handle_t handle)
{
    esp_err_t ret;
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE(ctlr->fsm == ESP_CAM_CTLR_SPI_CAM_FSM_INIT, ESP_ERR_INVALID_STATE, TAG, "processor isn't in init state");

    ret = esp_cam_spi_slave_enable(ctlr->spi_port);
    if (ret == ESP_OK) {
        ctlr->fsm = ESP_CAM_CTLR_SPI_CAM_FSM_ENABLED;
    }

    return ret;
}

/**
 * @brief Disable CAM SPI camera controller
 *
 * @param handle ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_disable(esp_cam_ctlr_handle_t handle)
{
    esp_err_t ret;
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE(ctlr->fsm == ESP_CAM_CTLR_SPI_CAM_FSM_ENABLED, ESP_ERR_INVALID_STATE, TAG, "processor isn't in enabled state");

    ret = esp_cam_spi_slave_disable(ctlr->spi_port);
    if (ret == ESP_OK) {
        ctlr->fsm = ESP_CAM_CTLR_SPI_CAM_FSM_INIT;
    }

    return ret;
}

/**
 * @brief Start CAM SPI camera controller
 *
 * @param handle ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_start(esp_cam_ctlr_handle_t handle)
{
    esp_err_t ret;
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE(ctlr->fsm == ESP_CAM_CTLR_SPI_CAM_FSM_ENABLED, ESP_ERR_INVALID_STATE, TAG, "processor isn't in enabled state");

    setup_trans_buffer(ctlr, &ctlr->spi_trans);
    ret = esp_cam_spi_slave_queue_trans(ctlr->spi_port, &ctlr->spi_trans, portMAX_DELAY);
    if (ret == ESP_OK) {
        ctlr->fsm = ESP_CAM_CTLR_SPI_CAM_FSM_STARTED;
    }

    return ret;
}

/**
 * @brief Stop CAM SPI camera controller
 *
 * @param handle ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_stop(esp_cam_ctlr_handle_t handle)
{
    esp_err_t ret;
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE(ctlr->fsm == ESP_CAM_CTLR_SPI_CAM_FSM_STARTED, ESP_ERR_INVALID_STATE, TAG, "processor isn't in started state");

    ret = esp_cam_spi_slave_disable(ctlr->spi_port);
    if (ret == ESP_OK) {
        ctlr->fsm = ESP_CAM_CTLR_SPI_CAM_FSM_ENABLED;
    }

    return ret;
}

/**
 * @brief Register CAM SPI camera controller event callbacks
 *
 * @param handle        ESP CAM controller handle
 * @param cbs           ESP CAM controller event callbacks
 * @param user_data     ESP CAM controller event user data
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_register_event_callbacks(esp_cam_ctlr_handle_t handle, const esp_cam_ctlr_evt_cbs_t *cbs, void *user_data)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;

    ESP_RETURN_ON_FALSE(handle && cbs, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle or cbs is null");
    ESP_RETURN_ON_FALSE(cbs->on_trans_finished, ESP_ERR_INVALID_ARG, TAG, "invalid argument: on_trans_finished is null");
    ESP_RETURN_ON_FALSE(cbs->on_get_new_trans || !ctlr->bk_buffer_dis, ESP_ERR_INVALID_ARG, TAG, "invalid argument: on_get_new_trans is null");
    ESP_RETURN_ON_FALSE(ctlr->fsm == ESP_CAM_CTLR_SPI_CAM_FSM_INIT, ESP_ERR_INVALID_STATE, TAG, "driver starts already, not allow cbs register");

#if CONFIG_CAM_CTLR_SPI_ISR_CACHE_SAFE
    if (cbs->on_get_new_trans) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(cbs->on_get_new_trans), ESP_ERR_INVALID_ARG, TAG, "on_get_new_trans callback not in IRAM");
    }
    if (cbs->on_trans_finished) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(cbs->on_trans_finished), ESP_ERR_INVALID_ARG, TAG, "on_trans_finished callback not in IRAM");
    }
#endif

    ctlr->cbs.on_get_new_trans = cbs->on_get_new_trans;
    ctlr->cbs.on_trans_finished = cbs->on_trans_finished;
    ctlr->cbs_user_data = user_data;

    return ESP_OK;
}

/**
 * @brief Get SPI camera controller backup buffer pointer
 *
 * @param handle        ESP CAM controller handle
 * @param fb_num        Backup buffer pointer storage buffer number
 * @param fb0           Backup buffer pointer storage buffer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_get_internal_buffer(esp_cam_ctlr_handle_t handle, uint32_t fb_num, const void **fb0, ...)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE((ctlr->fsm >= ESP_CAM_CTLR_SPI_CAM_FSM_INIT) && (ctlr->backup_buffer), ESP_ERR_INVALID_STATE, TAG, "driver don't initialized or back_buffer not available");
    ESP_RETURN_ON_FALSE(fb_num && fb_num <= 1, ESP_ERR_INVALID_ARG, TAG, "invalid frame buffer number");

    va_list args;
    const void **fb_itor = fb0;

    va_start(args, fb0);
    for (uint32_t i = 0; i < fb_num; i++) {
        if (fb_itor) {
            *fb_itor = ctlr->auto_decode_dis ? ctlr->frame_buffer : ctlr->backup_buffer;
            fb_itor = va_arg(args, const void **);
        }
    }
    va_end(args);

    ctlr->bk_buffer_exposed = true;

    return ESP_OK;
}

/**
 * @brief Get CAM SPI camera controller frame buffer length
 *
 * @param handle        ESP CAM controller handle
 * @param ret_fb_len    The size of each frame buffer in bytes
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_get_frame_buffer_len(esp_cam_ctlr_handle_t handle, size_t *ret_fb_len)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr && ret_fb_len, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle or ret_fb_len is null");
    ESP_RETURN_ON_FALSE((ctlr->fsm >= ESP_CAM_CTLR_SPI_CAM_FSM_INIT) && (ctlr->backup_buffer), ESP_ERR_INVALID_STATE, TAG, "driver don't initialized or back_buffer not available");

    *ret_fb_len = ctlr->auto_decode_dis ? ctlr->fb_size_in_bytes : ctlr->bf_size_in_bytes;

    return ESP_OK;
}

/**
 * @brief Delete CAM SPI camera controller
 *
 * @param handle ESP CAM controller handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_del(esp_cam_ctlr_handle_t handle)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE(ctlr->fsm == ESP_CAM_CTLR_SPI_CAM_FSM_INIT, ESP_ERR_INVALID_STATE, TAG, "processor isn't in init state");

    if (!ctlr->auto_decode_dis) {
        vTaskDelete(ctlr->spi_task_handle);
        vQueueDelete(ctlr->spi_recv_queue);
    }

    spi_cam_deinit_intf(ctlr);

    if (!ctlr->bk_buffer_dis) {
        heap_caps_free(ctlr->backup_buffer);
        heap_caps_free(ctlr->frame_buffer);
    }

    heap_caps_free(ctlr);

    return ESP_OK;
}

/**
 * @brief New ESP CAM SPI controller
 *
 * @param config      SPI controller configurations
 * @param ret_handle  Returned CAM controller handle
 *
 * @return
 *        - ESP_OK on success
 *        - ESP_ERR_INVALID_ARG:   Invalid argument
 *        - ESP_ERR_NO_MEM:        Out of memory
 *        - ESP_ERR_NOT_SUPPORTED: Currently not support modes or types
 *        - ESP_ERR_NOT_FOUND:     SPI is registered already
 */
esp_err_t esp_cam_new_spi_ctlr(const esp_cam_ctlr_spi_config_t *config, esp_cam_ctlr_handle_t *ret_handle)
{
    esp_err_t ret;
    size_t alignment_size;
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: config or ret_handle is null");
    ESP_RETURN_ON_FALSE(config->frame_info, ESP_ERR_INVALID_ARG, TAG, "invalid argument: frame_info is null");

    ESP_RETURN_ON_ERROR(esp_cache_get_alignment(MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA, &alignment_size), TAG, "failed to get cache alignment");

    esp_cam_ctlr_spi_cam_t *ctlr = heap_caps_calloc(1, sizeof(esp_cam_ctlr_spi_cam_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_NO_MEM, TAG, "no mem for CAM SPI controller context");

    ctlr->frame_info = config->frame_info;
    ctlr->fb_lines = config->v_res;
    ctlr->fb_size_in_bytes = ALIGN_UP_BY(config->frame_info->frame_size, alignment_size);
    ctlr->bf_size_in_bytes = config->frame_info->frame_size - config->frame_info->frame_header_size - config->frame_info->line_header_size * config->v_res;
    ctlr->bk_buffer_dis = config->bk_buffer_dis;
    ctlr->bk_buffer_sram = config->bk_buffer_sram;
    ctlr->auto_decode_dis = config->auto_decode_dis;
    ctlr->fsm = ESP_CAM_CTLR_SPI_CAM_FSM_INIT;

    ESP_LOGD(TAG, "fb_size_in_bytes: %" PRIu32 ", bf_size_in_bytes: %" PRIu32, ctlr->fb_size_in_bytes, ctlr->bf_size_in_bytes);

    if (!config->bk_buffer_dis) {
        uint32_t heap_cap = MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED;

#if CONFIG_SPIRAM
        if (config->bk_buffer_sram) {
            heap_cap |= MALLOC_CAP_SPIRAM;
        } else {
            heap_cap |= MALLOC_CAP_INTERNAL;
        }
#else
        heap_cap |= MALLOC_CAP_INTERNAL;
#endif

        ctlr->frame_buffer = heap_caps_calloc(1, ctlr->fb_size_in_bytes, heap_cap);
        ESP_GOTO_ON_FALSE(ctlr->frame_buffer, ESP_ERR_NO_MEM, fail0, TAG, "no mem for SPI frame buffer");

        if (!ctlr->auto_decode_dis) {
            ctlr->backup_buffer = heap_caps_calloc(1, ctlr->bf_size_in_bytes, heap_cap);
            ESP_GOTO_ON_FALSE(ctlr->backup_buffer, ESP_ERR_NO_MEM, fail1, TAG, "no mem for SPI backup buffer");
        }
    }

    ctlr->spi_port = config->spi_port;
    ESP_GOTO_ON_ERROR(spi_cam_init_intf(ctlr, config), fail2, TAG, "failed to initialize SPI slave");

    ctlr->base.del = spi_cam_del;
    ctlr->base.enable = spi_cam_enable;
    ctlr->base.start = spi_cam_start;
    ctlr->base.stop = spi_cam_stop;
    ctlr->base.disable = spi_cam_disable;
    ctlr->base.register_event_callbacks = spi_cam_register_event_callbacks;
    ctlr->base.get_internal_buffer = spi_cam_get_internal_buffer;
    ctlr->base.get_buffer_len = spi_cam_get_frame_buffer_len;

    if (!ctlr->auto_decode_dis) {
        char name[12];

        ret = snprintf(name, sizeof(name), SPI_TASK_NAME_BASE, config->spi_port);
        assert(ret > 0 && ret < sizeof(name));

        ctlr->spi_recv_queue = xQueueCreate(config->frame_buffer_count + 1, sizeof(spi_cam_msg_t));
        ESP_GOTO_ON_FALSE(ctlr->spi_recv_queue, ESP_ERR_NO_MEM, fail3, TAG, "failed to create SPI receive queue");

        BaseType_t os_ret = xTaskCreate(spi_cam_task, name, SPI_TASK_STACK_SIZE, ctlr, SPI_TASK_PRIORITY, &ctlr->spi_task_handle);
        ESP_GOTO_ON_FALSE(os_ret == pdPASS, ESP_ERR_NO_MEM, fail4, TAG, "failed to create SPI task");
    }

    *ret_handle = &ctlr->base;

    return ESP_OK;

fail4:
    vQueueDelete(ctlr->spi_recv_queue);
fail3:
    spi_cam_deinit_intf(ctlr);
fail2:
    heap_caps_free(ctlr->backup_buffer);
fail1:
    heap_caps_free(ctlr->frame_buffer);
fail0:
    heap_caps_free(ctlr);
    return ret;
}

/**
 * @brief Decode frame, remove frame header and line header, then copy the image data to the destination buffer
 *
 * @note The source buffer and the destination buffer can be the same buffer
 *
 * @param handle ESP CAM controller handle
 *
 * @param src Source buffer pointer
 * @param src_len Source buffer length
 * @param dst Destination buffer pointer
 * @param dst_len Destination buffer length
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_cam_spi_decode_frame(esp_cam_ctlr_handle_t handle, uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE(ctlr->auto_decode_dis, ESP_ERR_INVALID_STATE, TAG, "auto decode is enabled");
    ESP_RETURN_ON_FALSE(src && dst, ESP_ERR_INVALID_ARG, TAG, "invalid argument: src or dst is null");
    ESP_RETURN_ON_FALSE(src_len == ctlr->fb_size_in_bytes && dst_len >= ctlr->bf_size_in_bytes, ESP_ERR_INVALID_ARG, TAG, "invalid argument: src_len or dst_len is invalid");

    return spi_cam_decode(ctlr, src, dst);
}
