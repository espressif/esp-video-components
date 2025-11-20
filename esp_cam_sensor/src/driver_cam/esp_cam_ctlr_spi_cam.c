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
#include "esp_idf_version.h"
#include "../driver_spi/esp_cam_spi_slave.h"
#include "esp_cam_ctlr_spi_cam.h"
#include "esp_cam_ctlr_spi.h"

/**
 * Current the parlio RX driver is not available in esp-idf versions which is less than v6.1.0.
 */
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 1, 0))
#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
#undef CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
#pragma message("PARLIO RX driver is not available for SPI camera sensor driver in current IDF version")
#endif
#endif

#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
#include "esp_private/parlio_rx_private.h"
#endif

#if CONFIG_CAM_CTLR_SPI_ISR_CACHE_SAFE || CONFIG_PARLIO_RX_ISR_CACHE_SAFE
#define SPI_CAM_ISR_ATTR            IRAM_ATTR
#else
#define SPI_CAM_ISR_ATTR
#endif

#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
#define PARLIO_TASK_NAME_BASE       "parlio_msg"
#define PARLIO_TASK_STACK_SIZE      CONFIG_CAM_CTLR_PARLIO_MESSAGE_TASK_STACK_SIZE
#define PARLIO_TASK_PRIORITY        CONFIG_CAM_CTLR_PARLIO_MESSAGE_TASK_PRIORITY
#endif

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
#define SPI_TASK_STACK_SIZE         CONFIG_CAM_CTLR_SPI_DECODE_TASK_STACK_SIZE
#define SPI_TASK_PRIORITY           CONFIG_CAM_CTLR_SPI_DECODE_TASK_PRIORITY
#define SPI_TASK_NAME_BASE          "spi_cam%d"

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
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */


#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
typedef enum parlio_msg_type {
    PARLIO_MSG_FRAME_RECVED = 0,    /*!< Frame received message */
    PARLIO_MSG_EXIT,                /*!< Exit message */
    PARLIO_MSG_MAX,                 /*!< Maximum message type */
} parlio_msg_type_t;

typedef struct parlio_msg {
    parlio_msg_type_t type;         /*!< Message type */
} parlio_msg_t;
#endif /* CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO */

#if CONFIG_SPIRAM
#define SPI_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA)
#else
#define SPI_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_DMA)
#endif

#define ALIGN_UP_BY(num, align)     (((num) + ((align) - 1)) / (align) * (align))

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

    if (ctlr->dropped_frame_count >= ctlr->drop_frame_count) {
        if (ctlr->cbs.on_get_new_trans) {
            esp_cam_ctlr_trans_t trans = {0};

            ctlr->cbs.on_get_new_trans(&(ctlr->base), &trans, ctlr->cbs_user_data);
            if (trans.buffer && (trans.buflen >= ctlr->fb_size_in_bytes)) {
                spi_trans->rx_buffer = trans.buffer;
                spi_trans->length = trans.buflen * 8;
                spi_trans->user = ctlr;

#if !CAM_CTLR_SPI_HAS_BACKUP_BUFFER
                ctlr->setup_ll_buffer = 0;
#endif /* !CAM_CTLR_SPI_HAS_BACKUP_BUFFER */

                buffer_ready = true;
            }
        }
    }

    if (!buffer_ready) {
#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
        if (!ctlr->bk_buffer_dis) {
            spi_trans->rx_buffer = ctlr->frame_buffer;
            spi_trans->length = ctlr->fb_size_in_bytes * 8;
            spi_trans->user = ctlr;
        } else {
            assert(false && "no new buffer, and no driver internal buffer");
        }
#else /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
        /**
         * Setup the smallest low-level buffer as the SPI RX buffer
         * to avoid the SPI receive ISR to stop.
         *
         * Compare to sending free buffer to queue in application thread,
         * this method is able to avoid the multi-threaded access to the
         * SPI RX transaction.
         */

        spi_trans->rx_buffer = ctlr->spi_ll_buffer;
        spi_trans->length = ctlr->spi_ll_buffer_size * 8;
        spi_trans->user = ctlr;

        ctlr->setup_ll_buffer = 1;
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
    }
}

