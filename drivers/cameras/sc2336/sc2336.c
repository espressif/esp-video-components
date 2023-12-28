/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_log.h"

#include "esp_camera.h"
#include "sccb.h"
#include "sc2336_settings.h"

#define SC2336_IO_MUX_LOCK
#define SC2336_IO_MUX_UNLOCK
#define CAMERA_ENABLE_OUT_CLOCK(pin,clk)
#define CAMERA_DISABLE_OUT_CLOCK(pin)

#define SC2336_SCCB_ADDR   0x30
#define SC2336_PID         0xcb3a
#define SC2336_SENSOR_NAME "SC2336"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define SC2336_SUPPORT_NUM CONFIG_CAMERA_SC2336_MAX_SUPPORT

static const char *TAG = "sc2336";
static esp_camera_device_t *s_sc2336[SC2336_SUPPORT_NUM];
static uint8_t s_sc2336_index;

enum {
    SC2336_RAW_FORMAT_INDEX0,
    SC2336_RAW_FORMAT_INDEX1,
    SC2336_RAW_FORMAT_INDEX2,
    SC2336_RAW_FORMAT_INDEX3,
    SC2336_RAW_FORMAT_INDEX4,
    SC2336_RAW_FORMAT_INDEX5,
    SC2336_RAW_FORMAT_INDEX6,
    SC2336_RAW_FORMAT_INDEX7,
    SC2336_RAW_FORMAT_INDEX8,
};

static const sensor_isp_info_t sc2336_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1500,
            .hts = 1800,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1800,
            .hts = 900,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1800,
            .hts = 750,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81000000,
            .vts = 1125,
            .hts = 1200,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 66000000,
            .vts = 2250,
            .hts = 1200,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 66000000,
            .vts = 2250,
            .hts = 1200,
        },
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 49500000,
            .vts = 2200,
            .hts = 750,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 67200000,
            .vts = 1000,
            .hts = 2240,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 42000000,
            .vts = 525,
            .hts = 1600,
        }
    },
};

