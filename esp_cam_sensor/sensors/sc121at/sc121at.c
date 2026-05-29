/*
 * SPDX-FileCopyrightText: 2026 Shenzhen ALG-TECH Co., Ltd.,
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_chip_info.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "sc121at_settings.h"
#include "sc121at.h"

#define SC121AT_IO_MUX_LOCK(mux)
#define SC121AT_IO_MUX_UNLOCK(mux)
#define SC121AT_ENABLE_OUT_XCLK(pin,clk)
#define SC121AT_DISABLE_OUT_XCLK(pin)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
static const char *TAG = "sc121at";

#if CONFIG_SOC_MIPI_CSI_SUPPORTED

#ifndef CONFIG_CAMERA_SC121AT_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for SC121AT"
#endif

static const uint8_t sc121at_format_default_index = CONFIG_CAMERA_SC121AT_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t sc121at_format_index[] = {
#if CONFIG_CAMERA_SC121AT_MIPI_YUV422_1280X800_25FPS
    0,
#endif
};

static const esp_cam_sensor_format_t sc121at_format_info[] = {
#if CONFIG_CAMERA_SC121AT_MIPI_YUV422_1280X800_25FPS
    {
        .name = "MIPI_2lane_24Minput_YUV422_UYVY_1280x800_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 800,
        .regs = sc121at_mipi_2lane_24Minput_1280x800_yuv422_25fps,
        .regs_size = ARRAY_SIZE(sc121at_mipi_2lane_24Minput_1280x800_yuv422_25fps),
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = SC121AT_LINE_RATE_16BITS_1280x800_25FPS,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static uint8_t get_sc121at_mipi_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(sc121at_format_index); i++) {
        if (sc121at_format_index[i] == sc121at_format_default_index) {
            return i;
        }
    }
    return 0;
}

#endif

static esp_err_t sc121at_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t sc121at_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sc121at_write_array(esp_sccb_io_handle_t sccb_handle, const sc121at_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != SC121AT_REG_END) {
        if (regarray[i].reg != SC121AT_REG_DELAY) {
            ret = sc121at_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "count=%d", i);
    return ret;
}

static esp_err_t sc121at_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sc121at_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = sc121at_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sc121at_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sc121at_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = sc121at_set_reg_bits(dev->sccb_handle, 0x2103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sc121at_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    uint8_t pid_h, pid_l;
    esp_err_t ret = sc121at_read(dev->sccb_handle, SC121AT_REG_SENSOR_ID_H, &pid_h);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read pid_h failed");

    ret = sc121at_read(dev->sccb_handle, SC121AT_REG_SENSOR_ID_L, &pid_l);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read pid_l failed");

    uint16_t pid = (pid_h << 8) | pid_l;
    if (pid) {
        id->pid = pid;
    }
    return ret;
}

static esp_err_t sc121at_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        if (enable) {
            ret = sc121at_write_array(dev->sccb_handle, sc121at_mipi_stream_on);
        } else {
            ret = sc121at_write_array(dev->sccb_handle, sc121at_mipi_stream_off);
        }
    }
#endif
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
        ESP_LOGW(TAG, "Stream control not supported on DVP port");
        ret = ESP_ERR_NOT_SUPPORTED;
    }
#endif

    if (ret == ESP_OK) {
        dev->stream_status = (uint8_t)enable;
    }

    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t sc121at_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
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

static esp_err_t sc121at_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    switch (id) {
    case ESP_CAM_SENSOR_DATA_SEQ:
        *(uint32_t *)arg = ESP_CAM_SENSOR_DATA_SEQ_NONE;
        if (dev->cur_format != NULL) {
            if (dev->cur_format->port == ESP_CAM_SENSOR_MIPI_CSI) {
#if CONFIG_IDF_TARGET_ESP32P4
                if (dev->cur_format->format == ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY) {
                    esp_chip_info_t chip_info;
                    esp_chip_info(&chip_info);
                    unsigned major_rev = chip_info.revision / 100;
                    if (major_rev < 3) {
                        *(uint32_t *)arg = ESP_CAM_SENSOR_DATA_SEQ_WORD_INTERNAL_SWAPPED;
                    }
                }
#endif
            }
        }
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t sc121at_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t sc121at_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    esp_err_t ret = ESP_OK;
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        formats->count = ARRAY_SIZE(sc121at_format_info);
        formats->format_array = &sc121at_format_info[0];
    }
#endif
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
        ESP_LOGW(TAG, "DVP formats not supported yet");
        ret = ESP_ERR_NOT_SUPPORTED;
    }
#endif
    return ret;
}

static esp_err_t sc121at_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_yuv = 1;
    return ESP_OK;
}

static esp_err_t sc121at_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
        if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
            format = &sc121at_format_info[get_sc121at_mipi_actual_format_index()];
        }
#endif
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
        if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
            ESP_LOGW(TAG, "Set format not supported on DVP port");
            return ESP_ERR_NOT_SUPPORTED;
        }
#endif
    }
    ESP_RETURN_ON_FALSE(format != NULL, ESP_ERR_NOT_SUPPORTED, TAG, "format is NULL");
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        ret = sc121at_write_array(dev->sccb_handle, sc121at_mipi_reset_regs);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write reset regs failed");

        ret = sc121at_write_array(dev->sccb_handle, (const sc121at_reginfo_t *)format->regs);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT, TAG, "write format regs failed");

        dev->cur_format = format;
    }
#endif
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
        ESP_LOGW(TAG, "Set format not supported on DVP port");
        ret = ESP_ERR_NOT_SUPPORTED;
    }
#endif

    return ret;
}

static esp_err_t sc121at_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t sc121at_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SC121AT_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sc121at_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sc121at_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc121at_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sc121at_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc121at_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sc121at_get_sensor_id(dev, arg);
        break;
    default:
        ESP_LOGE(TAG, "cmd=%" PRIx32 " is not supported", cmd);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    SC121AT_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sc121at_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC121AT_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
        delay_ms(12);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pin config failed");
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
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pin config failed");
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t sc121at_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC121AT_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t sc121at_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sc121at (%p)", dev);
    if (dev) {
        sc121at_power_off(dev);
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t sc121at_ops = {
    .query_para_desc = sc121at_query_para_desc,
    .get_para_value = sc121at_get_para_value,
    .set_para_value = sc121at_set_para_value,
    .query_support_formats = sc121at_query_support_formats,
    .query_support_capability = sc121at_query_support_capability,
    .set_format = sc121at_set_format,
    .get_format = sc121at_get_format,
    .priv_ioctl = sc121at_priv_ioctl,
    .del = sc121at_delete
};

esp_cam_sensor_device_t *sc121at_detect(esp_cam_sensor_config_t *config)
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

    dev->name = (char *)SC121AT_SENSOR_NAME;
    dev->ops = &sc121at_ops;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (config->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        dev->cur_format = &sc121at_format_info[get_sc121at_mipi_actual_format_index()];
    }
#endif
#if CONFIG_SOC_LCDCAM_CAM_SUPPORTED
    if (config->sensor_port == ESP_CAM_SENSOR_DVP) {
        ESP_LOGW(TAG, "Not support DVP port");
        goto err_free_handler;
    }
#endif

    // Configure sensor power, clock, and SCCB port
    if (sc121at_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc121at_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Camera get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SC121AT_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SC121AT, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sc121at_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SC121AT_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc121at_detect, ESP_CAM_SENSOR_MIPI_CSI, SC121AT_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc121at_detect(config);
}
#endif

#if CONFIG_CAMERA_SC121AT_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc121at_detect, ESP_CAM_SENSOR_DVP, SC121AT_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return sc121at_detect(config);
}
#endif