/**
 * @brief SPI CAM receive done callback
 *
 * @param ctlr ESP CAM controller handle
 * @param rx_buffer RX buffer
 * @param rx_len RX buffer length
 *
 * @return None
 */
static void SPI_CAM_ISR_ATTR spi_cam_receive_done(esp_cam_ctlr_spi_cam_t *ctlr, uint8_t *rx_buffer, uint32_t rx_len)
{
    /**
     * Due to SPI slave does not support stop function,
     * we need to check if the controller is started,
     * if not started, we need to return immediately.
     */
    if (ctlr->fsm != ESP_CAM_CTLR_SPI_CAM_FSM_STARTED) {
        return;
    }

    if (ctlr->dropped_frame_count >= ctlr->drop_frame_count) {
#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
        if ((rx_buffer != ctlr->frame_buffer) || ctlr->bk_buffer_exposed)
#else /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
        if (!ctlr->setup_ll_buffer)
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
        {
#if CAM_CTLR_SPI_HAS_AUTO_DECODE
            if (!ctlr->auto_decode_dis) {
                spi_cam_msg_t msg;
                BaseType_t xTaskWoken = 0;

                msg.type = SPI_CAM_MSG_FRAME_RECVED;
                msg.recved_frame.buffer = rx_buffer;
                msg.recved_frame.length = ctlr->bf_size_in_bytes;
                if (xQueueSendFromISR(ctlr->spi_recv_queue, &msg, &xTaskWoken) == pdPASS) {
                    if (xTaskWoken) {
                        portYIELD_FROM_ISR(xTaskWoken);
                    }
                } else {
                    ESP_EARLY_LOGD(TAG, "failed to send frame received message");
                }
            } else
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */
            {
                esp_cam_ctlr_trans_t trans = {
                    .buffer = rx_buffer,
                    .buflen = ctlr->fb_size_in_bytes,
                    .received_size = ctlr->fb_size_in_bytes,
                };

                if (ctlr->cbs.on_trans_finished) {
                    ctlr->cbs.on_trans_finished(&(ctlr->base), &trans, ctlr->cbs_user_data);
                }
            }
        }
    } else {
        ctlr->dropped_frame_count++;
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

    spi_cam_receive_done(ctlr, spi_trans->rx_buffer, spi_trans->length / 8);

    setup_trans_buffer(ctlr, spi_trans);
    if (esp_cam_spi_slave_queue_trans_isr(ctlr->spi.port, spi_trans) != ESP_OK) {
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
static esp_err_t spi_cam_init_intf_spi(esp_cam_ctlr_spi_cam_t *ctlr, const esp_cam_ctlr_spi_config_t *config)
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
        .mode = 1, // 1: CPOL=0, CPHA=1; CLK idle low, data capture on rising edge
        .spics_io_num = config->spi_cs_pin,
        .queue_size = 1,
        .post_trans_cb = spi_cam_post_trans_cb,
        .flags = SPI_SLAVE_NO_RETURN_RESULT
    };

    ESP_RETURN_ON_ERROR(esp_cam_spi_slave_initialize(ctlr->spi.port, &buscfg, &slvcfg, SPI_DMA_CH_AUTO), TAG, "failed to initialize SPI slave");
    ESP_GOTO_ON_ERROR(esp_cam_spi_slave_disable(ctlr->spi.port), fail0, TAG, "failed to disable SPI slave");

    return ESP_OK;

fail0:
    esp_cam_spi_slave_free(ctlr->spi.port);
    return ret;
}

#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
/**
 * @brief Parlio transaction done callback
 *
 * @param rx_unit Parlio RX unit handle
 * @param edata Parlio receive event data
 * @param user_ctx User context
 *
 * @return true to awoke high priority tasks
 */
static bool SPI_CAM_ISR_ATTR parlio_rx_done_callback(parlio_rx_unit_handle_t rx_unit, const parlio_rx_event_data_t *edata, void *user_ctx)
{
    parlio_msg_t msg;
    BaseType_t xTaskWoken = 0;
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)user_ctx;

    spi_cam_receive_done(ctlr, edata->data, edata->recv_bytes);

    msg.type = PARLIO_MSG_FRAME_RECVED;
    if (xQueueSendFromISR(ctlr->parlio.ms_queue, &msg, &xTaskWoken) == pdPASS) {
        if (xTaskWoken) {
            portYIELD_FROM_ISR(xTaskWoken);
        }
    }

    return true;
}

