/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "gc0308_settings.h"
#include "sccb.h"
#include "esp_log.h"

static const char *TAG = "gc0308";

#define GC0308_IO_MUX_LOCK()
#define GC0308_IO_MUX_UNLOCK()
#define GC0308_SCCB_ADDR   0x21
#define GC0308_PID         0x9b
#define GC0308_SENSOR_NAME "GC0308"
#define GC0308_SUPPORT_NUM CONFIG_CAMERA_GC0308_MAX_SUPPORT

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

#define CAMERA_ENABLE_OUT_CLOCK(pin,clk)
#define CAMERA_DISABLE_OUT_CLOCK()

static esp_camera_device_t *s_gc0308[GC0308_SUPPORT_NUM];

static uint8_t s_gc0308_index;

static int gc0308_read(uint8_t sccb_port, uint8_t reg, uint8_t *read_buf)
{
    int value = -1;
    value = sccb_read_reg8_val8(sccb_port, GC0308_SCCB_ADDR, reg);
    if (value == -1) {
        ESP_LOGD(TAG, "Read err");
        return value;
    }
    *read_buf = value;
    return 0;
}

static int gc0308_write(uint8_t sccb_port, uint8_t reg, uint8_t data)
{
    return sccb_write_reg8_val8(sccb_port, GC0308_SCCB_ADDR, reg, data);
}

