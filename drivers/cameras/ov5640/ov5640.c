/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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
#include "ov5640_settings.h"

#define OV5640_IO_MUX_LOCK
#define OV5640_IO_MUX_UNLOCK
#define CAMERA_ENABLE_OUT_CLOCK(pin,clk)
#define CAMERA_DISABLE_OUT_CLOCK(pin)

#define OV5640_PID             0x5640
#define OV5640_SENSOR_NAME     "OV5640"
#define OV5640_REG_SENSOR_ID_H 0x300A
#define OV5640_REG_SENSOR_ID_L 0x300B

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define OV5640_SUPPORT_NUM CONFIG_CAMERA_OV5640_MAX_SUPPORT

static uint8_t OV5640_SCCB_ADDR = 0x3C;
static esp_camera_device_t *s_ov5640[OV5640_SUPPORT_NUM];
static uint8_t s_ov5640_index;
static const char *TAG = "ov5640";

enum {
    OV5640_RGB565_FORMAT_INDEX0,
};

static const sensor_format_t ov5640_format_info[] = {
    {
        .index = OV5640_RGB565_FORMAT_INDEX0,
        .name = "MIPI_24Minput_2lane_RGB565_800x800_30fps",
        .format = CAM_SENSOR_PIXFORMAT_RGB565,
        .port = MIPI_CSI_OUTPUT_LANE2,
        .bayer_type = CAM_SENSOR_BAYER_GBRG,
        .hdr_mode = CAM_SENSOR_HDR_LINEAR,
        .xclk = 24000000,
        .start_pos_x = 8,
        .start_pos_y = 8,
        .width = 800,
        .height = 800,
        .regs = input_24M_MIPI_2lane_rgb565_800x800_30fps,
        .regs_size = ARRAY_SIZE(input_24M_MIPI_2lane_rgb565_800x800_30fps),
        .bpp = 16,
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = OV5640_LINE_RATE_RGB565_FROM_RAW8_800x800_50FPS,
        },
        .reserved = NULL,
    },
};

static int ov5640_read(uint8_t sccb_port, uint16_t reg, uint8_t *read_buf)
{
    int value = -1;
    value = sccb_read_reg16_val8(sccb_port, OV5640_SCCB_ADDR, reg);
    if (value == -1) {
        ESP_LOGE(TAG, "Read err");
        return value;
    }
    *read_buf = value;
    return 0;
}

static int ov5640_write(uint8_t sccb_port, uint16_t reg, uint8_t data)
{
    return sccb_write_reg16_val8(sccb_port, OV5640_SCCB_ADDR, reg, data);
}