/**
 * @brief Parlio valid GPIO ISR handler, this is used to notify the parlio valid signal level is 0 -> 1.
 *
 * @param arg ESP CAM controller handle
 *
 * @return None
 */
static void SPI_CAM_ISR_ATTR parlio_valid_gpio_isr_handler(void *arg)
{
    bool need_yield = false;
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)arg;

    parlio_rx_unit_trigger_fake_eof(ctlr->parlio.rx_unit, &need_yield);
}

/**
 * @brief Parlio message task, this is used to receive the parlio message and setup the SPI transaction buffer.
 *
 * @param arg ESP CAM controller handle
 *
 * @return None
 */
static void parlio_message_task(void *arg)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)arg;

    while (1) {
        parlio_msg_t msg;

        if (xQueueReceive(ctlr->parlio.ms_queue, &msg, portMAX_DELAY) == pdPASS) {
            if (msg.type == PARLIO_MSG_FRAME_RECVED) {
                setup_trans_buffer(ctlr, &ctlr->spi_trans);

                parlio_receive_config_t recv_cfg = {
                    .delimiter = ctlr->parlio.rx_delimiter,
                };

                esp_err_t ret = parlio_rx_unit_receive(ctlr->parlio.rx_unit, ctlr->spi_trans.rx_buffer, ctlr->fb_size_in_bytes, &recv_cfg);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "failed to receive parlio frame");
                }
            } else if (msg.type == PARLIO_MSG_EXIT) {
                ESP_LOGD(TAG, "parlio message task exit");
                break;
            } else {
                ESP_LOGE(TAG, "invalid parlio message type");
            }
        }
    }

    vTaskDelete(NULL);
}

