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
#include "sp0a39_settings.h"
#include "sp0a39.h"

#define SP0A39_IO_MUX_LOCK(mux)
#define SP0A39_IO_MUX_UNLOCK(mux)
#define SP0A39_ENABLE_OUT_XCLK(pin,clk)
#define SP0A39_DISABLE_OUT_XCLK(pin)
#define SP0A39_FRAME_HEADER_SIZE 9
#define SP0A39_LINE_HEADER_SIZE  12
#define SP0A39_FRAME_END_SIZE    4

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

static const char *TAG = "sp0a39";

#ifndef CONFIG_CAMERA_SP0A39_SPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for SP0A39 SPI interface"
#endif

static const uint8_t sp0a39_spi_format_default_index = CONFIG_CAMERA_SP0A39_SPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t sp0a39_spi_format_index[] = {
#if CONFIG_CAMERA_SP0A39_SPI_2BIT_GRAY_640X480_15FPS
    0,
#endif
#if CONFIG_CAMERA_SP0A39_SPI_1BIT_GRAY_640X480_4FPS
    1,
#endif
#if CONFIG_CAMERA_SP0A39_SPI_4BIT_YUV422_640X480_15FPS
    2,
#endif
};

static const esp_cam_sensor_spi_frame_info sp0a39_frame_info_spi[] = {
    {
        .frame_size = (640 * 1 + SP0A39_LINE_HEADER_SIZE) * 480 + SP0A39_FRAME_HEADER_SIZE + SP0A39_FRAME_END_SIZE,
        .line_size = (640 * 1 + SP0A39_LINE_HEADER_SIZE),
        .frame_header_check = (uint8_t[]){0xff, 0xff, 0xff, 0x01},
        .line_header_check = (uint8_t[]){0xff, 0xff, 0xff, 0x02},
        .frame_header_size = SP0A39_FRAME_HEADER_SIZE,
        .line_header_size = SP0A39_LINE_HEADER_SIZE,
        .frame_header_check_size = 4,
        .line_header_check_size = 4,
        .drop_frame_count = 1, // The 1st SPI packet is not the image data, so we need to drop it
        .high_level_active = 1,
        .data_order_lsb_first = 1,
    },
    {
        .frame_size = (640 * 2 + SP0A39_LINE_HEADER_SIZE) * 480 + SP0A39_FRAME_HEADER_SIZE + SP0A39_FRAME_END_SIZE,
        .line_size = (640 * 2 + SP0A39_LINE_HEADER_SIZE),
        .frame_header_check = (uint8_t[]){0xff, 0xff, 0xff, 0x01},
        .line_header_check = (uint8_t[]){0xff, 0xff, 0xff, 0x02},
        .frame_header_size = SP0A39_FRAME_HEADER_SIZE,
        .line_header_size = SP0A39_LINE_HEADER_SIZE,
        .frame_header_check_size = 4,
        .line_header_check_size = 4,
        .drop_frame_count = 1,
        .high_level_active = 1,
        .data_order_lsb_first = 1,
    }
};

