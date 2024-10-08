/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
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
#include "ov5647_settings.h"
#include "ov5647.h"

#define OV5647_IO_MUX_LOCK(mux)
#define OV5647_IO_MUX_UNLOCK(mux)
#define OV5647_ENABLE_OUT_CLOCK(pin,clk)
#define OV5647_DISABLE_OUT_CLOCK(pin)

#define OV5647_PID         0x5647
#define OV5647_SENSOR_NAME "OV5647"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define OV5647_SUPPORT_NUM CONFIG_CAMERA_OV5647_MAX_SUPPORT

static const char *TAG = "ov5647";

static const esp_cam_sensor_isp_info_t ov5647_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81666700,
            .vts = 1896,
            .hts = 984,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81666700,
            .vts = 1896,
            .hts = 984,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81666700,
            .vts = 1896,
            .hts = 984,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
};

static const esp_cam_sensor_format_t ov5647_format_info[] = {
    {
        .name = "MIPI_2lane_24Minput_RAW8_800x1280_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 1280,
        .regs = ov5647_input_24M_MIPI_2lane_raw8_800x1280_50fps,
        .regs_size = ARRAY_SIZE(ov5647_input_24M_MIPI_2lane_raw8_800x1280_50fps),
        .fps = 50,
        .isp_info = &ov5647_isp_info[0],
        .mipi_info = {
            .mipi_clk = OV5647_MIPI_CSI_LINE_RATE_800x1280_50FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5647_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_800x640_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 640,
        .regs = ov5647_input_24M_MIPI_2lane_raw8_800x640_50fps,
        .regs_size = ARRAY_SIZE(ov5647_input_24M_MIPI_2lane_raw8_800x640_50fps),
        .fps = 50,
        .isp_info = &ov5647_isp_info[1],
        .mipi_info = {
            .mipi_clk = OV5647_MIPI_CSI_LINE_RATE_800x640_50FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5647_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_800x800_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 800,
        .regs = ov5647_input_24M_MIPI_2lane_raw8_800x800_50fps,
        .regs_size = ARRAY_SIZE(ov5647_input_24M_MIPI_2lane_raw8_800x800_50fps),
        .fps = 50,
        .isp_info = &ov5647_isp_info[2],
        .mipi_info = {
            .mipi_clk = OV5647_MIPI_CSI_LINE_RATE_800x800_50FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5647_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
};

static esp_err_t ov5647_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t ov5647_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers */
static esp_err_t ov5647_write_array(esp_sccb_io_handle_t sccb_handle, const ov5647_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != OV5647_REG_END) {
        if (regarray[i].reg != OV5647_REG_DELAY) {
            ret = ov5647_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "count=%d", i);
    return ret;
}

static esp_err_t ov5647_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = ov5647_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = ov5647_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t ov5647_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return ov5647_set_reg_bits(dev->sccb_handle, 0x503D, 7, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov5647_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return 0;
}

static esp_err_t ov5647_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ov5647_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t ov5647_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    uint8_t pid_h, pid_l;
    esp_err_t ret = ov5647_read(dev->sccb_handle, OV5647_REG_SENSOR_ID_H, &pid_h);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read pid_h failed");

    ret = ov5647_read(dev->sccb_handle, OV5647_REG_SENSOR_ID_L, &pid_l);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read pid_l failed");

    uint16_t pid = (pid_h << 8) | pid_l;
    if (pid) {
        id->pid = pid;
    }
    return ret;
}

static esp_err_t ov5647_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret;
    uint8_t val = OV5647_MIPI_CTRL00_BUS_IDLE;
    if (enable) {
#if CSI2_NONCONTINUOUS_CLOCK
        val |= OV5647_MIPI_CTRL00_CLOCK_LANE_GATE | OV5647_MIPI_CTRL00_LINE_SYNC_ENABLE;
#endif
    } else {
        val |= OV5647_MIPI_CTRL00_CLOCK_LANE_GATE | OV5647_MIPI_CTRL00_CLOCK_LANE_DISABLE;
    }

    ret = ov5647_write(dev->sccb_handle, 0x4800, CONFIG_CAMERA_OV5647_CSI_LINESYNC_ENABLE ? 0x14 : 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write pad out failed");

#if CONFIG_CAMERA_OV5647_ISP_AF_ENABLE
    ret = ov5647_write(dev->sccb_handle, 0x3002, enable ? 0x01 : 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write pad out failed");

    ret = ov5647_write(dev->sccb_handle, 0x3010, enable ? 0x01 : 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write pad out failed");

    ret = ov5647_write(dev->sccb_handle, 0x300D, enable ? 0x01 : 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write pad out failed");
#endif

    ret = ov5647_write(dev->sccb_handle, 0x0100, enable ? 0x01 : 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write pad out failed");

    dev->stream_status = enable;

    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t ov5647_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return ov5647_set_reg_bits(dev->sccb_handle, 0x3821, 1, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov5647_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return ov5647_set_reg_bits(dev->sccb_handle, 0x3820, 1, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov5647_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
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
    default: {
        ESP_LOGE(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t ov5647_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t ov5647_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = ov5647_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = ov5647_set_mirror(dev, *value);
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

static esp_err_t ov5647_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(ov5647_format_info);
    formats->format_array = &ov5647_format_info[0];
    return ESP_OK;
}

static esp_err_t ov5647_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return ESP_OK;
}

static esp_err_t ov5647_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
            format = &ov5647_format_info[CONFIG_CAMERA_OV5647_MIPI_IF_FORMAT_INDEX_DAFAULT];
        } else {
            ESP_LOGE(TAG, "Not support DVP port");
        }
    }
    // reset
    ret = ov5647_write_array(dev->sccb_handle, ov5647_mipi_reset_regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write reset regs failed");
    // write format related regs
    ret = ov5647_write_array(dev->sccb_handle, (const ov5647_reginfo_t *)format->regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write fmt regs failed");
    // stop stream default
    ret = ov5647_set_stream(dev, 0);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write stream regs failed");

    dev->cur_format = format;

    return ret;
}

static esp_err_t ov5647_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t ov5647_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_FAIL;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    OV5647_IO_MUX_LOCK(mux);
    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = ov5647_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = ov5647_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov5647_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = ov5647_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = ov5647_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov5647_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = ov5647_get_sensor_id(dev, arg);
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    OV5647_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t ov5647_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV5647_ENABLE_OUT_CLOCK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pwdn pin config failed");

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
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "reset pin config failed");

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t ov5647_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV5647_DISABLE_OUT_CLOCK(dev->xclk_pin);
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

static esp_err_t ov5647_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del ov5647 (%p)", dev);
    if (dev) {
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t ov5647_ops = {
    .query_para_desc = ov5647_query_para_desc,
    .get_para_value = ov5647_get_para_value,
    .set_para_value = ov5647_set_para_value,
    .query_support_formats = ov5647_query_support_formats,
    .query_support_capability = ov5647_query_support_capability,
    .set_format = ov5647_set_format,
    .get_format = ov5647_get_format,
    .priv_ioctl = ov5647_priv_ioctl,
    .del = ov5647_delete
};

// We need manage these devices, and maybe need to add it into the private member of esp_device
esp_cam_sensor_device_t *ov5647_detect(esp_cam_sensor_config_t *config)
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

    dev->name = (char *)OV5647_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &ov5647_ops;
    if (config->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        dev->cur_format = &ov5647_format_info[CONFIG_CAMERA_OV5647_MIPI_IF_FORMAT_INDEX_DAFAULT];
    } else {
        ESP_LOGE(TAG, "Not support DVP port");
    }

    // Configure sensor power, clock, and SCCB port
    if (ov5647_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (ov5647_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != OV5647_PID) {
        ESP_LOGE(TAG, "Camera sensor is not OV5647, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    ov5647_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_OV5647_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(ov5647_detect, ESP_CAM_SENSOR_MIPI_CSI, OV5647_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return ov5647_detect(config);
}
#endif