/**
 * @brief Initialize parlio interface
 *
 * @param ctlr        ESP CAM controller handle
 * @param config      SPI controller configurations
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_init_intf_parlio(esp_cam_ctlr_spi_cam_t *ctlr, const esp_cam_ctlr_spi_config_t *config)
{
    esp_err_t ret;

    uint64_t pin_bit_mask = 1ULL << config->spi_sclk_pin |
                            1ULL << config->spi_cs_pin |
                            1ULL << config->spi_data0_io_pin;
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = pin_bit_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&gpio_cfg), TAG, "Failed to config the parlio pins");

    ret = gpio_install_isr_service(0);
    ESP_RETURN_ON_FALSE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE, ESP_FAIL, TAG, "failed to install the parlio valid GPIO ISR service");
    ESP_RETURN_ON_ERROR(gpio_set_intr_type(config->spi_cs_pin, GPIO_INTR_POSEDGE), TAG, "Failed to set the parlio valid GPIO ISR type");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(config->spi_cs_pin, parlio_valid_gpio_isr_handler, ctlr), TAG, "Failed to add the parlio valid GPIO ISR handler");

    ctlr->parlio.frame_size = config->frame_info->frame_size;

    parlio_rx_unit_config_t parlio_rx_unit_cfg = {
        .trans_queue_depth = 1,
        .max_recv_size = ctlr->fb_size_in_bytes,
        .data_width = 1,
        .clk_src = PARLIO_CLK_SRC_EXTERNAL,
        .ext_clk_freq_hz = 24 * 1000 * 1000,
        .exp_clk_freq_hz = 24 * 1000 * 1000,
        .clk_in_gpio_num = config->spi_sclk_pin,
        .clk_out_gpio_num = -1,
        .valid_gpio_num = config->spi_cs_pin,
        .data_gpio_nums = {
            config->spi_data0_io_pin
        },
    };

    // disable other data pins
    for (int i = 1; i < PARLIO_RX_UNIT_MAX_DATA_WIDTH; i++) {
        parlio_rx_unit_cfg.data_gpio_nums[i] = -1;
    }

    ESP_GOTO_ON_ERROR(parlio_new_rx_unit(&parlio_rx_unit_cfg, &ctlr->parlio.rx_unit), fail0, TAG, "Failed to allocate the parlio rx unit");

    parlio_rx_level_delimiter_config_t parlio_delimiter_cfg = {
        .valid_sig_line_id = PARLIO_RX_UNIT_MAX_DATA_WIDTH - 1,
        .sample_edge = PARLIO_SAMPLE_EDGE_POS,
        .bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB,
        .eof_data_len = 0,
        .timeout_ticks = 0,
        .flags = {
            .active_low_en = 1,
        }
    };
    ESP_GOTO_ON_ERROR(parlio_new_rx_level_delimiter(&parlio_delimiter_cfg, &ctlr->parlio.rx_delimiter), fail1, TAG, "Failed to set the parlio delimiter");

    parlio_rx_event_callbacks_t rx_cbs = {
        .on_receive_done = parlio_rx_done_callback,
    };
    ESP_GOTO_ON_ERROR(parlio_rx_unit_register_event_callbacks(ctlr->parlio.rx_unit, &rx_cbs, ctlr), fail2, TAG, "Failed to register the parlio event callbacks");

    ctlr->parlio.ms_queue = xQueueCreate(config->frame_buffer_count + 1, sizeof(parlio_msg_t));
    ESP_GOTO_ON_FALSE(ctlr->parlio.ms_queue, ESP_ERR_NO_MEM, fail2, TAG, "Failed to create the parlio message queue");

    ret = xTaskCreate(parlio_message_task, PARLIO_TASK_NAME_BASE, PARLIO_TASK_STACK_SIZE, ctlr, PARLIO_TASK_PRIORITY, &ctlr->parlio.ms_task);
    ESP_GOTO_ON_FALSE(ret == pdPASS, ESP_ERR_NO_MEM, fail3, TAG, "Failed to create the parlio message task");

    return ESP_OK;

fail3:
    vQueueDelete(ctlr->parlio.ms_queue);
    ctlr->parlio.ms_queue = NULL;
fail2:
    parlio_del_rx_delimiter(ctlr->parlio.rx_delimiter);
    ctlr->parlio.rx_delimiter = NULL;
fail1:
    parlio_del_rx_unit(ctlr->parlio.rx_unit);
    ctlr->parlio.rx_unit = NULL;
fail0:
    gpio_isr_handler_remove(ctlr->spi_cs_pin);
    ctlr->spi_cs_pin = -1;
    return ret;
}
#endif

/**
 * @brief Initialize SPI CAM interface
 *
 * @param ctlr ESP CAM controller handle
 * @param config SPI CAM controller configurations
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_init_intf(esp_cam_ctlr_spi_cam_t *ctlr, const esp_cam_ctlr_spi_config_t *config)
{
    if (ctlr->intf == ESP_CAM_CTLR_SPI_CAM_INTF_SPI) {
        ESP_LOGI(TAG, "Initializing SPI interface");
        return spi_cam_init_intf_spi(ctlr, config);
    } else {
#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
        ESP_LOGI(TAG, "Initializing Parallel I/O interface");
        return spi_cam_init_intf_parlio(ctlr, config);
#else
        ESP_LOGE(TAG, "PARLIO RX driver is not enabled");
        return ESP_FAIL;
#endif
    }
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
    if (ctlr->intf == ESP_CAM_CTLR_SPI_CAM_INTF_SPI) {
        ESP_RETURN_ON_ERROR(esp_cam_spi_slave_free(ctlr->spi.port), TAG, "failed to free SPI slave");
    } else {
#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
        ESP_RETURN_ON_ERROR(gpio_isr_handler_remove(ctlr->spi_cs_pin), TAG, "failed to remove the parlio valid GPIO ISR handler");

        ESP_RETURN_ON_ERROR(parlio_del_rx_delimiter(ctlr->parlio.rx_delimiter), TAG, "failed to delete the parlio delimiter");
        ctlr->parlio.rx_delimiter = NULL;

        ESP_RETURN_ON_ERROR(parlio_del_rx_unit(ctlr->parlio.rx_unit), TAG, "failed to delete the parlio RX unit");
        ctlr->parlio.rx_unit = NULL;

        parlio_msg_t msg = {
            .type = PARLIO_MSG_EXIT,
        };
        ESP_RETURN_ON_FALSE(xQueueSend(ctlr->parlio.ms_queue, &msg, portMAX_DELAY) == pdPASS, ESP_FAIL, TAG, "failed to send the parlio exit message");
        ctlr->parlio.ms_task = NULL;

        vQueueDelete(ctlr->parlio.ms_queue);
        ctlr->parlio.ms_queue = NULL;
#endif
    }

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
 * @param decoded_size Decoded size pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t spi_cam_decode(esp_cam_ctlr_spi_cam_t *ctlr, uint8_t *src, uint8_t *dst, uint32_t *decoded_size)
{
    bool decode_check_dis = ctlr->decode_check_dis;

    if (!decode_check_dis && (memcmp(src, ctlr->frame_info->frame_header_check, ctlr->frame_info->frame_header_check_size) != 0)) {
        ESP_LOGE(TAG, "invalid frame header: %x %x %x %x", src[0], src[1], src[2], src[3]);
        return ESP_FAIL;
    }
    src += ctlr->frame_info->frame_header_size;

    int line_data_size = ctlr->frame_info->line_size - ctlr->frame_info->line_header_size;

    for (uint32_t i = 0; i < ctlr->fb_lines; i++) {
        if (!decode_check_dis && (memcmp(src, ctlr->frame_info->line_header_check, ctlr->frame_info->line_header_check_size) != 0)) {
            // ESP_LOGE(TAG, "invalid line header %d", (int)i);
            // return ESP_FAIL;
        }
        src += ctlr->frame_info->line_header_size;

        memcpy(dst, src, line_data_size);

        src += line_data_size;
        dst += line_data_size;
    }

    *decoded_size = ctlr->bf_size_in_bytes;

    return ESP_OK;
}

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
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
                uint32_t decoded_size;

#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
                if (rx_buffer == ctlr->frame_buffer) {
                    ret = spi_cam_decode(ctlr, rx_buffer, ctlr->backup_buffer, &decoded_size);
                    decoded_buffer = ctlr->backup_buffer;
                } else
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
                {
                    ret = spi_cam_decode(ctlr, rx_buffer, rx_buffer, &decoded_size);
                    decoded_buffer = rx_buffer;
                }

                if (ret == ESP_OK) {
                    esp_cam_ctlr_trans_t trans = {0};

                    trans.buffer = decoded_buffer;
                    trans.buflen = decoded_size;
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
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */

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

    if (ctlr->intf == ESP_CAM_CTLR_SPI_CAM_INTF_SPI) {
        ret = esp_cam_spi_slave_enable(ctlr->spi.port);
    } else {
#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
        ret = gpio_intr_enable(ctlr->spi_cs_pin);
        if (ret == ESP_OK) {
            ret = parlio_rx_unit_enable(ctlr->parlio.rx_unit, true);
        }
#else
        ret = ESP_FAIL;
#endif
    }

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

    if (ctlr->intf == ESP_CAM_CTLR_SPI_CAM_INTF_SPI) {
        ret = esp_cam_spi_slave_disable(ctlr->spi.port);
    } else {
#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
        ret = parlio_rx_unit_disable(ctlr->parlio.rx_unit);
        if (ret == ESP_OK) {
            ret = gpio_intr_disable(ctlr->spi_cs_pin);
            if (ret == ESP_OK) {
                ret = parlio_rx_unit_wait_all_done(ctlr->parlio.rx_unit, 1000);
            }
        }
#else
        ret = ESP_FAIL;
#endif
    }

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

    ctlr->dropped_frame_count = 0;
    setup_trans_buffer(ctlr, &ctlr->spi_trans);
    if (ctlr->intf == ESP_CAM_CTLR_SPI_CAM_INTF_SPI) {
        ret = esp_cam_spi_slave_queue_trans(ctlr->spi.port, &ctlr->spi_trans, portMAX_DELAY);
    } else {
#if CONFIG_CAM_CTLR_SPI_ENABLE_PARLIO
        parlio_receive_config_t recv_cfg = {
            .delimiter = ctlr->parlio.rx_delimiter,
        };

        ret = parlio_rx_unit_receive(ctlr->parlio.rx_unit, ctlr->spi_trans.rx_buffer, ctlr->fb_size_in_bytes, &recv_cfg);
#else
        ret = ESP_FAIL;
#endif
    }

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
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE(ctlr->fsm == ESP_CAM_CTLR_SPI_CAM_FSM_STARTED, ESP_ERR_INVALID_STATE, TAG, "processor isn't in started state");

    /* SPI slave does not support stop function */

    ctlr->fsm = ESP_CAM_CTLR_SPI_CAM_FSM_ENABLED;

    return ESP_OK;
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
#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
    ESP_RETURN_ON_FALSE(cbs->on_get_new_trans || !ctlr->bk_buffer_dis, ESP_ERR_INVALID_ARG, TAG, "invalid argument: on_get_new_trans is null");
