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
#include "sti2250_settings.h"
#include "sti2250.h"

typedef struct {
    uint8_t analog_gain_fine;
    uint8_t analog_gain_coarse;
} sti2250_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    volatile sti2250_bank_t reg_bank;  // current bank
    size_t limited_gain_index;
    bool stream_en;
} sti2250_para_t;

struct sti2250_cam {
    sti2250_para_t sti2250_para;
};

#define STI2250_IO_MUX_LOCK(mux)
#define STI2250_IO_MUX_UNLOCK(mux)
#define STI2250_ENABLE_OUT_XCLK(pin,clk)
#define STI2250_DISABLE_OUT_XCLK(pin)

#define STI2250_FETCH_EXP_H(val)     (((val) >> 8) & 0xFF)
#define STI2250_FETCH_EXP_L(val)     ((val) & 0xFF)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_STI2250(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_STI2250_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

#define STI2250_EXPOSURE_TEST_EN 0
#define STI2250_EXPOSURE_TEST_EN_GAIN 0

static const uint8_t s_sti2250_exp_min = 0x04;
static const uint8_t s_sti2250_exp_max_offset = 2;
static const char *TAG = "sti2250";

#ifndef CONFIG_CAMERA_STI2250_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose default format in menuconfig for STI2250"
#endif

#if !CONFIG_CAMERA_STI2250_MIPI_RAW8_800X600_50FPS
#error "Please enable at least one format in menuconfig for STI2250"
#endif

static const uint8_t sti2250_format_default_index = CONFIG_CAMERA_STI2250_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t sti2250_format_index[] = {
#if CONFIG_CAMERA_STI2250_MIPI_RAW8_800X600_50FPS
    0,
#endif
};

static uint8_t get_sti2250_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(sti2250_format_index); i++) {
        if (sti2250_format_index[i] == sti2250_format_default_index) {
            return i;
        }
    }

    return 0;
}

static const uint32_t sti2250_total_gain_val_map[] = {
    // 2x
    2000,
    2152,
    2250,
    2375,
    2500,
    2625,
    2750,
    2875,
    // 3x
    3000,
    3125,
    3250,
    3375,
    3500,
    3625,
    3750,
    3875,
    // 4x
    4000,
    4250,
    4500,
    4750,
    // 5x
    5000,
    5250,
    5500,
    5750,
    // 6x
    6000,
    6250,
    6500,
    6750,
    // 7x
    7000,
    7250,
    7500,
    7750,
    // 8x
    8000,
};

static const sti2250_gain_t sti2250_gain_map[] = {
    // 2x
    {0x00, 0x02},
    {0x01, 0x02},
    {0x02, 0x02},
    {0x03, 0x02},
    {0x04, 0x02},
    {0x05, 0x02},
    {0x06, 0x02},
    {0x07, 0x02},
    // 3x
    {0x08, 0x02},
    {0x09, 0x02},
    {0x0a, 0x02},
    {0x0b, 0x02},
    {0x0c, 0x02},
    {0x0d, 0x02},
    {0x0e, 0x02},
    {0x0f, 0x02},
    // 4x
    {0x00, 0x03},
    {0x01, 0x03},
    {0x02, 0x03},
    {0x03, 0x03},
    // 5x
    {0x04, 0x03},
    {0x05, 0x03},
    {0x06, 0x03},
    {0x07, 0x03},
    // 6x
    {0x08, 0x03},
    {0x09, 0x03},
    {0x0a, 0x03},
    {0x0b, 0x03},
    // 7x
    {0x0c, 0x03},
    {0x0d, 0x03},
    {0x0e, 0x03},
    {0x0f, 0x03},
    // 8x
    {0x00, 0x04},
};

static const esp_cam_sensor_isp_info_t sti2250_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 36000000,
            .vts = 1598,
            .hts = 319, // not used.
            .tline_ns = 12510,
            .gain_def = 0,
            .exp_def = 0x014c,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
};

