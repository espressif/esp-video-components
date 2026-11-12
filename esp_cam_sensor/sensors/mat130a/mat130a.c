/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************
Copyright (C), 2026, MetaSilicon Tech. Co., Ltd.
All rights reserved.
****************************************************/

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "mat130a_settings.h"
#include "mat130a.h"

#define MAT130A_IO_MUX_LOCK(mux)
#define MAT130A_IO_MUX_UNLOCK(mux)
#define MAT130A_ENABLE_OUT_XCLK(pin, clk)
#define MAT130A_DISABLE_OUT_XCLK(pin)
#define delay_ms(ms) vTaskDelay(pdMS_TO_TICKS((ms) > 0 ? (ms) : 1))

static const char *TAG = "mat130a";

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
#ifndef CONFIG_CAMERA_MAT130A_DVP_IF_FORMAT_INDEX_DEFAULT
#error "Please enable the MAT130A DVP format in menuconfig"
#endif

static const esp_cam_sensor_format_t mat130a_format_info_dvp[] = {
#if CONFIG_CAMERA_MAT130A_DVP_YUV422_1280X720_25FPS
    {
        .name = "DVP_8bit_24Minput_YUV422_UYVY_1280x720_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = MAT130A_DVP_PROFILE_XCLK_HZ,
        .width = MAT130A_DVP_PROFILE_WIDTH,
        .height = MAT130A_DVP_PROFILE_HEIGHT,
        .regs = NULL,
        .regs_size = 0,
        .fps = MAT130A_DVP_PROFILE_FPS,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
};
#endif

static esp_err_t mat130a_read(esp_sccb_io_handle_t sccb, uint16_t reg, uint16_t *value)
{
    return esp_sccb_transmit_receive_reg_a16v16(sccb, reg, value);
}

static esp_err_t mat130a_write(esp_sccb_io_handle_t sccb, uint16_t reg, uint16_t value)
{
    return esp_sccb_transmit_reg_a16v16(sccb, reg, value);
}

static esp_err_t mat130a_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin < 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    gpio_set_level(dev->reset_pin, 0);
    delay_ms(10);
    gpio_set_level(dev->reset_pin, 1);
    delay_ms(MAT130A_STARTUP_DELAY_MS);
    return ESP_OK;
}

static esp_err_t mat130a_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = mat130a_write(dev->sccb_handle,
                                  MAT130A_REG_SOFTWARE_RESET, 0x0001);
    if (ret == ESP_OK) {
        delay_ms(MAT130A_STARTUP_DELAY_MS);
    }
    return ret;
}

static esp_err_t mat130a_get_sensor_id(esp_cam_sensor_device_t *dev,
                                       esp_cam_sensor_id_t *id)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, id);

    uint32_t chip_id = 0;
    esp_err_t ret = esp_sccb_transmit_receive_reg_a16v32(
                        dev->sccb_handle, MAT130A_REG_CHIP_ID_RPT, &chip_id);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read CHIP_ID_RPT failed");

    if (chip_id != MAT130A_CHIP_ID_RPT_VALUE &&
            __builtin_bswap32(chip_id) == MAT130A_CHIP_ID_RPT_VALUE) {
        chip_id = __builtin_bswap32(chip_id);
    }
    ESP_RETURN_ON_FALSE(chip_id == MAT130A_CHIP_ID_RPT_VALUE,
                        ESP_ERR_NOT_FOUND, TAG,
                        "unexpected CHIP_ID_RPT=0x%08" PRIx32, chip_id);

    id->pid = (uint16_t)(chip_id >> 16);
    ESP_LOGI(TAG, "CHIP_ID_RPT=0x%08" PRIx32 ", PID=0x%04x", chip_id, id->pid);
    return ESP_OK;
}