/* write a array of registers  */
static int gc0308_write_array(uint8_t sccb_port, gc0308_reginfo_t *regarray, size_t regs_size)
{
    int i = 0, ret = 0;
    while (!ret && (i < regs_size)) {
        if (regarray[i].reg == GC0308_REG_DELAY) {
            delay_ms(10);
        } else {
            ret = gc0308_write(sccb_port, regarray[i].reg, regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "count=%d", i);
    return ret;
}

static int gc0308_set_reg_bits(uint8_t sccb_port, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    int ret = 0;
    uint8_t reg_data = 0;

    ret = gc0308_read(sccb_port, reg, &reg_data);
    if (ret < 0) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = gc0308_write(sccb_port, reg, value);
    return ret;
}

static int gc0308_set_pattern(esp_camera_device_t *dev, int enable)
{
    int ret = 0;
    ret = gc0308_write(dev->sccb_port, GC0308_REG_PAGE_SELECT, 0x00);
    if (enable) {
        ret |= gc0308_set_reg_bits(dev->sccb_port, GC0308_REG_DEBUG_MODE, 0, 0x01, 0x01);
    } else {
        ret |= gc0308_set_reg_bits(dev->sccb_port, GC0308_REG_DEBUG_MODE, 0, 0x01, 0x00);
    }
    return ret;
}

static int gc0308_soft_reset(esp_camera_device_t *dev)
{
    int ret = gc0308_write(dev->sccb_port, GC0308_REG_PAGE_SELECT, 0x00);
    ret |= gc0308_set_reg_bits(dev->sccb_port, GC0308_REG_PAGE_SELECT, 7, 1, 0x01);
    delay_ms(5);
    return ret;
}

static int gc0308_get_sensor_id(esp_camera_device_t *dev, sensor_id_t *id)
{
    int ret = -1;
    uint8_t pid_h;
    ret = gc0308_write(dev->sccb_port, GC0308_REG_PAGE_SELECT, 0x00);
    ret |= gc0308_read(dev->sccb_port, 0x00, &pid_h);
    if (!ret && pid_h) {
        id->PID = pid_h;
        ret = 0;
    }
    return ret;
}

static int gc0308_set_stream(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    // gc0308_set_pattern(dev, 1);
    if (enable) {
        ret = gc0308_set_reg_bits(dev->sccb_port, GC0308_REG_ANALOG_MODE, 0, 1, 0);
        ret |= gc0308_write(dev->sccb_port, GC0308_REG_OUTPUT_EN, 0x0f);
    } else {
        ret = gc0308_set_reg_bits(dev->sccb_port, GC0308_REG_ANALOG_MODE, 0, 1, 0x01);
        ret |= gc0308_write(dev->sccb_port, GC0308_REG_OUTPUT_EN, 0x00);
    }

    if (!ret) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static int gc0308_set_mirror(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    ret = gc0308_write(dev->sccb_port, GC0308_REG_PAGE_SELECT, 0x00);
    ret |= gc0308_set_reg_bits(dev->sccb_port, GC0308_REG_CISCTL_MODE1, 0, 0x01, enable != 0);
    if (ret == 0) {
        ESP_LOGD(TAG, "Set h-mirror to: %d", enable);
    }
    return ret;
}

static int gc0308_set_vflip(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    ret = gc0308_write(dev->sccb_port, GC0308_REG_PAGE_SELECT, 0x00);
    ret |= gc0308_set_reg_bits(dev->sccb_port, GC0308_REG_CISCTL_MODE1, 1, 0x01, enable != 0);
    if (ret == 0) {
        ESP_LOGD(TAG, "Set h-mirror to: %d", enable);
    }

    return ret;
}

static int gc0308_query_support_formats(esp_camera_device_t *dev, void *parry)
{
    sensor_format_array_info_t *formats = (sensor_format_array_info_t *)parry;
    formats->count = ARRAY_SIZE(gc0308_format_info);
    formats->format_array = &gc0308_format_info[0];
    ESP_LOGI(TAG, "f_array=%p", formats->format_array);
    return 0;
}

static int gc0308_query_support_capability(esp_camera_device_t *dev, void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
    sensor_capability_t *sensor_cap = (sensor_capability_t *)arg;
    sensor_cap->fmt_yuv = 1;
    sensor_cap->fmt_rgb565 = 1;
    return 0;
}

static int gc0308_set_format(esp_camera_device_t *dev, void *format)
{
    int ret = 0;
    sensor_format_t *fmt = (sensor_format_t *)format;

    ret = gc0308_write_array(dev->sccb_port, (gc0308_reginfo_t *)fmt->regs, fmt->regs_size);

    if (ret < 0) {
        ESP_CAM_SENSOR_LOGE("Set format regs fail");
        return ESP_CAM_SENSOR_FAILED_TO_S_FORMAT;
    }

    dev->cur_format = fmt;

    return ret;
}

static int gc0308_get_format(esp_camera_device_t *dev, void *ret_format)
{
    int ret = -1;
    sensor_format_t **format = (sensor_format_t **)ret_format;
    if (dev->cur_format != NULL) {
        *format = (void *)dev->cur_format;
        ret = 0;
    }
    return ret;
}

static int gc0308_priv_ioctl(esp_camera_device_t *dev, unsigned int cmd, void *arg)
{
    int ret = 0;
    gc0308_reginfo_t *gc0308_reg;
    GC0308_IO_MUX_LOCK();

    if (cmd & SENSOR_IOC_SET) {
        switch (cmd) {
        case CAM_SENSOR_S_HW_RESET:
            ret = 0;
            break;
        case CAM_SENSOR_S_SF_RESET:
            ret = gc0308_soft_reset(dev);
            break;
        case CAM_SENSOR_S_REG:
            gc0308_reg = (gc0308_reginfo_t *)arg;
            ret = gc0308_write(dev->sccb_port, gc0308_reg->reg, gc0308_reg->val);
            break;
        case CAM_SENSOR_S_STREAM:
            ret = gc0308_set_stream(dev, *(int *)arg);
            break;
        case CAM_SENSOR_S_VFLIP:
            ret = gc0308_set_vflip(dev, *(int *)arg);
            break;
        case CAM_SENSOR_S_HMIRROR:
            ret = gc0308_set_mirror(dev, *(int *)arg);
            break;
        case CAM_SENSOR_S_TEST_PATTERN:
            ret = gc0308_set_pattern(dev, *(int *)arg);
            break;
        }
    } else {
        switch (cmd) {
        case CAM_SENSOR_G_REG:
            gc0308_reg = (gc0308_reginfo_t *)arg;
            ret = gc0308_read(dev->sccb_port, gc0308_reg->reg, &gc0308_reg->val);
            break;
        case CAM_SENSOR_G_CHIP_ID:
            ret = gc0308_get_sensor_id(dev, arg);
            break;
        default:
            break;
        }
    }
    GC0308_IO_MUX_UNLOCK();
    return ret;
}

static int gc0308_get_name(esp_camera_device_t *dev, void *name, size_t *size)
{
    size_t len = strlen(GC0308_SENSOR_NAME);
    if (size == NULL || *size < len) {
        return -1;
    }
    strcpy((char *)name, GC0308_SENSOR_NAME);
    *size = len;
    return 0;
}

static int gc0308_power_on(esp_camera_device_t *dev)
{
    int ret = 0;

    if (dev->xclk_pin >= 0) {
        CAMERA_ENABLE_OUT_CLOCK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        // carefull, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_camera_ops_t gc0308_ops = {
    .query_support_formats = gc0308_query_support_formats,
    .query_support_capability = gc0308_query_support_capability,
    .set_format = gc0308_set_format,
    .get_format = gc0308_get_format,
    .priv_ioctl = gc0308_priv_ioctl,
    .get_name = gc0308_get_name,
};

esp_camera_device_t *gc0308_dvp_detect(const esp_camera_driver_config_t *config)
{
    esp_camera_device_t *dev = NULL;

    if (config == NULL) {
        return NULL;
    }

    if (s_gc0308_index >= GC0308_SUPPORT_NUM) {
        ESP_LOGE(TAG, "Only support max %d cameras", GC0308_SUPPORT_NUM);
        return NULL;
    }

    s_gc0308[s_gc0308_index] = calloc(sizeof(esp_camera_device_t), 1);
    if (s_gc0308[s_gc0308_index] == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }
    dev = s_gc0308[s_gc0308_index];
    dev->ops = &gc0308_ops;
    dev->sccb_port = config->sccb_port;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;

    // Configure sensor power, clock, and SCCB port
    if (gc0308_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (gc0308_get_sensor_id(dev, &dev->id) == -1) {
        ESP_LOGE(TAG, "Camera get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.PID != GC0308_PID) {
        ESP_LOGE(TAG, "Camera sensor is not GC0308, PID=0x%x", dev->id.PID);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x with index %d", dev->id.PID, s_gc0308_index);

    s_gc0308_index++;

    return dev;

err_free_handler:
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_GC0308_AUTO_DETECT
ESP_CAMERA_DETECT_FN(gc0308_dvp_detect, CAMERA_INTF_DVP)
{
    return gc0308_dvp_detect(config);
}
#endif
