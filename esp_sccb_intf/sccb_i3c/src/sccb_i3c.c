/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <esp_types.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "driver/i3c_master_i2c.h"
#include "esp_sccb_types.h"
#include "esp_sccb_i3c.h"
#include "sccb_i3c_internal.h"
#include "esp_sccb_io_interface.h"

#define SCCB_I3C_MEM_CAPS       MALLOC_CAP_DEFAULT
#define SCCB_I3C_DMA_ALIGN      4
#define SCCB_I3C_DMA_CAPS       (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)

static const char *TAG = "sccb_i3c";

static esp_err_t s_sccb_i3c_transmit_reg_a8v8(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_reg_a16v8(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_reg_a8v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_reg_a16v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_reg_a16v32(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_reg_a16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_receive_reg_a8v8(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_receive_reg_a16v8(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_receive_reg_a8v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_receive_reg_a16v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_receive_reg_a16v32(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_receive_reg_a16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_transmit_v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_receive_v16(esp_sccb_io_t *io_handle, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
static esp_err_t s_sccb_i3c_destroy(esp_sccb_io_t *io_handle);

static size_t s_sccb_i3c_read_align(void)
{
    static size_t align;

    if (align == 0) {
        uint8_t *probe = heap_caps_malloc(SCCB_I3C_DMA_ALIGN, SCCB_I3C_DMA_CAPS);
        size_t line = probe ? esp_cache_get_line_size_by_addr(probe) : 0;

        heap_caps_free(probe);
        align = (line < SCCB_I3C_DMA_ALIGN) ? SCCB_I3C_DMA_ALIGN : line;
    }
    return align;
}

static void s_sccb_i3c_free_io(sccb_io_i3c_t *io_i3c)
{
    if (io_i3c->i3c_device) {
        i3c_master_bus_rm_i2c_device(io_i3c->i3c_device);
    }
    if (io_i3c->lock) {
        vSemaphoreDelete(io_i3c->lock);
    }
    heap_caps_free(io_i3c->tx_buf);
    heap_caps_free(io_i3c->rx_buf);
    heap_caps_free(io_i3c);
}

static esp_err_t s_sccb_i3c_stage_write(sccb_io_i3c_t *io_i3c, const uint8_t *write_buffer, size_t write_size, const uint8_t **staged)
{
    size_t cap = (write_size + SCCB_I3C_DMA_ALIGN - 1) & ~(size_t)(SCCB_I3C_DMA_ALIGN - 1);

    if (cap < write_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (io_i3c->tx_buf_cap < cap) {
        uint8_t *buf = heap_caps_aligned_calloc(SCCB_I3C_DMA_ALIGN, 1, cap, SCCB_I3C_DMA_CAPS);
        if (!buf) {
            return ESP_ERR_NO_MEM;
        }
        heap_caps_free(io_i3c->tx_buf);
        io_i3c->tx_buf = buf;
        io_i3c->tx_buf_cap = cap;
    }
    memcpy(io_i3c->tx_buf, write_buffer, write_size);
    if (cap > write_size) {
        memset(io_i3c->tx_buf + write_size, 0, cap - write_size);
    }
    *staged = io_i3c->tx_buf;
    return ESP_OK;
}

static esp_err_t s_sccb_i3c_stage_read(sccb_io_i3c_t *io_i3c, size_t read_size, uint8_t **staged)
{
    size_t align = s_sccb_i3c_read_align();
    size_t cap = (read_size + align - 1) & ~(align - 1);

    if (cap < read_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (io_i3c->rx_buf_cap < cap) {
        uint8_t *buf = heap_caps_aligned_calloc(align, 1, cap, SCCB_I3C_DMA_CAPS);
        if (!buf) {
            return ESP_ERR_NO_MEM;
        }
        heap_caps_free(io_i3c->rx_buf);
        io_i3c->rx_buf = buf;
        io_i3c->rx_buf_cap = cap;
    }
    *staged = io_i3c->rx_buf;
    return ESP_OK;
}

static esp_err_t s_sccb_i3c_transmit(sccb_io_i3c_t *io_i3c, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    const uint8_t *staged = NULL;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(io_i3c->lock, portMAX_DELAY);
    ESP_GOTO_ON_ERROR(s_sccb_i3c_stage_write(io_i3c, write_buffer, write_size, &staged), out, TAG, "no mem for i3c write buffer");
    ESP_GOTO_ON_ERROR(i3c_master_i2c_device_transmit(io_i3c->i3c_device, staged, write_size, xfer_timeout_ms), out, TAG, "failed to i3c transmit");

out:
    xSemaphoreGive(io_i3c->lock);
    return ret;
}

static esp_err_t s_sccb_i3c_transmit_receive(sccb_io_i3c_t *io_i3c, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    const uint8_t *tx = NULL;
    uint8_t *rx = NULL;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(io_i3c->lock, portMAX_DELAY);
    ESP_GOTO_ON_ERROR(s_sccb_i3c_stage_write(io_i3c, write_buffer, write_size, &tx), out, TAG, "no mem for i3c write buffer");
    ESP_GOTO_ON_ERROR(s_sccb_i3c_stage_read(io_i3c, read_size, &rx), out, TAG, "no mem for i3c read buffer");
    ESP_GOTO_ON_ERROR(i3c_master_i2c_device_transmit_receive(io_i3c->i3c_device, tx, write_size, rx, read_size, xfer_timeout_ms), out, TAG, "failed to i3c transmit receive");
    memcpy(read_buffer, rx, read_size);

out:
    xSemaphoreGive(io_i3c->lock);
    return ret;
}

static esp_err_t s_sccb_i3c_receive(sccb_io_i3c_t *io_i3c, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    uint8_t *staged = NULL;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(io_i3c->lock, portMAX_DELAY);
    ESP_GOTO_ON_ERROR(s_sccb_i3c_stage_read(io_i3c, read_size, &staged), out, TAG, "no mem for i3c read buffer");
    ESP_GOTO_ON_ERROR(i3c_master_i2c_device_receive(io_i3c->i3c_device, staged, read_size, xfer_timeout_ms), out, TAG, "failed to i3c receive");
    memcpy(read_buffer, staged, read_size);

out:
    xSemaphoreGive(io_i3c->lock);
    return ret;
}

esp_err_t sccb_new_i3c_io(i3c_master_bus_handle_t bus_handle, const sccb_i3c_config_t *config, esp_sccb_io_handle_t *io_handle)
{
    esp_err_t ret = ESP_FAIL;
    ESP_RETURN_ON_FALSE(bus_handle && config && io_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(config->device_address <= 0x7F, ESP_ERR_INVALID_ARG, TAG, "I3C only supports 7-bit address");

    sccb_io_i3c_t *io_i3c = heap_caps_calloc(1, sizeof(sccb_io_i3c_t), SCCB_I3C_MEM_CAPS);
    ESP_RETURN_ON_FALSE(io_i3c, ESP_ERR_NO_MEM, TAG, "no mem for io handle");

    i3c_device_i2c_config_t dev_cfg = {
        .device_address = config->device_address,
        .scl_freq_hz = config->scl_speed_hz,
    };
    i3c_master_i2c_device_handle_t dev_handle = NULL;
    ESP_GOTO_ON_ERROR(i3c_master_bus_add_i2c_device(bus_handle, &dev_cfg, &dev_handle), err, TAG, "failed to add i2c device on i3c bus");

    io_i3c->i3c_device = dev_handle;
    io_i3c->lock = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(io_i3c->lock, ESP_ERR_NO_MEM, err, TAG, "no mem for i3c lock");

    size_t bounce_size = s_sccb_i3c_read_align();
    io_i3c->tx_buf = heap_caps_aligned_calloc(SCCB_I3C_DMA_ALIGN, 1, bounce_size, SCCB_I3C_DMA_CAPS);
    io_i3c->rx_buf = heap_caps_aligned_calloc(bounce_size, 1, bounce_size, SCCB_I3C_DMA_CAPS);
    ESP_GOTO_ON_FALSE(io_i3c->tx_buf && io_i3c->rx_buf, ESP_ERR_NO_MEM, err, TAG, "no mem for i3c dma buffer");
    io_i3c->tx_buf_cap = bounce_size;
    io_i3c->rx_buf_cap = bounce_size;

    i3c_master_i2c_event_callbacks_t cbs = { 0 };
    esp_err_t cb_ret = i3c_master_i2c_device_register_event_callbacks(dev_handle, &cbs, NULL);
    if (cb_ret == ESP_OK) {
        ret = ESP_ERR_NOT_SUPPORTED;
        ESP_LOGE(TAG, "only a synchronous I3C bus is supported");
        goto err;
    } else if (cb_ret != ESP_ERR_INVALID_STATE) {
        ret = cb_ret;
        ESP_LOGE(TAG, "failed to register i3c callback");
        goto err;
    }

    io_i3c->base.transmit_reg_a8v8 = s_sccb_i3c_transmit_reg_a8v8;
    io_i3c->base.transmit_reg_a16v8 = s_sccb_i3c_transmit_reg_a16v8;
    io_i3c->base.transmit_reg_a8v16 = s_sccb_i3c_transmit_reg_a8v16;
    io_i3c->base.transmit_reg_a16v16 = s_sccb_i3c_transmit_reg_a16v16;
    io_i3c->base.transmit_reg_a16v32 = s_sccb_i3c_transmit_reg_a16v32;
    io_i3c->base.transmit_reg_a16 = s_sccb_i3c_transmit_reg_a16;
    io_i3c->base.transmit_receive_reg_a8v8 = s_sccb_i3c_transmit_receive_reg_a8v8;
    io_i3c->base.transmit_receive_reg_a16v8 = s_sccb_i3c_transmit_receive_reg_a16v8;
    io_i3c->base.transmit_receive_reg_a8v16 = s_sccb_i3c_transmit_receive_reg_a8v16;
    io_i3c->base.transmit_receive_reg_a16v16 = s_sccb_i3c_transmit_receive_reg_a16v16;
    io_i3c->base.transmit_receive_reg_a16v32 = s_sccb_i3c_transmit_receive_reg_a16v32;
    io_i3c->base.transmit_receive_reg_a16 = s_sccb_i3c_transmit_receive_reg_a16;
    io_i3c->base.transmit_v16 = s_sccb_i3c_transmit_v16;
    io_i3c->base.receive_v16 = s_sccb_i3c_receive_v16;
    io_i3c->base.del = s_sccb_i3c_destroy;
    *io_handle = &(io_i3c->base);
    ESP_LOGD(TAG, "new io_i3c: %p", io_i3c);
    return ESP_OK;

err:
    s_sccb_i3c_free_io(io_i3c);
    return ret;
}

static esp_err_t s_sccb_i3c_transmit_reg_a8v8(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_reg_a16v8(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_reg_a8v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_reg_a16v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_reg_a16v32(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_reg_a16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_receive_reg_a8v8(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit_receive(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, read_buffer, read_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_receive_reg_a16v8(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit_receive(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, read_buffer, read_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_receive_reg_a8v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit_receive(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, read_buffer, read_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_receive_reg_a16v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit_receive(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, read_buffer, read_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_receive_reg_a16v32(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit_receive(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, read_buffer, read_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_receive_reg_a16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit_receive(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, read_buffer, read_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_transmit_v16(esp_sccb_io_t *io_handle, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_transmit(__containerof(io_handle, sccb_io_i3c_t, base), write_buffer, write_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_receive_v16(esp_sccb_io_t *io_handle, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms)
{
    return s_sccb_i3c_receive(__containerof(io_handle, sccb_io_i3c_t, base), read_buffer, read_size, xfer_timeout_ms);
}

static esp_err_t s_sccb_i3c_destroy(esp_sccb_io_t *io_handle)
{
    sccb_io_i3c_t *io_i3c = __containerof(io_handle, sccb_io_i3c_t, base);

    /*
     * Caller contract (esp_sccb_del_i3c_io): no concurrent IO on this handle.
     * Do not take io_i3c->lock here — FreeRTOS forbids deleting a mutex that is
     * still held, and an object-local refcount cannot protect a thread that has
     * entered transmit but not yet completed Take against a concurrent free.
     */
    esp_err_t ret = i3c_master_bus_rm_i2c_device(io_i3c->i3c_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to remove i2c device from i3c bus");
        return ret;
    }
    io_i3c->i3c_device = NULL;
    s_sccb_i3c_free_io(io_i3c);
    return ESP_OK;
}