static const esp_cam_sensor_format_t sti2250_format_info[] = {
#if CONFIG_CAMERA_STI2250_MIPI_RAW8_800X600_50FPS
    {
        .name = "MIPI_1lane_24Minput_RAW8_800x600_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 600,
        .regs = MIPI_1lane_24Minput_RAW8_800x600_50fps,
        .regs_size = ARRAY_SIZE(MIPI_1lane_24Minput_RAW8_800x600_50fps),
        .fps = 50,
        .isp_info = &sti2250_isp_info[0],
        .mipi_info = {
            .mipi_clk = 816000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static esp_err_t sti2250_set_bank(esp_cam_sensor_device_t *dev, sti2250_bank_t bank)
{
    esp_err_t ret = ESP_OK;
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    if (bank != cam_sti2250->sti2250_para.reg_bank) {
        ret = esp_sccb_transmit_reg_a8v8(dev->sccb_handle, STI2250_REG_PAGE_SELECT, bank);
        if (ret == ESP_OK) {
            cam_sti2250->sti2250_para.reg_bank = bank;
        } else {
            ESP_LOGE(TAG, "Bank update failed");
        }
    }
    return ret;
}

static esp_err_t sti2250_read(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, read_buf);
}

static esp_err_t sti2250_write(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a8v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sti2250_write_array(esp_sccb_io_handle_t sccb_handle, sti2250_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && (i < regs_size)) {
        if (regarray[i].reg != STI2250_REG_DELAY) {
            ret = sti2250_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t sti2250_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sti2250_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = sti2250_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sti2250_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    if (sti2250_set_bank(dev, STI2250_BANK1) != ESP_OK) {
        return ESP_FAIL;
    }
    return sti2250_write(dev->sccb_handle, 0x8c, enable & 0xff);
}

static esp_err_t sti2250_hw_reset(esp_cam_sensor_device_t *dev)
{
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
        cam_sti2250->sti2250_para.reg_bank = STI2250_BANK_MAX;
    }

    return ESP_OK;
}

static esp_err_t sti2250_soft_reset(esp_cam_sensor_device_t *dev)
{
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    esp_err_t ret = sti2250_write(dev->sccb_handle, STI2250_REG_SYSTEM, 0xff);
    cam_sti2250->sti2250_para.reg_bank = STI2250_BANK_MAX;
    delay_ms(5);
    return ret;
}

static esp_err_t sti2250_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;
    ret = sti2250_set_bank(dev, STI2250_BANK0);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sti2250_read(dev->sccb_handle, STI2250_REG_CHIP_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sti2250_read(dev->sccb_handle, STI2250_REG_CHIP_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sti2250_trigger_gain(esp_cam_sensor_device_t *dev)
{
    if (sti2250_set_bank(dev, STI2250_BANK0) != ESP_OK) {
        return ESP_FAIL;
    }
    return sti2250_set_reg_bits(dev->sccb_handle, STI2250_REG_EV_UPDATE, 0x04, 0x1, 0x01);
}

static esp_err_t sti2250_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_sti2250_exp_min);
    value_buf = MIN(value_buf, cam_sti2250->sti2250_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);
    if (sti2250_set_bank(dev, STI2250_BANK0) != ESP_OK) {
        return ESP_FAIL;
    }
    ret = sti2250_write(dev->sccb_handle,
                        STI2250_REG_EXP_L,
                        STI2250_FETCH_EXP_L(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "exp reg low write failed");
    ret = sti2250_write(dev->sccb_handle,
                        STI2250_REG_EXP_H,
                        STI2250_FETCH_EXP_H(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "exp reg high write failed");
    cam_sti2250->sti2250_para.exposure_val = value_buf;

    return ret;
}

static esp_err_t sti2250_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    if (u32_val >= cam_sti2250->sti2250_para.limited_gain_index) {
        ESP_LOGW(TAG, "gain_index %" PRIu32 " out of range, clamped to max",
                 u32_val);
        u32_val = cam_sti2250->sti2250_para.limited_gain_index - 1;
    }

    if (sti2250_set_bank(dev, STI2250_BANK0) != ESP_OK) {
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "again index = 0x%" PRIx32, u32_val);
    ret = sti2250_write(dev->sccb_handle,
                        STI2250_REG_A_GAIN_FINE,
                        sti2250_gain_map[u32_val].analog_gain_fine);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg fine write failed");
    ret = sti2250_write(dev->sccb_handle,
                        STI2250_REG_A_GAIN_COARSE,
                        sti2250_gain_map[u32_val].analog_gain_coarse);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg coarse write failed");
    cam_sti2250->sti2250_para.gain_index = u32_val;

    return ret;
}

#if STI2250_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = 0x05;
static bool s_exp_add = true;
TimerHandle_t ae_timer_handle;
static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
#if STI2250_EXPOSURE_TEST_EN_GAIN
    if (s_exp_v >= cam_sti2250->sti2250_para.limited_gain_index) {
        s_exp_add = false;
    } else if (s_exp_v < 1) {
        s_exp_add = true;
    }
    sti2250_set_total_gain_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 1;
    } else {
        s_exp_v -= 1;
    }
#else
    if (s_exp_v >= cam_sti2250->sti2250_para.exposure_max) {
        s_exp_add = false;
    } else if (s_exp_v < s_sti2250_exp_min) {
        s_exp_add = true;
    }
    sti2250_set_exp_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 5;
    } else {
        s_exp_v -= 5;
    }
#endif
    sti2250_trigger_gain(dev);
    ESP_LOGI(TAG, "E=%" PRIu32, s_exp_v);
}
#endif

static esp_err_t sti2250_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    bool enable_bool = enable ? true : false;
    if (cam_sti2250->sti2250_para.stream_en == enable_bool) {
        ESP_LOGW(TAG, "stream has been set to %d", enable);
        return ESP_OK;
    }
    if (sti2250_set_bank(dev, STI2250_BANK0) != ESP_OK) {
        return ESP_FAIL;
    }
    /*This is actually an output state switching function.
     * When output is started, if 1 is written, output is turned off;
     * when output is turned off, if 1 is written, output is started.
     */
    ret = sti2250_write(dev->sccb_handle, 0x10, 0x01);
    if (ret == ESP_OK) {
        dev->stream_status = enable_bool;
        cam_sti2250->sti2250_para.stream_en = enable_bool;
    }
#if STI2250_EXPOSURE_TEST_EN
    ae_timer_handle = xTimerCreate("AE_t", 100 / portTICK_PERIOD_MS, pdTRUE,
                                   (void *)dev, ae_timer_callback);
    xTimerStart(ae_timer_handle, portMAX_DELAY);
#endif
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t sti2250_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = sti2250_set_bank(dev, STI2250_BANK0);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "set bank failed");
    return sti2250_set_reg_bits(dev->sccb_handle, 0x11, 6, 0x01, enable != 0);
}

