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
#include "sc2336_settings.h"
#include "sc2336.h"

#define SC2336_IO_MUX_LOCK(mux)
#define SC2336_IO_MUX_UNLOCK(mux)
#define SC2336_ENABLE_OUT_XCLK(pin,clk)
#define SC2336_DISABLE_OUT_XCLK(pin)

#define SC2336_PID         0xcb3a
#define SC2336_SENSOR_NAME "SC2336"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define SC2336_SUPPORT_NUM CONFIG_CAMERA_SC2336_MAX_SUPPORT

static const char *TAG = "sc2336";

static const esp_cam_sensor_isp_info_t sc2336_isp_info[] = {
    /* For MIPI */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1500,
            .hts = 1800,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1800,
            .hts = 900,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1800,
            .hts = 750,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1125,
            .hts = 1200,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 66000000,
            .vts = 2250,
            .hts = 1200,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 2250,
            .hts = 1200,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 49500000,
            .vts = 2200,
            .hts = 750,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 67200000,
            .vts = 1000,
            .hts = 2240,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1250,
            .hts = 2240,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1250,
            .hts = 2240,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1250,
            .hts = 2240,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1250,
            .hts = 2240,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    /* For DVP */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 42000000,
            .vts = 525,
            .hts = 1600,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
};

static const esp_cam_sensor_format_t sc2336_format_info[] = {
    /* For MIPI */
    {
        .name = "MIPI_2lane_24Minput_RAW10_1280x720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[0],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1280x720_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_50fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_50fps),
        .fps = 50,
        .isp_info = &sc2336_isp_info[1],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1280x720_60fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_60fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_60fps),
        .fps = 60,
        .isp_info = &sc2336_isp_info[2],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RAW10_1920x1080_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_1lane_1080p_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_1080p_25fps),
        .fps = 25,
        .isp_info = &sc2336_isp_info[3],
        .mipi_info = {
            .mipi_clk = 660000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_1080p_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1080p_25fps),
        .fps = 25,
        .isp_info = &sc2336_isp_info[4],
        .mipi_info = {
            .mipi_clk = 330000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_1080p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1080p_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[5],
        .mipi_info = {
            .mipi_clk = 405000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_800*800_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 800,
        .regs = init_reglist_MIPI_2lane_10bit_800x800_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_10bit_800x800_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[6],
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_640*480_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = init_reglist_MIPI_2lane_10bit_640x480_50fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_10bit_640x480_50fps),
        .fps = 50,
        .isp_info = &sc2336_isp_info[7],
        .mipi_info = {
            .mipi_clk = 210000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_1920*1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_1080p_raw8_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1080p_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[8],
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_1280*720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_raw8_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[9],
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_800*800_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 800,
        .regs = init_reglist_MIPI_2lane_800x800_raw8_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_800x800_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[10],
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW8_1024*600_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1024,
        .height = 600,
        .regs = init_reglist_MIPI_2lane_1024x600_raw8_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1024x600_raw8_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[11],
        .mipi_info = {
            .mipi_clk = 288000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    /* For DVP */
    {
        .name = "DVP_8bits_24Minput_RAW10_1280*720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_DVP_720p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_720p_30fps),
        .fps = 30,
        .isp_info = &sc2336_isp_info[12],
        .mipi_info = {0},
        .reserved = NULL,
    },
};

static esp_err_t sc2336_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t sc2336_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sc2336_write_array(esp_sccb_io_handle_t sccb_handle, sc2336_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != SC2336_REG_END) {
        if (regarray[i].reg != SC2336_REG_DELAY) {
            ret = sc2336_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    return ret;
}

static esp_err_t sc2336_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sc2336_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = sc2336_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sc2336_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2336_set_reg_bits(dev->sccb_handle, 0x4501, 3, 1, enable ? 0x01 : 0x00);
}

static esp_err_t sc2336_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sc2336_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = sc2336_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sc2336_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = sc2336_read(dev->sccb_handle, SC2336_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sc2336_read(dev->sccb_handle, SC2336_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sc2336_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = sc2336_write(dev->sccb_handle, SC2336_REG_SLEEP_MODE, enable ? 0x01 : 0x00);

    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t sc2336_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2336_set_reg_bits(dev->sccb_handle, 0x3221, 1, 2,  enable ? 0x03 : 0x00);
}

static esp_err_t sc2336_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return sc2336_set_reg_bits(dev->sccb_handle, 0x3221, 5, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc2336_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t sc2336_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t sc2336_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = sc2336_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = sc2336_set_mirror(dev, *value);
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

static esp_err_t sc2336_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(sc2336_format_info);
    formats->format_array = &sc2336_format_info[0];
    return ESP_OK;
}

static esp_err_t sc2336_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t sc2336_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        if (dev->sensor_port != ESP_CAM_SENSOR_DVP) {
            format = &sc2336_format_info[CONFIG_CAMERA_SC2336_MIPI_IF_FORMAT_INDEX_DAFAULT];
        } else {
            format = &sc2336_format_info[CONFIG_CAMERA_SC2336_DVP_IF_FORMAT_INDEX_DAFAULT];
        }
    }

    ret = sc2336_write_array(dev->sccb_handle, (sc2336_reginfo_t *)format->regs);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;

    return ret;
}

static esp_err_t sc2336_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t sc2336_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SC2336_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sc2336_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sc2336_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc2336_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sc2336_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sc2336_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc2336_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sc2336_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    SC2336_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sc2336_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC2336_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
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

static esp_err_t sc2336_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC2336_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t sc2336_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sc2336 (%p)", dev);
    if (dev) {
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t sc2336_ops = {
    .query_para_desc = sc2336_query_para_desc,
    .get_para_value = sc2336_get_para_value,
    .set_para_value = sc2336_set_para_value,
    .query_support_formats = sc2336_query_support_formats,
    .query_support_capability = sc2336_query_support_capability,
    .set_format = sc2336_set_format,
    .get_format = sc2336_get_format,
    .priv_ioctl = sc2336_priv_ioctl,
    .del = sc2336_delete
};

esp_cam_sensor_device_t *sc2336_detect(esp_cam_sensor_config_t *config)
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

    dev->name = (char *)SC2336_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sc2336_ops;
    if (config->sensor_port != ESP_CAM_SENSOR_DVP) {
        dev->cur_format = &sc2336_format_info[CONFIG_CAMERA_SC2336_MIPI_IF_FORMAT_INDEX_DAFAULT];
    } else {
        dev->cur_format = &sc2336_format_info[CONFIG_CAMERA_SC2336_DVP_IF_FORMAT_INDEX_DAFAULT];
    }

    // Configure sensor power, clock, and SCCB port
    if (sc2336_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc2336_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SC2336_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SC2336, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sc2336_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SC2336_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc2336_detect, ESP_CAM_SENSOR_MIPI_CSI, SC2336_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc2336_detect(config);
}
#endif

#if CONFIG_CAMERA_SC2336_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc2336_detect, ESP_CAM_SENSOR_DVP, SC2336_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return sc2336_detect(config);
}
#endif