#else /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
    ESP_RETURN_ON_FALSE(cbs->on_get_new_trans, ESP_ERR_INVALID_ARG, TAG, "invalid argument: on_get_new_trans is null");
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
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
#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");
    ESP_RETURN_ON_FALSE(fb_num && fb_num <= 1, ESP_ERR_INVALID_ARG, TAG, "invalid frame buffer number");
    ESP_RETURN_ON_FALSE((ctlr->fsm >= ESP_CAM_CTLR_SPI_CAM_FSM_INIT) && (!ctlr->bk_buffer_dis), ESP_ERR_INVALID_STATE, TAG, "driver don't initialized or back_buffer not available");

    va_list args;
    const void **fb_itor = fb0;

    va_start(args, fb0);
    for (uint32_t i = 0; i < fb_num; i++) {
        if (fb_itor) {
#if CAM_CTLR_SPI_HAS_AUTO_DECODE
            *fb_itor = ctlr->auto_decode_dis ? ctlr->frame_buffer : ctlr->backup_buffer;
#else /* CAM_CTLR_SPI_HAS_AUTO_DECODE */
            *fb_itor = ctlr->frame_buffer;
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */

            fb_itor = va_arg(args, const void **);
        }
    }
    va_end(args);

    ctlr->bk_buffer_exposed = true;

    return ESP_OK;