/* write a array of registers  */
static int ov5640_write_array(uint8_t sccb_port, const reginfo_t *regs, size_t regs_size)
{
    int i = 0, res = 0;
    while (!res && i < regs_size) {
        if (regs[i].reg != OV5640_REG_DELAY_MS) {
            res = sccb_write_reg16_val8(sccb_port, OV5640_SCCB_ADDR, regs[i].reg, regs[i].val);
        } else {
            delay_ms(regs[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "write i=%d", i);
    return res;
}

static int ov5640_set_reg_bits(uint8_t sccb_port, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    int ret = 0;
    uint8_t reg_data = 0;

    ret = ov5640_read(sccb_port, reg, &reg_data);
    if (ret < 0) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = ov5640_write(sccb_port, reg, value);
    return ret;
}

static int ov5640_set_test_pattern(esp_camera_device_t *dev, int enable)
{
    return ov5640_set_reg_bits(dev->sccb_port, PRE_ISP_TEST_SETTING_1, 7, 1, enable ? 0x01 : 0x00);
}

static int ov5640_hw_reset(esp_camera_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return 0;
}

static int ov5640_soft_reset(esp_camera_device_t *dev)
{
    int ret = ov5640_set_reg_bits(dev->sccb_port, SYSTEM_CTROL0, 7, 1, 0x01);
    delay_ms(5);
    return ret;
}

static int ov5640_get_sensor_id(esp_camera_device_t *dev, sensor_id_t *id)
{
    int ret = -1;
    uint8_t pid_h, pid_l;
    ov5640_read(dev->sccb_port, OV5640_REG_SENSOR_ID_H, &pid_h);
    ov5640_read(dev->sccb_port, OV5640_REG_SENSOR_ID_L, &pid_l);
    uint16_t PID = (pid_h << 8) | pid_l;
    if (PID) {
        id->PID = PID;
        ret = 0;
    }
    return ret;
}

static int ov5640_set_stream_dvp(esp_camera_device_t *dev, int on)
{
    return ov5640_write(dev->sccb_port, SYSTEM_CTROL0, on ?
                        OV5640_SYS_CTRL0_SW_PWUP :
                        OV5640_SYS_CTRL0_SW_PWDN);
}

static int ov5640_set_stream_mipi(esp_camera_device_t *dev, int on)
{
    int ret;

    /*
     * Enable/disable the MIPI interface
     *
     * 0x300e = on ? 0x45 : 0x40
     *
     * FIXME: the sensor manual (version 2.03) reports
     * [7:5] = 000  : 1 data lane mode
     * [7:5] = 001  : 2 data lanes mode
     * But this settings do not work, while the following ones
     * have been validated for 2 data lanes mode.
     *
     * [7:5] = 010  : 2 data lanes mode
     * [4] = 0  : Power up MIPI HS Tx
     * [3] = 0  : Power up MIPI LS Rx
     * [2] = 1/0    : MIPI interface enable/disable
     * [1:0] = 01/00: FIXME: 'debug'
     */
    ret = ov5640_write(dev->sccb_port, OV5640_REG_IO_MIPI_CTRL00,
                       on ? 0x45 : 0x40);
    if (ret) {
        return ret;
    }

    ret = ov5640_write(dev->sccb_port, FRAME_CTRL02,
                       on ? 0x00 : 0x0f);
    if (ret) {
        return ret;
    }

    return ov5640_write(dev->sccb_port, SYSTEM_CTROL0,
                        on ? 0x02 : 0x42);
}

static int ov5640_set_stream(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    if (dev->cur_format->port == DVP_OUTPUT_8BITS) {
        ret = ov5640_set_stream_dvp(dev, enable);
    } else {
        ret = ov5640_set_stream_mipi(dev, enable);
    }

    if (!ret) {
        dev->stream_status = enable;
    } else {
        ESP_LOGE(TAG, "Stream set failed");
    }

    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static int ov5640_set_mirror(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    ret = ov5640_set_reg_bits(dev->sccb_port, TIMING_TC_REG21, 1, 1,  enable ? 0x01 : 0x00);

    return ret;
}

static int ov5640_set_vflip(esp_camera_device_t *dev, int enable)
{
    int ret = -1;
    ret = ov5640_set_reg_bits(dev->sccb_port, TIMING_TC_REG20, 1, 1, enable ? 0x01 : 0x00);

    return ret;
}

static int ov5640_query_para_desc(esp_camera_device_t *dev, struct v4l2_query_ext_ctrl *qctrl)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static int ov5640_get_para_value(esp_camera_device_t *dev, struct v4l2_ext_control *ctrl)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static int ov5640_set_para_value(esp_camera_device_t *dev, const struct v4l2_ext_control *ctrl)
{
    esp_err_t ret = ESP_OK;

    switch (ctrl->id) {
    case CAM_SENSOR_VFLIP: {
        ret = ov5640_set_vflip(dev, ctrl->value);
        break;
    }
    case CAM_SENSOR_HMIRROR: {
        ret = ov5640_set_mirror(dev, ctrl->value);
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

static int ov5640_query_support_formats(esp_camera_device_t *dev, sensor_format_array_info_t *formats)
{
    formats->count = ARRAY_SIZE(ov5640_format_info);
    formats->format_array = &ov5640_format_info[0];
    ESP_LOGI(TAG, "f_array=%p", formats->format_array);
    return 0;
}

static int ov5640_query_support_capability(esp_camera_device_t *dev, sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);
    sensor_cap->fmt_raw = 1;
    return 0;
}

static int ov5640_set_format(esp_camera_device_t *dev, const sensor_format_t *format)
{
    int ret = 0;

    ret = ov5640_write_array(dev->sccb_port, (reginfo_t *)format->regs, format->regs_size);

    if (ret < 0) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_FAILED_TO_S_FORMAT;
    }

    dev->cur_format = &ov5640_format_info[format->index];

    return ret;
}

static int ov5640_get_format(esp_camera_device_t *dev, sensor_format_t *format)
{
    int ret = -1;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(sensor_format_t));
        ret = 0;
    }
    return ret;
}

static int ov5640_priv_ioctl(esp_camera_device_t *dev, unsigned int cmd, void *arg)
{
    int ret = 0;
    uint8_t regval;
    struct sensor_reg_val *sensor_reg;
    OV5640_IO_MUX_LOCK

    if (cmd & (_IOC_WRITE << _IOC_DIRSHIFT)) {
        switch (cmd) {
        case CAM_SENSOR_IOC_HW_RESET:
            ret = ov5640_hw_reset(dev);
            break;
        case CAM_SENSOR_IOC_SW_RESET:
            ret = ov5640_soft_reset(dev);
            break;
        case CAM_SENSOR_IOC_S_REG:
            sensor_reg = (struct sensor_reg_val *)arg;
            ret = ov5640_write(dev->sccb_port, sensor_reg->regaddr, sensor_reg->value);
            break;
        case CAM_SENSOR_IOC_S_STREAM:
            ret = ov5640_set_stream(dev, *(int *)arg);
            break;
        case CAM_SENSOR_IOC_S_TEST_PATTERN:
            ret = ov5640_set_test_pattern(dev, *(int *)arg);
            break;
        }
    } else {
        switch (cmd) {
        case CAM_SENSOR_IOC_G_REG:
            sensor_reg = (struct sensor_reg_val *)arg;
            ret = ov5640_read(dev->sccb_port, sensor_reg->regaddr, &regval);
            if (ret == ESP_OK) {
                sensor_reg->value = regval;
            }
            break;
        case CAM_SENSOR_IOC_G_CHIP_ID:
            ret = ov5640_get_sensor_id(dev, arg);
            break;
        default:
            break;
        }
    }
    OV5640_IO_MUX_UNLOCK
    return ret;
}

static int ov5640_power_on(esp_camera_device_t *dev)
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

static int ov5640_power_off(esp_camera_device_t *dev)
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

static esp_camera_ops_t ov5640_ops = {
    .query_para_desc = ov5640_query_para_desc,
    .get_para_value = ov5640_get_para_value,
    .set_para_value = ov5640_set_para_value,
    .query_support_formats = ov5640_query_support_formats,
    .query_support_capability = ov5640_query_support_capability,
    .set_format = ov5640_set_format,
    .get_format = ov5640_get_format,
    .priv_ioctl = ov5640_priv_ioctl
};

// We need manage these devices, and maybe need to add it into the private member of esp_device
esp_camera_device_t *ov5640_csi_detect(esp_camera_driver_config_t *config)
{
    esp_camera_device_t *dev = NULL;

    if (config == NULL) {
        return NULL;
    }

    if (s_ov5640_index >= OV5640_SUPPORT_NUM) {
        ESP_LOGE(TAG, "Only support max %d cameras", OV5640_SUPPORT_NUM);
        return NULL;
    }

    s_ov5640[s_ov5640_index] = calloc(sizeof(esp_camera_device_t), 1);
    if (s_ov5640[s_ov5640_index] == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }
    dev = s_ov5640[s_ov5640_index];
    dev->name = (char *)OV5640_SENSOR_NAME;
    dev->ops = &ov5640_ops;
    dev->sccb_port = config->sccb_port;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;

    // Configure sensor power, clock, and SCCB port
    if (ov5640_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (ov5640_get_sensor_id(dev, &dev->id) == -1) {
        ESP_LOGE(TAG, "Camera get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.PID != OV5640_PID) {
        ESP_LOGE(TAG, "Camera sensor is not OV5640, PID=0x%x", dev->id.PID);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x with index %d", dev->id.PID, s_ov5640_index);

    s_ov5640_index++;

    return dev;

err_free_handler:
    ov5640_power_off(dev);
    free(dev);

    return NULL;
}

esp_camera_device_t *ov5640_dvp_detect(esp_camera_driver_config_t *config)
{
    ESP_LOGI(TAG, "ov5640_dvp_detect");
    s_ov5640_index++;

    return NULL;
}

#if CONFIG_CAMERA_OV5640_AUTO_DETECT
ESP_CAMERA_DETECT_FN(ov5640_csi_detect, CAMERA_INTF_CSI)
{
    return ov5640_csi_detect(config);
}

ESP_CAMERA_DETECT_FN(ov5640_dvp_detect, CAMERA_INTF_DVP)
{
    return ov5640_dvp_detect(config);
}
#endif