static const sensor_format_t sc2336_format_info[] = {
    {
        //cleaned_0x2e_SC2336_MIPI_24Minput_2lane_405Mbps_10bit_1280x720_30fps
        .index = SC2336_RAW_FORMAT_INDEX0,
        .name = "MIPI_24Minput_2lane_RAW10_1280x720_30fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = MIPI_CSI_OUTPUT_LANE2,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_30fps),
        .bpp = 10,
        .fps = 30,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX0],
        .mipi_info = {
            .mipi_clk = 405000000,
        },
        .reserved = NULL,
    },
    {
        // cleaned_0x2f_SC2336_MIPI_24Minput_2lane_405Mbps_10bit_1280x720_50fps
        .index = SC2336_RAW_FORMAT_INDEX1,
        .name = "MIPI_24Minput_2lane_RAW10_1280x720_50fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = MIPI_CSI_OUTPUT_LANE2,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_50fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_50fps),
        .bpp = 10,
        .fps = 50,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX1],
        .mipi_info = {
            .mipi_clk = 405000000,
        },
        .reserved = NULL,
    },
    {
        // cleaned_0x2f_SC2336_MIPI_24Minput_2lane_405Mbps_10bit_1280x720_60fps
        .index = SC2336_RAW_FORMAT_INDEX2,
        .name = "MIPI_24Minput_2lane_RAW10_1280x720_60fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = MIPI_CSI_OUTPUT_LANE2,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_2lane_720p_60fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_720p_60fps),
        .bpp = 10,
        .fps = 60,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX2],
        .mipi_info = {
            .mipi_clk = 405000000,
        },
        .reserved = NULL,
    },
    {
        // cleaned_0x29_SC2336_MIPI_24Minput_1lane_660Mbps_10bit_1920x1080_25fps
        .index = SC2336_RAW_FORMAT_INDEX3,
        .name = "MIPI_24Minput_1lane_RAW10_1920x1080_25fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = MIPI_CSI_OUTPUT_LANE1,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_1lane_1080p_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_1080p_25fps),
        .bpp = 10,
        .fps = 25,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX3],
        .mipi_info = {
            .mipi_clk = 660000000,
        },
        .reserved = NULL,
    },
    {
        // cleaned_0x28_SC2336_MIPI_24Minput_2lane_330Mbps_10bit_1920x1080_25fps
        .index = SC2336_RAW_FORMAT_INDEX4,
        .name = "MIPI_24Minput_2lane_RAW10_1920x1080_25fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = MIPI_CSI_OUTPUT_LANE2,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_1080p_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1080p_25fps),
        .bpp = 10,
        .fps = 25,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX4],
        .mipi_info = {
            .mipi_clk = 330000000,
        },
        .reserved = NULL,
    },
    {
        // cleaned_0x28_SC2336_MIPI_24Minput_2lane_330Mbps_10bit_1920x1080_25fps
        .index = SC2336_RAW_FORMAT_INDEX5,
        .name = "MIPI_24Minput_2lane_RAW10_1920x1080_25fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = MIPI_CSI_OUTPUT_LANE2,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_2lane_1080p_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1080p_25fps),
        .bpp = 10,
        .fps = 25,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX5],
        .mipi_info = {
            .mipi_clk = 330000000,
        },
        .reserved = NULL,
    },
    {
        // cleaned_0xa7_SC2336_MIPI_24Minput_2lane_336Mbps_10bit_800x800_30fps
        .index = SC2336_RAW_FORMAT_INDEX6,
        .name = "MIPI_24Minput_2lane_RAW10_800*800_30fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = MIPI_CSI_OUTPUT_LANE2,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 800,
        .height = 800,
        .regs = init_reglist_MIPI_2lane_10bit_800x800_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_10bit_800x800_30fps),
        .bpp = 10,
        .fps = 30,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX6],
        .mipi_info = {
            .mipi_clk = 336000000,
        },
        .reserved = NULL,
    },
    {
        // cleaned_0x28_SC2336_MIPI_24Minput_2lane_330Mbps_10bit_1920x1080_25fps
        .index = SC2336_RAW_FORMAT_INDEX7,
        .name = "MIPI_24Minput_lane2_RAW10_640*480_50fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = MIPI_CSI_OUTPUT_LANE2,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 640,
        .height = 480,
        .regs = init_reglist_MIPI_2lane_10bit_640x480_50fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_10bit_640x480_50fps),
        .bpp = 10,
        .fps = 50,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX7],
        .mipi_info = {
            .mipi_clk = 210000000,
        },
        .reserved = NULL,
    },
    {
        // cleaned_0x28_SC2336_MIPI_24Minput_2lane_330Mbps_10bit_1920x1080_25fps
        .index = SC2336_RAW_FORMAT_INDEX8,
        .name = "DVP_24Minput_RAW10_1280*720_30fps",
        .format = CAM_SENSOR_PIXFORMAT_RAW10,
        .port = DVP_OUTPUT_8BITS,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 4,
        .start_pos_y = 4,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_DVP_720p_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_720p_30fps),
        .bpp = 10,
        .fps = 30,
        .isp_info = &sc2336_isp_info[SC2336_RAW_FORMAT_INDEX6],
        .mipi_info = {0},
        .reserved = NULL,
    },
};

static int sc2336_read(uint8_t sccb_port, uint16_t reg, uint8_t *read_buf)
{
    int value = -1;
    value = sccb_read_reg16_val8(sccb_port, SC2336_SCCB_ADDR, reg);
    if (value == -1) {
        ESP_LOGD(TAG, "Read err");
        return value;
    }
    *read_buf = value;
    return 0;
}

static int sc2336_write(uint8_t sccb_port, uint16_t reg, uint8_t data)
{
    return sccb_write_reg16_val8(sccb_port, SC2336_SCCB_ADDR, reg, data);
}