static const esp_cam_sensor_format_t sp0a39_format_info_spi[] = {
#if CONFIG_CAMERA_SP0A39_SPI_2BIT_GRAY_640X480_15FPS
    {
        .name = "SPI_2bit_24Minput_Gray_640x480_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,
        .port = ESP_CAM_SENSOR_SPI,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = SPI_2bit_24Minput_Gray_640x480_15fps,
        .regs_size = ARRAY_SIZE(SPI_2bit_24Minput_Gray_640x480_15fps),
        .fps = 15,
        .isp_info = NULL,
        .spi_info = {
            .rx_lines = 2,
            .frame_info = &sp0a39_frame_info_spi[0],
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SP0A39_SPI_1BIT_GRAY_640X480_4FPS
    {
        .name = "SPI_1bit_24Minput_Gray_640x480_4fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,
        .port = ESP_CAM_SENSOR_SPI,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = SPI_1bit_24Minput_Gray_640x480_4fps,
        .regs_size = ARRAY_SIZE(SPI_1bit_24Minput_Gray_640x480_4fps),
        .fps = 4,
        .isp_info = NULL,
        .spi_info = {
            .rx_lines = 1,
            .frame_info = &sp0a39_frame_info_spi[0],
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SP0A39_SPI_4BIT_YUV422_640X480_15FPS
    {
        .name = "SPI_4bit_24Minput_YUV422_640x480_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        .port = ESP_CAM_SENSOR_SPI,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = SPI_4bit_24Minput_YUV422_640x480_15fps,
        .regs_size = ARRAY_SIZE(SPI_4bit_24Minput_YUV422_640x480_15fps),
        .fps = 15,
        .isp_info = NULL,
        .spi_info = {
            .rx_lines = 4,
            .frame_info = &sp0a39_frame_info_spi[1],
        },
        .reserved = NULL,
    },
#endif
};

static uint8_t get_sp0a39_spi_actual_format_index(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(sp0a39_spi_format_index); i++) {
        if (sp0a39_spi_format_index[i] == sp0a39_spi_format_default_index) {
            return (uint8_t)i;
        }
    }
    return 0;
}

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
#ifndef CONFIG_CAMERA_SP0A39_DVP_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for SP0A39 DVP interface"
#endif

static const uint8_t sp0a39_dvp_format_default_index = CONFIG_CAMERA_SP0A39_DVP_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t sp0a39_dvp_format_index[] = {
#if CONFIG_CAMERA_SP0A39_DVP_GRAY_640X480_30FPS
    0,
#endif
#if CONFIG_CAMERA_SP0A39_DVP_GRAY_200X200_30FPS
    1,
#endif
};

static const esp_cam_sensor_format_t sp0a39_format_info_dvp[] = {
#if CONFIG_CAMERA_SP0A39_DVP_GRAY_640X480_30FPS
    {
        .name = "DVP_8bit_24Minput_Gray_640x480_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = DVP_8bit_24Minput_Gray_640x480_30fps,
        .regs_size = ARRAY_SIZE(DVP_8bit_24Minput_Gray_640x480_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SP0A39_DVP_GRAY_200X200_30FPS
    {
        .name = "DVP_8bit_24Minput_Gray_200x200_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 200,
        .height = 200,
        .regs = DVP_8bit_24Minput_Gray_200x200_30fps,
        .regs_size = ARRAY_SIZE(DVP_8bit_24Minput_Gray_200x200_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
#endif
};

static uint8_t get_sp0a39_dvp_actual_format_index(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(sp0a39_dvp_format_index); i++) {
        if (sp0a39_dvp_format_index[i] == sp0a39_dvp_format_default_index) {
            return (uint8_t)i;
        }
    }
    return 0;
}
#endif

static esp_err_t sp0a39_read(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, read_buf);
}

static esp_err_t sp0a39_write(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a8v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sp0a39_write_array(esp_sccb_io_handle_t sccb_handle, sp0a39_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && (i < regs_size)) {
        if (regarray[i].reg != SP0A39_REG_DELAY) {
            ret = sp0a39_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t sp0a39_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sp0a39_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = sp0a39_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sp0a39_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = sp0a39_write(dev->sccb_handle, SP0A39_REG_PAGE_SELECT, 0x01);
    ret |= sp0a39_set_reg_bits(dev->sccb_handle, 0x32, 0x07, 0x01, enable ? 0x01 : 0x00); // // p1:0x32[7]

    return ret;
}

static esp_err_t sp0a39_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sp0a39_soft_reset(esp_cam_sensor_device_t *dev)
{
    // todo, check soft reset
    esp_err_t ret = sp0a39_write(dev->sccb_handle, 0x31, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sp0a39_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = sp0a39_read(dev->sccb_handle, SP0A39_REG_CHIP_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sp0a39_read(dev->sccb_handle, SP0A39_REG_CHIP_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sp0a39_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    /* No soft standby.
    * HW PWDN pin must be used to ensure the stability of the device. */
    dev->stream_status = enable;
    return ESP_OK;
}

static esp_err_t sp0a39_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = sp0a39_write(dev->sccb_handle, SP0A39_REG_PAGE_SELECT, 0x00);
    ret |= sp0a39_set_reg_bits(dev->sccb_handle, 0x31, 1, 0x01, enable != 0);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set h-mirror to: %d", enable);
    }
    return ret;
}

static esp_err_t sp0a39_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = sp0a39_write(dev->sccb_handle, SP0A39_REG_PAGE_SELECT, 0x00);
    ret |= sp0a39_set_reg_bits(dev->sccb_handle, 0x31, 2, 0x01, enable != 0);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set vflip to: %d", enable);
    }

    return ret;
}

static esp_err_t sp0a39_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
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
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t sp0a39_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t sp0a39_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = sp0a39_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = sp0a39_set_mirror(dev, *value);
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

static esp_err_t sp0a39_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    if (dev->sensor_port == ESP_CAM_SENSOR_SPI) {
        formats->count = ARRAY_SIZE(sp0a39_format_info_spi);
        formats->format_array = &sp0a39_format_info_spi[0];
        return ESP_OK;
    }

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
        formats->count = ARRAY_SIZE(sp0a39_format_info_dvp);
        formats->format_array = &sp0a39_format_info_dvp[0];
        return ESP_OK;
    }
#endif

    ESP_LOGE(TAG, "Unsupported sensor port: %d", dev->sensor_port);
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t sp0a39_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_yuv = 1;
    return ESP_OK;
}

static esp_err_t sp0a39_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    if (dev->sensor_port != ESP_CAM_SENSOR_SPI &&
            dev->sensor_port != ESP_CAM_SENSOR_DVP) {
        ESP_LOGE(TAG, "Invalid sensor port: %d", dev->sensor_port);
        return ESP_ERR_INVALID_ARG;
    }

#if !CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
        ESP_LOGE(TAG, "DVP interface not supported in current configuration");
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif

    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        if (dev->sensor_port == ESP_CAM_SENSOR_SPI) {
            format = &sp0a39_format_info_spi[get_sp0a39_spi_actual_format_index()];
        }
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
        if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
            format = &sp0a39_format_info_dvp[get_sp0a39_dvp_actual_format_index()];
        }
#endif
    }

    if (format == NULL) {
        ESP_LOGE(TAG, "No format found for sensor port %d", dev->sensor_port);
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* Todo, I2C NACK error causes the I2C driver to fail. After fixing the error, re-enable the reset.*/
    // sp0a39_write(dev->sccb_handle, 0x12, 0x80);//Reset
    // delay_ms(5);

    ret = sp0a39_write_array(dev->sccb_handle, (sp0a39_reginfo_t *)format->regs, format->regs_size);
    ESP_LOGD(TAG, "Set format %s", format->name);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;

    return ret;
}

static esp_err_t sp0a39_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t sp0a39_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SP0A39_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sp0a39_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sp0a39_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sp0a39_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sp0a39_set_stream(dev, *(int *)arg);
        // ret = sp0a39_set_test_pattern(dev, 1);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sp0a39_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sp0a39_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sp0a39_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    SP0A39_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sp0a39_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    bool xclk_enabled = false;
    bool pwdn_configured = false;

    if (dev->xclk_pin >= 0) {
        SP0A39_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
        xclk_enabled = true;
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        if (ret != ESP_OK) {
            goto cleanup;
        }
        pwdn_configured = true;

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
            goto cleanup;
        }

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ESP_OK;

cleanup:
    if (pwdn_configured) {
        gpio_reset_pin(dev->pwdn_pin);
    }
    if (xclk_enabled) {
        SP0A39_DISABLE_OUT_XCLK(dev->xclk_pin);
    }
    return ret;
}

static esp_err_t sp0a39_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SP0A39_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t sp0a39_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sp0a39 (%p)", dev);
    if (dev) {
        sp0a39_power_off(dev);
        free(dev);
    }
    return ESP_OK;
}