#else /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
    ESP_LOGD(TAG, "backup buffer is disabled");

    return ESP_ERR_NOT_SUPPORTED;
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
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
#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER

    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr && ret_fb_len, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle or ret_fb_len is null");
    ESP_RETURN_ON_FALSE((ctlr->fsm >= ESP_CAM_CTLR_SPI_CAM_FSM_INIT) && (!ctlr->bk_buffer_dis), ESP_ERR_INVALID_STATE, TAG, "driver don't initialized or back_buffer not available");

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
    *ret_fb_len = ctlr->auto_decode_dis ? ctlr->fb_size_in_bytes : ctlr->bf_size_in_bytes;
#else /* CAM_CTLR_SPI_HAS_AUTO_DECODE */
    *ret_fb_len = ctlr->fb_size_in_bytes;
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */

    return ESP_OK;
#else /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
    ESP_LOGD(TAG, "backup buffer is disabled");

    return ESP_ERR_NOT_SUPPORTED;
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
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

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
    if (!ctlr->auto_decode_dis) {
        vTaskDelete(ctlr->spi_task_handle);
        vQueueDelete(ctlr->spi_recv_queue);
    }
#endif

    spi_cam_deinit_intf(ctlr);

#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
    if (!ctlr->bk_buffer_dis) {
#if CAM_CTLR_SPI_HAS_AUTO_DECODE
        if (!ctlr->auto_decode_dis) {
            heap_caps_free(ctlr->backup_buffer);
        }
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */

        heap_caps_free(ctlr->frame_buffer);
    }