static esp_err_t mat130a_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    ESP_RETURN_ON_FALSE(dev->sensor_port == ESP_CAM_SENSOR_DVP,
                        ESP_ERR_NOT_SUPPORTED, TAG, "only DVP is supported");
    esp_err_t ret = mat130a_write(dev->sccb_handle, MAT130A_REG_STREAM,
                                  enable ? MAT130A_DVP_STREAM_ON : MAT130A_DVP_STREAM_OFF);
    if (ret == ESP_OK) {
        dev->stream_status = (uint8_t)!!enable;
        delay_ms(enable ? MAT130A_STREAM_ON_DELAY_MS : MAT130A_STREAM_OFF_DELAY_MS);
    }
    ESP_LOGD(TAG, "set_stream done");
    return ret;
}

static esp_err_t mat130a_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return mat130a_write(dev->sccb_handle, MAT130A_REG_TEST_PATTERN,
                         enable ? 0x0001 : 0x0000);
}

static esp_err_t mat130a_set_hmirror(esp_cam_sensor_device_t *dev, int enable)
{
    return mat130a_write(dev->sccb_handle, MAT130A_REG_MIRROR,
                         enable ? 0x0001 : 0x0000);
}

static esp_err_t mat130a_query_para_desc(esp_cam_sensor_device_t *dev,
        esp_cam_sensor_param_desc_t *qdesc)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, qdesc);

    switch (qdesc->id) {
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        return ESP_OK;
    case ESP_CAM_SENSOR_DATA_SEQ:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_U8;
        qdesc->u8.size = sizeof(uint32_t);
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static esp_err_t mat130a_get_para_value(esp_cam_sensor_device_t *dev,
                                        uint32_t id, void *arg, size_t size)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);

    if (id == ESP_CAM_SENSOR_DATA_SEQ) {
        ESP_RETURN_ON_FALSE(size >= sizeof(uint32_t), ESP_ERR_INVALID_SIZE,
                            TAG, "invalid DATA_SEQ size");

        *(uint32_t *)arg = ESP_CAM_SENSOR_DATA_SEQ_NONE;
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t mat130a_set_para_value(esp_cam_sensor_device_t *dev,
                                        uint32_t id, const void *arg, size_t size)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);

    if (id == ESP_CAM_SENSOR_HMIRROR) {
        ESP_RETURN_ON_FALSE(size >= sizeof(int), ESP_ERR_INVALID_SIZE,
                            TAG, "invalid HMIRROR size");
        return mat130a_set_hmirror(dev, *(const int *)arg);
    }
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t mat130a_query_support_formats(esp_cam_sensor_device_t *dev,
        esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
        formats->count = ARRAY_SIZE(mat130a_format_info_dvp);
        formats->format_array = &mat130a_format_info_dvp[0];
        return ESP_OK;
    }
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t mat130a_query_support_capability(esp_cam_sensor_device_t *dev,
        esp_cam_sensor_capability_t *cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, cap);
    cap->fmt_yuv = 1;
    return ESP_OK;
}

static esp_err_t mat130a_set_format(esp_cam_sensor_device_t *dev,
                                    const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    if (format == NULL) {
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
        if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
            format = &mat130a_format_info_dvp[CONFIG_CAMERA_MAT130A_DVP_IF_FORMAT_INDEX_DEFAULT];
        } else
#endif
        {
            return ESP_ERR_NOT_SUPPORTED;
        }
    }

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
        ESP_RETURN_ON_FALSE(format->port == ESP_CAM_SENSOR_DVP,
                            ESP_ERR_NOT_SUPPORTED, TAG, "format is not DVP");
        ESP_RETURN_ON_FALSE(format->width == MAT130A_DVP_PROFILE_WIDTH &&
                            format->height == MAT130A_DVP_PROFILE_HEIGHT &&
                            format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
                            ESP_ERR_NOT_SUPPORTED, TAG, "unsupported DVP format");
    } else
#endif
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_LOGD(TAG, "set_format:%s", format->name);

    dev->cur_format = format;
    return ESP_OK;
}