static esp_err_t sti2250_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = sti2250_set_bank(dev, STI2250_BANK0);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "set bank failed");
    return sti2250_set_reg_bits(dev->sccb_handle, 0x11, 7, 0x01, enable != 0);
}

static esp_err_t sti2250_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    if (dev->cur_format == NULL || dev->cur_format->isp_info == NULL) {
        ESP_LOGE(TAG, "Device format not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_sti2250_exp_min;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - s_sti2250_exp_max_offset;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = EXPOSURE_STI2250_TO_V4L2(s_sti2250_exp_min, dev->cur_format);
        qdesc->number.maximum = EXPOSURE_STI2250_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - s_sti2250_exp_max_offset), dev->cur_format);
        qdesc->number.step = MAX(EXPOSURE_STI2250_TO_V4L2(0x01, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_STI2250_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_sti2250->sti2250_para.limited_gain_index;
        qdesc->enumeration.elements = sti2250_total_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def; // gain index
        break;
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_U8;
        qdesc->u8.size = sizeof(esp_cam_sensor_gh_exp_gain_t);
        break;
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

static esp_err_t sti2250_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_sti2250->sti2250_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_sti2250->sti2250_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t sti2250_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    if (dev->cur_format == NULL) {
        ESP_LOGE(TAG, "sensor format not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = sti2250_set_exp_val(dev, u32_val);
        if (ret != ESP_OK) {
            break;
        }
        ret = sti2250_trigger_gain(dev);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_STI2250(u32_val, dev->cur_format);
        ret = sti2250_set_exp_val(dev, ori_exp);
        if (ret != ESP_OK) {
            break;
        }
        ret = sti2250_trigger_gain(dev);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = sti2250_set_total_gain_val(dev, u32_val);
        if (ret != ESP_OK) {
            break;
        }
        ret = sti2250_trigger_gain(dev);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_STI2250(value->exposure_us, dev->cur_format);
        } else if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = sti2250_set_exp_val(dev, ori_exp);
        if (ret != ESP_OK) {
            break;
        }
        ret = sti2250_set_total_gain_val(dev, value->gain_index);
        if (ret != ESP_OK) {
            break;
        }
        ret = sti2250_trigger_gain(dev);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = sti2250_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = sti2250_set_mirror(dev, *value);
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

static esp_err_t sti2250_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(sti2250_format_info);
    formats->format_array = &sti2250_format_info[0];
    return ESP_OK;
}

static esp_err_t sti2250_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return ESP_OK;
}

static esp_err_t sti2250_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct sti2250_cam *cam_sti2250 = (struct sti2250_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &sti2250_format_info[get_sti2250_actual_format_index()];
    }

    ret = sti2250_write_array(dev->sccb_handle, (sti2250_reginfo_t *)format->regs, format->regs_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_sti2250->sti2250_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_sti2250->sti2250_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_sti2250->sti2250_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - s_sti2250_exp_max_offset;

    return ret;
}

static esp_err_t sti2250_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t sti2250_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    STI2250_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sti2250_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sti2250_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sti2250_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sti2250_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sti2250_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sti2250_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sti2250_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    STI2250_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sti2250_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        STI2250_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to config pwdn_pin: %s", esp_err_to_name(ret));
            return ret;
        }

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
            ESP_LOGE(TAG, "Failed to config reset_pin: %s", esp_err_to_name(ret));
            return ret;
        }

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t sti2250_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        STI2250_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t sti2250_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sti2250 (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        sti2250_power_off(dev);
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t sti2250_ops = {
    .query_para_desc = sti2250_query_para_desc,
    .get_para_value = sti2250_get_para_value,
    .set_para_value = sti2250_set_para_value,
    .query_support_formats = sti2250_query_support_formats,
    .query_support_capability = sti2250_query_support_capability,
    .set_format = sti2250_set_format,
    .get_format = sti2250_get_format,
    .priv_ioctl = sti2250_priv_ioctl,
    .del = sti2250_delete
};

esp_cam_sensor_device_t *sti2250_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct sti2250_cam *cam_sti2250;
    bool power_on_success = false;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_sti2250 = heap_caps_calloc(1, sizeof(struct sti2250_cam), MALLOC_CAP_DEFAULT);
    if (!cam_sti2250) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }
    cam_sti2250->sti2250_para.reg_bank = STI2250_BANK_MAX;
    cam_sti2250->sti2250_para.limited_gain_index = ARRAY_SIZE(sti2250_total_gain_val_map);

    dev->name = (char *)STI2250_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sti2250_ops;
    dev->priv = cam_sti2250;
    dev->cur_format = &sti2250_format_info[get_sti2250_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (sti2250_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }
    power_on_success = true;

    if (sti2250_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != STI2250_PID) {
        ESP_LOGE(TAG, "Camera sensor is not STI2250, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    if (power_on_success) {
        sti2250_power_off(dev);
    }
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_STI2250_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sti2250_detect, ESP_CAM_SENSOR_MIPI_CSI, STI2250_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sti2250_detect(config);
}
#endif