static const esp_cam_sensor_ops_t sp0a39_ops = {
    .query_para_desc = sp0a39_query_para_desc,
    .get_para_value = sp0a39_get_para_value,
    .set_para_value = sp0a39_set_para_value,
    .query_support_formats = sp0a39_query_support_formats,
    .query_support_capability = sp0a39_query_support_capability,
    .set_format = sp0a39_set_format,
    .get_format = sp0a39_get_format,
    .priv_ioctl = sp0a39_priv_ioctl,
    .del = sp0a39_delete
};

esp_cam_sensor_device_t *sp0a39_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    if (config == NULL) {
        return NULL;
    }

    if (config->sensor_port != ESP_CAM_SENSOR_SPI &&
            config->sensor_port != ESP_CAM_SENSOR_DVP) {
        ESP_LOGE(TAG, "Unsupported sensor port: %d", config->sensor_port);
        return NULL;
    }

#if !CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (config->sensor_port == ESP_CAM_SENSOR_DVP) {
        ESP_LOGE(TAG, "DVP interface not supported on this chip");
        return NULL;
    }
#endif

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    dev->name = (char *)SP0A39_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sp0a39_ops;

    if (config->sensor_port == ESP_CAM_SENSOR_SPI) {
        dev->cur_format = &sp0a39_format_info_spi[get_sp0a39_spi_actual_format_index()];
    }

#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
        dev->cur_format = &sp0a39_format_info_dvp[get_sp0a39_dvp_actual_format_index()];
    }
#endif
    // Configure sensor power, clock, and SCCB port
    if (sp0a39_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sp0a39_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SP0A39_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SP0A39, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sp0a39_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SP0A39_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sp0a39_detect, ESP_CAM_SENSOR_DVP, SP0A39_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return sp0a39_detect(config);
}
#endif

#if CONFIG_CAMERA_SP0A39_AUTO_DETECT_SPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sp0a39_detect, ESP_CAM_SENSOR_SPI, SP0A39_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_SPI;
    return sp0a39_detect(config);
}
#endif