static esp_err_t mat130a_get_format(esp_cam_sensor_device_t *dev,
                                    esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, format);
    ESP_RETURN_ON_FALSE(dev->cur_format, ESP_FAIL, TAG, "format is not set");
    memcpy(format, dev->cur_format, sizeof(*format));
    return ESP_OK;
}

static esp_err_t mat130a_priv_ioctl(esp_cam_sensor_device_t *dev,
                                    uint32_t cmd, void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    uint16_t value = 0;
    esp_cam_sensor_reg_val_t *sensor_reg;
    MAT130A_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = mat130a_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = mat130a_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = mat130a_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = mat130a_read(dev->sccb_handle, sensor_reg->regaddr, &value);
        if (ret == ESP_OK) {
            sensor_reg->value = value;
        }
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = mat130a_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = mat130a_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = mat130a_get_sensor_id(dev, (esp_cam_sensor_id_t *)arg);
        break;
    default:
        ESP_LOGE(TAG, "cmd=%" PRIx32 " is not supported", cmd);
        break;
    }

    MAT130A_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t mat130a_power_on(esp_cam_sensor_device_t *dev)
{
    if (dev->xclk_pin >= 0) {
        MAT130A_ENABLE_OUT_XCLK(dev->xclk_pin, MAT130A_DVP_PROFILE_XCLK_HZ);
        delay_ms(12);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = {
            .pin_bit_mask = 1ULL << dev->pwdn_pin,
                                 .mode = GPIO_MODE_OUTPUT,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&conf), TAG, "PWDN pin config failed");
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = {
            .pin_bit_mask = 1ULL << dev->reset_pin,
                                 .mode = GPIO_MODE_OUTPUT,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&conf), TAG, "RESET pin config failed");
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
    }

    delay_ms(MAT130A_STARTUP_DELAY_MS);
    return ESP_OK;
}

static esp_err_t mat130a_power_off(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
    }
    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 1);
    }
    if (dev->xclk_pin >= 0) {
        MAT130A_DISABLE_OUT_XCLK(dev->xclk_pin);
    }
    return ESP_OK;
}

static esp_err_t mat130a_delete(esp_cam_sensor_device_t *dev)
{
    if (dev) {
        mat130a_power_off(dev);
        free(dev);
    }
    return ESP_OK;
}

static const esp_cam_sensor_ops_t mat130a_ops = {
    .query_para_desc = mat130a_query_para_desc,
    .get_para_value = mat130a_get_para_value,
    .set_para_value = mat130a_set_para_value,
    .query_support_formats = mat130a_query_support_formats,
    .query_support_capability = mat130a_query_support_capability,
    .set_format = mat130a_set_format,
    .get_format = mat130a_get_format,
    .priv_ioctl = mat130a_priv_ioctl,
    .del = mat130a_delete,
};

esp_cam_sensor_device_t *mat130a_detect(esp_cam_sensor_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    esp_cam_sensor_device_t *dev = calloc(1, sizeof(*dev));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    dev->name = (char *)MAT130A_SENSOR_NAME;
    dev->ops = &mat130a_ops;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (config->sensor_port == ESP_CAM_SENSOR_DVP) {
        dev->cur_format = &mat130a_format_info_dvp[CONFIG_CAMERA_MAT130A_DVP_IF_FORMAT_INDEX_DEFAULT];
    } else
#endif
    {
        ESP_LOGE(TAG, "Unsupported sensor port");
        free(dev);
        return NULL;
    }

    if (mat130a_power_on(dev) != ESP_OK ||
            mat130a_get_sensor_id(dev, &dev->id) != ESP_OK ||
            dev->id.pid != MAT130A_PID) {
        ESP_LOGE(TAG, "MAT130A detection failed");
        mat130a_power_off(dev);
        free(dev);
        return NULL;
    }

    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);
    return dev;
}

#if CONFIG_CAMERA_MAT130A_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(mat130a_detect, ESP_CAM_SENSOR_DVP, MAT130A_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return mat130a_detect(config);
}
#endif