/* write a array of registers  */
static int sc2336_write_array(uint8_t sccb_port, reginfo_t *regarray)
{
    int i = 0, ret = 0;
    while (!ret && regarray[i].reg != SC2336_REG_END) {
        if (regarray[i].reg != SC2336_REG_DELAY) {
            ret = sc2336_write(sccb_port, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(10);
        }
        i++;
    }
    ESP_LOGD(TAG, "count=%d", i);
    return ret;
}

static int sc2336_set_reg_bits(uint8_t sccb_port, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    int ret = 0;
    uint8_t reg_data = 0;

    ret = sc2336_read(sccb_port, reg, &reg_data);
    if (ret < 0) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = sc2336_write(sccb_port, reg, value);
    return ret;
}

static int sc2336_set_test_pattern(esp_camera_device_t *dev, int enable)
{
    return sc2336_set_reg_bits(dev->sccb_port, 0x4501, 3, 1, enable ? 0x01 : 0x00);
}

static int sc2336_hw_reset(esp_camera_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return 0;
}

static int sc2336_soft_reset(esp_camera_device_t *dev)
{
    int ret = sc2336_set_reg_bits(dev->sccb_port, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static int sc2336_get_sensor_id(esp_camera_device_t *dev, sensor_id_t *id)
{
    int ret = -1;
    uint8_t pid_h, pid_l;
    sc2336_read(dev->sccb_port, SC2336_REG_SENSOR_ID_H, &pid_h);
    sc2336_read(dev->sccb_port, SC2336_REG_SENSOR_ID_L, &pid_l);
    uint16_t PID = (pid_h << 8) | pid_l;
    if (PID) {
        id->PID = PID;
        ret = 0;
    }
    return ret;
}

static int sc2336_set_stream(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    ret = sc2336_write(dev->sccb_port, SC2336_REG_SLEEP_MODE, enable ? 0x01 : 0x00);

    // sc2336_set_test_pattern(dev, 1);
    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static int sc2336_set_mirror(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    ret = sc2336_set_reg_bits(dev->sccb_port, 0x3221, 1, 2,  enable ? 0x03 : 0x00);

    return ret;
}

static int sc2336_set_vflip(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    ret = sc2336_set_reg_bits(dev->sccb_port, 0x3221, 5, 2, enable ? 0x03 : 0x00);

    return ret;
}

static int sc2336_query_para_desc(esp_camera_device_t *dev, struct v4l2_query_ext_ctrl *qctrl)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static int sc2336_get_para_value(esp_camera_device_t *dev, struct v4l2_ext_control *ctrl)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static int sc2336_set_para_value(esp_camera_device_t *dev, const struct v4l2_ext_control *ctrl)
{
    esp_err_t ret = ESP_OK;

    switch (ctrl->id) {
    case CAM_SENSOR_VFLIP: {
        ret = sc2336_set_vflip(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_HMIRROR: {
        ret = sc2336_set_mirror(dev, ctrl->value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", ctrl->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static int sc2336_query_support_formats(esp_camera_device_t *dev, sensor_format_array_info_t *formats)
{
    formats->count = ARRAY_SIZE(sc2336_format_info);
    formats->format_array = &sc2336_format_info[0];
    ESP_LOGI(TAG, "f_array=%p", formats->format_array);
    return 0;
}

static int sc2336_query_support_capability(esp_camera_device_t *dev, sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);
    sensor_cap->fmt_raw = 1;
    return 0;
}

static int sc2336_set_format(esp_camera_device_t *dev, const sensor_format_t *format)
{
    int ret = 0;

    ret = sc2336_write_array(dev->sccb_port, (reginfo_t *)format->regs);

    if (ret < 0) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_FAILED_TO_S_FORMAT;
    }

    dev->cur_format = &sc2336_format_info[format->index];

    return ret;
}

static int sc2336_get_format(esp_camera_device_t *dev, sensor_format_t *format)
{
    int ret = -1;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(sensor_format_t));
        ret = 0;
    }
    return ret;
}

static int sc2336_priv_ioctl(esp_camera_device_t *dev, unsigned int cmd, void *arg)
{
    int ret = 0;
    uint8_t regval;
    struct sensor_reg_val *sensor_reg;
    SC2336_IO_MUX_LOCK

    if (cmd & (_IOC_WRITE << _IOC_DIRSHIFT)) {
        switch (cmd) {
        case CAM_SENSOR_IOC_HW_RESET:
            ret = sc2336_hw_reset(dev);
            break;
        case CAM_SENSOR_IOC_SW_RESET:
            ret = sc2336_soft_reset(dev);
            break;
        case CAM_SENSOR_IOC_S_REG:
            sensor_reg = (struct sensor_reg_val *)arg;
            ret = sc2336_write(dev->sccb_port, sensor_reg->regaddr, sensor_reg->value);
            break;
        case CAM_SENSOR_IOC_S_STREAM:
            ret = sc2336_set_stream(dev, *(int *)arg);
            break;
        case CAM_SENSOR_IOC_S_TEST_PATTERN:
            ret = sc2336_set_test_pattern(dev, *(int *)arg);
            break;
        }
    } else {
        switch (cmd) {
        case CAM_SENSOR_IOC_G_REG:
            sensor_reg = (struct sensor_reg_val *)arg;
            ret = sc2336_read(dev->sccb_port, sensor_reg->regaddr, &regval);
            if (ret == ESP_OK) {
                sensor_reg->value = regval;
            }
            break;
        case CAM_SENSOR_IOC_G_CHIP_ID:
            ret = sc2336_get_sensor_id(dev, arg);
            break;
        default:
            break;
        }
    }
    SC2336_IO_MUX_UNLOCK
    return ret;
}

static int sc2336_power_on(esp_camera_device_t *dev)
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

static int sc2336_power_off(esp_camera_device_t *dev)
{
    int ret = 0;

    if (dev->xclk_pin >= 0) {
        CAMERA_DISABLE_OUT_CLOCK(dev->xclk_pin);
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

static esp_camera_ops_t sc2336_ops = {
    .query_para_desc = sc2336_query_para_desc,
    .get_para_value = sc2336_get_para_value,
    .set_para_value = sc2336_set_para_value,
    .query_support_formats = sc2336_query_support_formats,
    .query_support_capability = sc2336_query_support_capability,
    .set_format = sc2336_set_format,
    .get_format = sc2336_get_format,
    .priv_ioctl = sc2336_priv_ioctl
};

// We need manage these devices, and maybe need to add it into the private member of esp_device
esp_camera_device_t *sc2336_csi_detect(esp_camera_driver_config_t *config)
{
    esp_camera_device_t *dev = NULL;

    if (config == NULL) {
        return NULL;
    }

    if (s_sc2336_index >= SC2336_SUPPORT_NUM) {
        ESP_LOGE(TAG, "Only support max %d cameras", SC2336_SUPPORT_NUM);
        return NULL;
    }

    s_sc2336[s_sc2336_index] = calloc(sizeof(esp_camera_device_t), 1);
    if (s_sc2336[s_sc2336_index] == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }
    dev = s_sc2336[s_sc2336_index];
    dev->name = (char *)SC2336_SENSOR_NAME;
    dev->ops = &sc2336_ops;
    dev->sccb_port = config->sccb_port;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;

    // Configure sensor power, clock, and SCCB port
    if (sc2336_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc2336_get_sensor_id(dev, &dev->id) == -1) {
        ESP_LOGE(TAG, "Camera get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.PID != SC2336_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SC2336, PID=0x%x", dev->id.PID);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x with index %d", dev->id.PID, s_sc2336_index);

    s_sc2336_index++;

    return dev;

err_free_handler:
    sc2336_power_off(dev);
    free(dev);

    return NULL;
}

esp_camera_device_t *sc2336_dvp_detect(esp_camera_driver_config_t *config)
{
    ESP_LOGI(TAG, "sc2336_dvp_detect");
    s_sc2336_index++;

    return NULL;
}

#if CONFIG_CAMERA_SC2336_AUTO_DETECT
ESP_CAMERA_DETECT_FN(sc2336_csi_detect, CAMERA_INTF_CSI)
{
    return sc2336_csi_detect(config);
}

ESP_CAMERA_DETECT_FN(sc2336_dvp_detect, CAMERA_INTF_DVP)
{
    return sc2336_dvp_detect(config);
}
#endif
