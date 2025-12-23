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
#include "bf3045_settings.h"
#include "bf3045.h"

/*
 * BF3045 camera sensor banding mode.
 */
typedef enum {
    BF3045_BANDING_50HZ,
    BF3045_BANDING_60HZ,
} bf3045_banding_mode_t;

#define BF3045_IO_MUX_LOCK(mux)
#define BF3045_IO_MUX_UNLOCK(mux)
#define BF3045_ENABLE_OUT_XCLK(pin,clk)
#define BF3045_DISABLE_OUT_XCLK(pin)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define BF3045_SUPPORT_NUM CONFIG_CAMERA_BF3045_MAX_SUPPORT

static const char *TAG = "bf3045";

#ifndef CONFIG_CAMERA_BF3045_DVP_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for BF3045"
#endif

static const uint8_t bf3045_format_default_index = CONFIG_CAMERA_BF3045_DVP_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t bf3045_format_index[] = {
#if CONFIG_CAMERA_BF3045_DVP_RGB565_BE_640X480_15FPS
    0,
#endif
};

static const esp_cam_sensor_format_t bf3045_format_info[] = {
#if CONFIG_CAMERA_BF3045_DVP_RGB565_BE_640X480_15FPS
    {
        .name = "DVP_8bit_20Minput_RGB565_BE_640x480_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565_BE,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = bf3045_dvp_8bit_20Minput_640x480_rgb565_be_15fps,
        .regs_size = ARRAY_SIZE(bf3045_dvp_8bit_20Minput_640x480_rgb565_be_15fps),
        .fps = 15,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
};

static uint8_t get_bf3045_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(bf3045_format_index); i++) {
        if (bf3045_format_index[i] == bf3045_format_default_index) {
            return i;
        }
    }

    return 0;
}

static esp_err_t bf3045_read(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, read_buf);
}

static esp_err_t bf3045_write(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a8v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t bf3045_write_array(esp_sccb_io_handle_t sccb_handle, bf3045_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && (i < regs_size)) {
        if (regarray[i].reg != BF3045_REG_DELAY) {
            ret = bf3045_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t bf3045_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = bf3045_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = bf3045_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t bf3045_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    ret = bf3045_write(dev->sccb_handle, BF3045_REG_TEST_MODE, enable ? 0x80 : 0x00);
    return ret;
}

static esp_err_t bf3045_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t bf3045_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = bf3045_write(dev->sccb_handle, BF3045_REG_COM7, 0x80);
    delay_ms(5);
    return ret;
}

static esp_err_t bf3045_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = bf3045_read(dev->sccb_handle, BF3045_REG_CHIP_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = bf3045_read(dev->sccb_handle, BF3045_REG_CHIP_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t bf3045_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    // Hardware standby mode is supported only.
    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ESP_OK;
}

static esp_err_t bf3045_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = bf3045_set_reg_bits(dev->sccb_handle, BF3045_REG_MVFP, 5, 1, enable != 0);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set h-mirror to: %d", enable);
    }
    return ret;
}

static esp_err_t bf3045_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = bf3045_set_reg_bits(dev->sccb_handle, BF3045_REG_MVFP, 4, 1, enable != 0);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set vflip to: %d", enable);
    }

    return ret;
}

static esp_err_t bf3045_set_brightness(esp_cam_sensor_device_t *dev, int val)
{
    esp_err_t ret = bf3045_write(dev->sccb_handle, 0x55, val);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set bright to: %d", val);
    }

    return ret;
}

static esp_err_t bf3045_set_antibanding(esp_cam_sensor_device_t *dev, int val)
{
    esp_err_t ret = ESP_OK;
    switch (val) {
    case BF3045_BANDING_50HZ:
        ret = bf3045_set_reg_bits(dev->sccb_handle, 0x80, 0, 0x01, 1);
        break;
    case BF3045_BANDING_60HZ:
        ret = bf3045_set_reg_bits(dev->sccb_handle, 0x80, 0, 0x01, 0);
        break;
    default:
        break;
    }
    return ret;
}

static esp_err_t bf3045_set_AE_target(esp_cam_sensor_device_t *dev, int ae_level)
{
    esp_err_t ret = ESP_OK;
    if (ae_level > BF3045_AEC_TARGET_MAX) {
        ae_level = BF3045_AEC_TARGET_MAX;
    } else if (ae_level < BF3045_AEC_TARGET_MIN) {
        ae_level = BF3045_AEC_TARGET_MIN;
    }

    ret = bf3045_write(dev->sccb_handle, 0x24, ae_level);
    return ret;
}

static esp_err_t bf3045_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_AE_LEVEL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = BF3045_AEC_TARGET_MIN;
        qdesc->number.maximum = BF3045_AEC_TARGET_MAX;
        qdesc->number.step = 1;
        qdesc->default_value = BF3045_AEC_TARGET_DEFAULT;
        break;
    case ESP_CAM_SENSOR_AE_FLICKER:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = BF3045_BANDING_50HZ;
        qdesc->number.maximum = BF3045_BANDING_60HZ;
        qdesc->number.step = 1;
        qdesc->default_value = BF3045_BANDING_50HZ;
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t bf3045_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t bf3045_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = bf3045_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = bf3045_set_mirror(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_AE_LEVEL: {
        int *value = (int *)arg;

        ret = bf3045_set_AE_target(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_AE_FLICKER: {
        int *value = (int *)arg;

        ret = bf3045_set_antibanding(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_BRIGHTNESS: {
        int *value = (int *)arg;

        ret = bf3045_set_brightness(dev, *value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t bf3045_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(bf3045_format_info);
    formats->format_array = &bf3045_format_info[0];
    return ESP_OK;
}

static esp_err_t bf3045_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_rgb565 = 1;
    sensor_cap->fmt_yuv = 0;
    return ESP_OK;
}

static esp_err_t bf3045_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &bf3045_format_info[get_bf3045_actual_format_index()];
    }

    ret = bf3045_write_array(dev->sccb_handle, (bf3045_reginfo_t *)format->regs, format->regs_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;

    return ret;
}

static esp_err_t bf3045_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t bf3045_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    BF3045_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = bf3045_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = bf3045_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = bf3045_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = bf3045_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = bf3045_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = bf3045_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = bf3045_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    BF3045_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t bf3045_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        BF3045_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
        delay_ms(6);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

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
        gpio_config(&conf);

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t bf3045_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        BF3045_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t bf3045_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del bf3045 (%p)", dev);
    if (dev) {
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t bf3045_ops = {
    .query_para_desc = bf3045_query_para_desc,
    .get_para_value = bf3045_get_para_value,
    .set_para_value = bf3045_set_para_value,
    .query_support_formats = bf3045_query_support_formats,
    .query_support_capability = bf3045_query_support_capability,
    .set_format = bf3045_set_format,
    .get_format = bf3045_get_format,
    .priv_ioctl = bf3045_priv_ioctl,
    .del = bf3045_delete
};

esp_cam_sensor_device_t *bf3045_detect(esp_cam_sensor_config_t *config)
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

    dev->name = (char *)BF3045_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &bf3045_ops;
    dev->cur_format = &bf3045_format_info[get_bf3045_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (bf3045_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (bf3045_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != BF3045_PID) {
        ESP_LOGE(TAG, "Camera sensor is not BF3045, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    bf3045_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_BF3045_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(bf3045_detect, ESP_CAM_SENSOR_DVP, BF3045_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return bf3045_detect(config);
}
#endif
