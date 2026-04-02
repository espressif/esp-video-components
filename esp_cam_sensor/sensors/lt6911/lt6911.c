/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "lt6911_settings.h"
#include "lt6911.h"

#define LT6911_IO_MUX_LOCK(mux)
#define LT6911_IO_MUX_UNLOCK(mux)
#define LT6911_ENABLE_OUT_XCLK(pin,clk)
#define LT6911_DISABLE_OUT_XCLK(pin)
#define LT6911_DEBUG_EN (0)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

static const char *TAG = "lt6911";

#ifndef CONFIG_CAMERA_LT6911_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for LT6911"
#endif

static const uint8_t lt6911_format_default_index = CONFIG_CAMERA_LT6911_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t lt6911_format_index[] = {
#if CONFIG_CAMERA_LT6911_MIPI_YUV422_1280X720_60FPS
    0,
#endif
#if CONFIG_CAMERA_LT6911_MIPI_YUV422_1920X1080_48FPS
    1,
#endif
};

static const esp_cam_sensor_format_t lt6911_format_info[] = {
#if CONFIG_CAMERA_LT6911_MIPI_YUV422_1280X720_60FPS
    {
        .name = "MIPI_2lane_24Minput_YUV422_1280x720_60fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = lt6911_mipi_2lane_24Minput_1280x720_yuv422_60fps,
        .regs_size = ARRAY_SIZE(lt6911_mipi_2lane_24Minput_1280x720_yuv422_60fps),
        .fps = 60,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = 357000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_LT6911_MIPI_YUV422_1920X1080_48FPS
    {
        .name = "MIPI_2lane_24Minput_YUV422_1920x1080_48fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = lt6911_mipi_2lane_24Minput_1920x1080_yuv422_48fps,
        .regs_size = ARRAY_SIZE(lt6911_mipi_2lane_24Minput_1920x1080_yuv422_48fps),
        .fps = 48,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = 654000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static uint8_t get_lt6911_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(lt6911_format_index); i++) {
        if (lt6911_format_index[i] == lt6911_format_default_index) {
            return i;
        }
    }

    return 0;
}

static esp_err_t lt6911_read(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, read_buf);
}