#else /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
    heap_caps_free(ctlr->spi_ll_buffer);
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */

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
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0))
    if (config->intf == ESP_CAM_CTLR_SPI_CAM_INTF_PARLIO) {
        ESP_LOGD(TAG, "Parallel I/O for SPI camera sensor only supported on IDF version v5.5 and later");
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif

    esp_err_t ret;
    size_t alignment_size;
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: config or ret_handle is null");
    ESP_RETURN_ON_FALSE(config->frame_info, ESP_ERR_INVALID_ARG, TAG, "invalid argument: frame_info is null");

#if CONFIG_SPIRAM
    ESP_RETURN_ON_ERROR(esp_cache_get_alignment(SPI_MEM_CAPS, &alignment_size), TAG, "failed to get cache alignment");
#else
    alignment_size = 4;
#endif

    esp_cam_ctlr_spi_cam_t *ctlr = heap_caps_calloc(1, sizeof(esp_cam_ctlr_spi_cam_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_NO_MEM, TAG, "no mem for CAM SPI controller context");

    ESP_LOGD(TAG, "frame_size=%d, alignment_size=%d", (int)config->frame_info->frame_size, (int)alignment_size);

    ctlr->frame_info = config->frame_info;
    ctlr->fb_lines = config->v_res;
    ctlr->fb_size_in_bytes = ALIGN_UP_BY(config->frame_info->frame_size, alignment_size);
    ctlr->bf_size_in_bytes = config->frame_info->frame_size - config->frame_info->frame_header_size - config->frame_info->line_header_size * config->v_res;
    ctlr->drop_frame_count = config->frame_info->drop_frame_count;

#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
    ctlr->bk_buffer_dis = config->bk_buffer_dis;
#endif

#if CONFIG_SPIRAM
    ctlr->bk_buffer_sram = config->bk_buffer_sram;
#endif

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
    ctlr->auto_decode_dis = config->auto_decode_dis;
#endif

    ctlr->fsm = ESP_CAM_CTLR_SPI_CAM_FSM_INIT;

    ESP_LOGD(TAG, "fb_size_in_bytes: %" PRIu32 ", bf_size_in_bytes: %" PRIu32, ctlr->fb_size_in_bytes, ctlr->bf_size_in_bytes);

#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
    if (!config->bk_buffer_dis) {
        uint32_t heap_cap = MALLOC_CAP_8BIT;

#if CONFIG_SPIRAM
        if (!config->bk_buffer_sram) {
            heap_cap |= MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED;
        } else {
            heap_cap |= MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
        }
#else /* CONFIG_SPIRAM */
        heap_cap |= MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
#endif /* CONFIG_SPIRAM */
        ESP_LOGD(TAG, "free memory: %d, frame buffer size: %d", (int)heap_caps_get_free_size(heap_cap), (int)ctlr->fb_size_in_bytes);
        ctlr->frame_buffer = heap_caps_calloc(1, ctlr->fb_size_in_bytes, heap_cap);
        ESP_GOTO_ON_FALSE(ctlr->frame_buffer, ESP_ERR_NO_MEM, fail0, TAG, "no mem for SPI frame buffer");

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
        if (!ctlr->auto_decode_dis) {
            ctlr->backup_buffer = heap_caps_calloc(1, ctlr->bf_size_in_bytes, heap_cap);
            ESP_GOTO_ON_FALSE(ctlr->backup_buffer, ESP_ERR_NO_MEM, fail1, TAG, "no mem for SPI backup buffer");
        }
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */
    }
#else /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
    if (config->intf == ESP_CAM_CTLR_SPI_CAM_INTF_PARLIO) {
        ctlr->spi_ll_buffer_size = ctlr->frame_info->frame_size;
    } else {
        ctlr->spi_ll_buffer_size = alignment_size;
    }
    ctlr->spi_ll_buffer = heap_caps_aligned_alloc(alignment_size, ctlr->spi_ll_buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    ESP_GOTO_ON_FALSE(ctlr->spi_ll_buffer, ESP_ERR_NO_MEM, fail0, TAG, "no mem for SPI low level buffer");
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */

    ctlr->spi.port = config->spi_port;
    ctlr->intf = config->intf;
    ctlr->spi_cs_pin = config->spi_cs_pin;
    ESP_GOTO_ON_ERROR(spi_cam_init_intf(ctlr, config), fail2, TAG, "failed to initialize SPI interface");

    ctlr->base.del = spi_cam_del;
    ctlr->base.enable = spi_cam_enable;
    ctlr->base.start = spi_cam_start;
    ctlr->base.stop = spi_cam_stop;
    ctlr->base.disable = spi_cam_disable;
    ctlr->base.register_event_callbacks = spi_cam_register_event_callbacks;
    ctlr->base.get_internal_buffer = spi_cam_get_internal_buffer;
    ctlr->base.get_buffer_len = spi_cam_get_frame_buffer_len;

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
    if (!ctlr->auto_decode_dis) {
        char name[12];

        ret = snprintf(name, sizeof(name), SPI_TASK_NAME_BASE, config->spi_port);
        assert(ret > 0 && ret < sizeof(name));

        ctlr->spi_recv_queue = xQueueCreate(config->frame_buffer_count + 1, sizeof(spi_cam_msg_t));
        ESP_GOTO_ON_FALSE(ctlr->spi_recv_queue, ESP_ERR_NO_MEM, fail3, TAG, "failed to create SPI receive queue");

        BaseType_t os_ret = xTaskCreate(spi_cam_task, name, SPI_TASK_STACK_SIZE, ctlr, SPI_TASK_PRIORITY, &ctlr->spi_task_handle);
        ESP_GOTO_ON_FALSE(os_ret == pdPASS, ESP_ERR_NO_MEM, fail4, TAG, "failed to create SPI task");
    }
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */

    *ret_handle = &ctlr->base;

    return ESP_OK;

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
fail4:
    vQueueDelete(ctlr->spi_recv_queue);
fail3:
    spi_cam_deinit_intf(ctlr);
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */
fail2:
#if CAM_CTLR_SPI_HAS_BACKUP_BUFFER
#if CAM_CTLR_SPI_HAS_AUTO_DECODE
    heap_caps_free(ctlr->backup_buffer);
fail1:
#endif /* CAM_CTLR_SPI_HAS_AUTO_DECODE */
    heap_caps_free(ctlr->frame_buffer);
#else /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
    heap_caps_free(ctlr->spi_ll_buffer);
#endif /* CAM_CTLR_SPI_HAS_BACKUP_BUFFER */
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
 * @param decoded_size Decoded size pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_cam_spi_decode_frame(esp_cam_ctlr_handle_t handle, uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len, uint32_t *decoded_size)
{
    esp_cam_ctlr_spi_cam_t *ctlr = (esp_cam_ctlr_spi_cam_t *)handle;
    ESP_RETURN_ON_FALSE(ctlr, ESP_ERR_INVALID_ARG, TAG, "invalid argument: handle is null");

#if CAM_CTLR_SPI_HAS_AUTO_DECODE
    ESP_RETURN_ON_FALSE(ctlr->auto_decode_dis, ESP_ERR_INVALID_STATE, TAG, "auto decode is enabled");
#endif

    ESP_RETURN_ON_FALSE(src && dst, ESP_ERR_INVALID_ARG, TAG, "invalid argument: src or dst is null");
    ESP_RETURN_ON_FALSE(src_len == ctlr->fb_size_in_bytes && dst_len >= ctlr->bf_size_in_bytes, ESP_ERR_INVALID_ARG, TAG, "invalid argument: src_len or dst_len is invalid");

    return spi_cam_decode(ctlr, src, dst, decoded_size);
}