static esp_err_t lt6911_write(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a8v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t lt6911_write_array(esp_sccb_io_handle_t sccb_handle, lt6911_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && (i < regs_size)) {
        if (regarray[i].reg != LT6911_REG_DELAY) {
            ret = lt6911_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t lt6911_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t lt6911_soft_reset(esp_cam_sensor_device_t *dev)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lt6911_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;
#if LT6911_DEBUG_EN
    ret = lt6911_write(dev->sccb_handle, 0xFF, 0xE0); // set bank to 0xe0
    // MIPI TX status
    uint8_t temp_val = 0;
    // lane status: 1,2,4,8
    lt6911_read(dev->sccb_handle, 0x95, &temp_val);
    ESP_LOGI(TAG, "[0x95] MIPI lanes     = 0x%02X (%d lanes)", temp_val, temp_val & 0x0F);
    // stream status: 0x01: enable, 0x00: disable
    lt6911_read(dev->sccb_handle, 0xB0, &temp_val);
    ESP_LOGI(TAG, "[0xB0] MIPI TX ctrl   = 0x%02X", temp_val);
    // format status
    /* 0x96 states: 0x00: RGB_6Bit, 0x01: RGB_8Bit, 0x02: RGB_10Bit, 0x03: RGB_12Bit
    0x04: YUV444_8Bit, 0x05: YUV444_10Bit, 0x06: YUV444_12Bit, 0x07: YUV422_8Bit
    0x08: YUV422_10Bit, 0x09:YUV422_12Bit, 0x0a: YUV420_8Bit, 0x0b: YUV420_10Bit
    0x0c: YUV420_12Bit*/
    lt6911_read(dev->sccb_handle, 0x96, &temp_val);
    ESP_LOGI(TAG, "[0x96] MIPI Format  = 0x%02X", temp_val);
    // interrupt status
    lt6911_read(dev->sccb_handle, 0x84, &temp_val); // 0x00: video disappear, 0x01: video ready, 0x02: audio disappear, 0x03: audio ready
    ESP_LOGI(TAG, "[0x84] interrupt type  = 0x%02X", temp_val);
    // MIPI Clock status
    uint8_t bc_h, bc_m, bc_l;
    lt6911_read(dev->sccb_handle, 0x92, &bc_h);
    lt6911_read(dev->sccb_handle, 0x93, &bc_m);
    lt6911_read(dev->sccb_handle, 0x94, &bc_l);
    uint32_t byte_clock_khz      = ((uint32_t)bc_h << 16) | ((uint32_t)bc_m << 8) | bc_l;
    uint32_t mipi_data_rate_mbps = (byte_clock_khz * 8) / 1000;
    ESP_LOGI(TAG, "ByteClock = %lu KHz, MIPI data rate = %lu Mbps", byte_clock_khz, mipi_data_rate_mbps);
    // Size status
    uint8_t vtotal_h, vtotal_l, half_hactive_h, half_hactive_l, vactive_h, vactive_l;
    lt6911_read(dev->sccb_handle, 0x8A, &vtotal_h);
    lt6911_read(dev->sccb_handle, 0x8B, &vtotal_l);
    lt6911_read(dev->sccb_handle, 0x8C, &half_hactive_h);
    lt6911_read(dev->sccb_handle, 0x8D, &half_hactive_l);
    lt6911_read(dev->sccb_handle, 0x8E, &vactive_h);
    lt6911_read(dev->sccb_handle, 0x8F, &vactive_l);
    uint16_t vtotal  = ((uint16_t)vtotal_h << 8) | vtotal_l;
    uint16_t hactive = (((uint16_t)half_hactive_h << 8) | half_hactive_l) * 2;  // half_hactive is half of hactive
    uint16_t vactive = ((uint16_t)vactive_h << 8) | vactive_l;
    ESP_LOGI(TAG, "VTotal = %d, HActive = %d, VActive = %d", vtotal, hactive, vactive);

    // Ensure MIPI stream is enabled (do NOT disable/re-enable — LT6911D may fail to restart)
    lt6911_read(dev->sccb_handle, 0xB0, &temp_val);
    if (temp_val != 0x01) {
        ESP_LOGD(TAG, "MIPI stream not active (0xB0=0x%02X), enabling...", temp_val);
        temp_val = 0x01;
        ret = lt6911_write(dev->sccb_handle, 0xB0, temp_val);
        vTaskDelay(pdMS_TO_TICKS(200));
        lt6911_read(dev->sccb_handle, 0xB0, &temp_val);
        ESP_LOGI(TAG, "[0xB0] MIPI Stream enable, readback = 0x%02X", temp_val);
    } else {
        ESP_LOGI(TAG, "[0xB0] MIPI Stream already active = 0x%02X", temp_val);
    }
    ESP_LOGI(TAG, "Initializing HDMI-CSI to DSI display START");
#endif
    ret = lt6911_write(dev->sccb_handle, 0xFF, 0xE0);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = lt6911_write(dev->sccb_handle, 0xEE, 0x01);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = lt6911_write(dev->sccb_handle, 0xFF, 0xE1);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = lt6911_read(dev->sccb_handle, LT6911_REG_CHIP_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = lt6911_read(dev->sccb_handle, LT6911_REG_CHIP_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = lt6911_write(dev->sccb_handle, 0xFF, 0xE0);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = lt6911_write(dev->sccb_handle, 0xEE, 0x00);
    if (ret != ESP_OK) {
        return ret;
    }

    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t lt6911_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
#if LT6911_DEBUG_EN
    // Verify LT6911D state after CSI start
    uint8_t temp_val = 0;
    lt6911_write(dev->sccb_handle, 0xFF, 0xE0);
    lt6911_read(dev->sccb_handle, 0x84, &temp_val);
    ESP_LOGI(TAG, "[0x84] interrupt type after CSI start = 0x%02X (0x01=video ready)", temp_val);

    lt6911_read(dev->sccb_handle, 0xB0, &temp_val);
    ESP_LOGI(TAG, "[0xB0] MIPI Stream = 0x%02X", temp_val);

    lt6911_read(dev->sccb_handle, 0x96, &temp_val);
    ESP_LOGI(TAG, "[0x96] MIPI Format = 0x%02X (0x07=YUV422_8bit)", temp_val);

    // MIPI Clock status
    uint8_t bc_h, bc_m, bc_l;
    lt6911_read(dev->sccb_handle, 0x92, &bc_h);
    lt6911_read(dev->sccb_handle, 0x93, &bc_m);
    lt6911_read(dev->sccb_handle, 0x94, &bc_l);
    uint32_t byte_clock_khz      = ((uint32_t)bc_h << 16) | ((uint32_t)bc_m << 8) | bc_l;
    uint32_t mipi_data_rate_mbps = (byte_clock_khz * 8) / 1000;
    ESP_LOGI(TAG, "ByteClock = %lu KHz, MIPI data rate = %lu Mbps", byte_clock_khz, mipi_data_rate_mbps);
#endif
    // Check LT6911D status — PHY reset may have disrupted the TX side
    esp_err_t ret = lt6911_write(dev->sccb_handle, 0xFF, 0xE0);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t lt_b0 = 0;
    ret = lt6911_read(dev->sccb_handle, 0xB0, &lt_b0);
    if (ret != ESP_OK) {
        return ret;
    }

    bool en = (enable != 0) ? true : false;

    if (en && lt_b0 != 0x01) {
        ESP_LOGD(TAG, "LT6911D MIPI TX not active! Attempting re-enable...");
        lt_b0 = 0x01;
        ret = lt6911_write(dev->sccb_handle, 0xFF, 0xE0);
        if (ret == ESP_OK) {
            ret = lt6911_write(dev->sccb_handle, 0xB0, lt_b0);
        }
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        ret = lt6911_read(dev->sccb_handle, 0xB0, &lt_b0);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    if (!en && lt_b0 != 0x00) {
        ESP_LOGD(TAG, "LT6911D MIPI TX active! Attempting disable...");
        lt_b0 = 0x00;
        ret = lt6911_write(dev->sccb_handle, 0xFF, 0xE0);
        if (ret == ESP_OK) {
            ret = lt6911_write(dev->sccb_handle, 0xB0, lt_b0);
        }
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        ret = lt6911_read(dev->sccb_handle, 0xB0, &lt_b0);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ESP_LOGI(TAG, "LT6911D B0 readback = 0x%02X", lt_b0);
    dev->stream_status = en;
    return ESP_OK;
}

static esp_err_t lt6911_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_DATA_SEQ:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_U8;
        qdesc->u8.size = sizeof(uint32_t);
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t lt6911_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_DATA_SEQ:
        if (dev->cur_format->port == ESP_CAM_SENSOR_MIPI_CSI) {
            *(uint32_t *)arg = ESP_CAM_SENSOR_DATA_SEQ_SHORT_SWAPPED;
        }
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t lt6911_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lt6911_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(lt6911_format_info);
    formats->format_array = &lt6911_format_info[0];
    return ESP_OK;
}

static esp_err_t lt6911_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_yuv = 1;
    return ESP_OK;
}

static esp_err_t lt6911_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &lt6911_format_info[get_lt6911_actual_format_index()];
    }

    ret = lt6911_write_array(dev->sccb_handle, (lt6911_reginfo_t *)format->regs, format->regs_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;

    return ret;
}

static esp_err_t lt6911_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, format);

    esp_err_t ret = ESP_FAIL;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(esp_cam_sensor_format_t));
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t lt6911_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    LT6911_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = lt6911_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = lt6911_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = lt6911_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = lt6911_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = lt6911_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = lt6911_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    LT6911_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t lt6911_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        LT6911_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
        delay_ms(6);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "pwdn pin config failed");
            return ret;
        }

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "reset pin config failed");
            return ret;
        }

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t lt6911_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        LT6911_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t lt6911_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del lt6911 (%p)", dev);
    if (dev) {
        lt6911_power_off(dev);
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t lt6911_ops = {
    .query_para_desc = lt6911_query_para_desc,
    .get_para_value = lt6911_get_para_value,
    .set_para_value = lt6911_set_para_value,
    .query_support_formats = lt6911_query_support_formats,
    .query_support_capability = lt6911_query_support_capability,
    .set_format = lt6911_set_format,
    .get_format = lt6911_get_format,
    .priv_ioctl = lt6911_priv_ioctl,
    .del = lt6911_delete
};

esp_cam_sensor_device_t *lt6911_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    dev->name = (char *)LT6911_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &lt6911_ops;
    dev->cur_format = &lt6911_format_info[get_lt6911_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (lt6911_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (lt6911_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != LT6911_PID) {
        ESP_LOGE(TAG, "Camera sensor is not LT6911, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    lt6911_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_LT6911_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(lt6911_detect, ESP_CAM_SENSOR_MIPI_CSI, LT6911_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return lt6911_detect(config);
}
#endif
