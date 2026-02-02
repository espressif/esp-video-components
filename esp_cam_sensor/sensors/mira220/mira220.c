/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "mira220_settings.h"
#include "mira220.h"

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
} mira220_para_t;

typedef struct {
    esp_cam_sensor_output_format_t color_fiter;
    esp_cam_sensor_isp_info_v1_t bayer_type;
    int lane_number;
    int mipi_speed;
    int bit_depth;

    uint32_t row_lenght;
    uint32_t hsize_reg_val;
    uint32_t vsize_reg_val;
    uint32_t vblank_reg_val;
    uint32_t vblank_reg_val_max;
    uint32_t vblank_reg_val_min;
    float row_length_us;

    float time_frame_us;
    float sensor_fps;
    float sensor_fps_min;
    float sensor_fps_max;

    float time_exposure_us;
    float time_exp_max_us;
    uint32_t exp_reg_val;
    uint32_t exp_reg_min_val;
    uint32_t exp_reg_max_val;
} sensor_settings_t;

struct mira220_cam {
    mira220_para_t mira220_para;
    sensor_settings_t settings;
};

#define MIRA220_IO_MUX_LOCK(mux)
#define MIRA220_IO_MUX_UNLOCK(mux)
#define MIRA220_ENABLE_OUT_XCLK(pin,clk)
#define MIRA220_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_MIRA220(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_MIRA220_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))
#define EXPOSURE_MIRA220_MAX_VAL(sf)          \
    ((uint32_t)(((sf)->isp_info->isp_v1_info.vts - 1928.0 / ((sf)->isp_info->isp_v1_info.hts) - 0.5)))

#define MIRA220_FETCH_EXP_H(val)     (uint8_t)(((val) >> 8) & 0xFF)
#define MIRA220_FETCH_EXP_L(val)     (uint8_t)((val) & 0xFF)

#define MIRA220_EXP_MAX_OFFSET 0x04
#define MIRA220_EXPOSURE_TEST_EN 0
#define MIRA220_FPS_TEST_EN      0

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

static const uint8_t s_mira220_exp_min = 0x01;
static const char *TAG = "mira220";

static const esp_cam_sensor_isp_info_t mira220_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .vts = 4086, // 600 + 3486(Vsize + Vblank)
            .hts = 600,
            .pclk = 65536000,
            .tline_ns = 15625,
            .gain_def = 0,
            .exp_def = 0x0d,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    }
};

static const esp_cam_sensor_format_t mira220_format_info[] = {
    {
        .name = "MIPI_2lane_RAW8_1024_600_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 38400000,
        .width = 1024,
        .height = 600,
        .regs = mira220_mipi_2lane_24Minput_1024x600_raw8_15fps,
        .regs_size = ARRAY_SIZE(mira220_mipi_2lane_24Minput_1024x600_raw8_15fps),
        .fps = 15,
        .isp_info = &mira220_isp_info[0],
        .mipi_info = {
            .mipi_clk = 800000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    }
};

static esp_err_t mira220_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t mira220_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

static esp_err_t mira220_write_array(esp_sccb_io_handle_t sccb_handle, const mira220_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    delay_ms(100);
    while (regarray[i].reg != MIRA220_REG_END) {
        ret = mira220_write(sccb_handle, regarray[i].reg, regarray[i].val);
        if (ret != ESP_OK) {
            break;
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t mira220_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = mira220_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    reg_data = (reg_data & ~mask) | ((value << offset) & mask);
    ret = mira220_write(sccb_handle, reg, reg_data);
    return ret;
}

static esp_err_t mira220_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return mira220_write(dev->sccb_handle, 0x2091, enable ? 0x01 : 0x00);
}

static esp_err_t mira220_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t mira220_soft_reset(esp_cam_sensor_device_t *dev)
{
    return mira220_write(dev->sccb_handle, 0x0040, 0x01);
}

static esp_err_t mira220_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h = 0;
    uint8_t pid_l = 0;

    /* The sensor must be powered again(pwdn pins) to be able to read the CHIP_ID register*/
    ret = mira220_read(dev->sccb_handle, MIRA220_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = mira220_read(dev->sccb_handle, MIRA220_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t mira220_otp_read(esp_cam_sensor_device_t *dev, uint8_t addr, uint8_t offset,
                                  uint8_t *val)
{
    esp_err_t ret;
    uint8_t readback;

    ret = mira220_write(dev->sccb_handle, 0x0086, addr);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = mira220_write(dev->sccb_handle, MIRA220_REG_OTP_CMD_REG, 0x02);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = mira220_read(dev->sccb_handle, (0x0082 + offset), &readback);
    if (ret != ESP_OK) {
        return ret;
    }

    *val = readback & 0xFF;
    return ESP_OK;
}

static esp_err_t mira220_otp_power_on(esp_cam_sensor_device_t *dev)
{
    return mira220_write(dev->sccb_handle, MIRA220_REG_OTP_CMD_REG, MIRA220_REG_OTP_CMD_UP);
}

static esp_err_t mira220_otp_power_off(esp_cam_sensor_device_t *dev)
{
    return mira220_write(dev->sccb_handle, MIRA220_REG_OTP_CMD_REG, MIRA220_REG_OTP_CMD_DOWN);
}

static esp_err_t mira220_get_sensor_device_revision(esp_cam_sensor_device_t *dev, uint8_t *revision)
{
    esp_err_t ret = ESP_FAIL;

    mira220_otp_power_on(dev);

    delay_ms(1);

    ret = mira220_otp_read(dev, 0x3a, 0, revision);
    mira220_otp_power_off(dev);

    return ret;
}

static esp_err_t mira220_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_mira220_exp_min);
    value_buf = MIN(value_buf, cam_mira220->mira220_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);

    esp_err_t ret = mira220_write(dev->sccb_handle, MIRA220_REG_EXP_L, MIRA220_FETCH_EXP_L(value_buf));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write exposure low byte");
        return ret;
    }

    ret = mira220_write(dev->sccb_handle, MIRA220_REG_EXP_H, MIRA220_FETCH_EXP_H(value_buf));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write exposure high byte");
        return ret;
    }

    cam_mira220->mira220_para.exposure_val = value_buf;
    cam_mira220->settings.exp_reg_val = value_buf;

    return ESP_OK;
}

#if MIRA220_FPS_TEST_EN
/* Try to generate a new initialization list and add its description, mimicking mira220_mipi_2lane_24Minput_1024x600_raw8_15fps. */
static esp_err_t mira220_set_fps(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    float t_frame = 1.0 / u32_val;
    int xclk = dev->cur_format->xclk;
    float vblank = (t_frame / (cam_mira220->settings.row_lenght / xclk)) - cam_mira220->settings.vsize_reg_val;
    uint32_t vblank_val = (vblank);

    if (vblank_val < s_mira220_settings.VBLANK_reg_val_min ||
            vblank_val > s_mira220_settings.VBLANK_reg_val_max) {
        return ESP_ERR_INVALID_ARG;
    }

    cam_mira220->settings.vblank_reg_val = vblank_val;
    mira220_write(dev->sccb_handle, MIRA220_REG_VBLANK_L, vblank_val & 0x00FF);
    mira220_write(dev->sccb_handle, MIRA220_REG_VBLANK_H, (vblank_val & 0xFF00) >> 8);
    ESP_LOGD(TAG, "VBLANK_L=0x%" PRIx32", VBLANK_H=0x%" PRIx32, vblank_val & 0x00FF, (vblank_val & 0xFF00) >> 8);
    ESP_LOGD(TAG, "SET VBLANK=0x%" PRIx32" to FPS=0x%" PRIx32, cam_mira220->settings.vblank_reg_val, u32_val);
    return ESP_OK;
}
#endif

static void mira220_get_settings(esp_cam_sensor_device_t *dev)
{
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    sensor_settings_t *st = &cam_mira220->settings;
    uint8_t row_lenght_MSB, row_lenght_LSB;
    mira220_read(dev->sccb_handle, MIRA220_REG_SENSOR_ID_L, &row_lenght_LSB);
    mira220_read(dev->sccb_handle, MIRA220_REG_SENSOR_ID_H, &row_lenght_MSB);
    uint8_t exp_reg_val_MSB, exp_reg_val_LSB;
    mira220_read(dev->sccb_handle, MIRA220_REG_EXP_L, &exp_reg_val_LSB);
    mira220_read(dev->sccb_handle, MIRA220_REG_EXP_H, &exp_reg_val_MSB);
    uint8_t vsize_reg_val_MSB, vsize_reg_val_LSB;
    mira220_read(dev->sccb_handle, MIRA220_REG_VSIZE_L, &vsize_reg_val_LSB);
    mira220_read(dev->sccb_handle, MIRA220_REG_VSIZE_H, &vsize_reg_val_MSB);
    uint8_t vblank_reg_val_MSB, vblank_reg_val_LSB;
    mira220_read(dev->sccb_handle, MIRA220_REG_VBLANK_L, &vblank_reg_val_LSB);
    mira220_read(dev->sccb_handle, MIRA220_REG_VBLANK_H, &vblank_reg_val_MSB);

    st->row_lenght = (row_lenght_MSB * 256 + row_lenght_LSB);
    st->exp_reg_val = (exp_reg_val_MSB * 256 + exp_reg_val_LSB);
    st->vsize_reg_val = (vsize_reg_val_MSB * 256 + vsize_reg_val_LSB);
    st->vblank_reg_val = (vblank_reg_val_MSB * 256 + vblank_reg_val_LSB);
    st->row_length_us = st->row_lenght * 1000000 / (mira220_format_info->xclk / 1.0);
    st->time_exposure_us = st->row_length_us * st->exp_reg_val;
    st->time_frame_us = (1000000.0f / (mira220_format_info->xclk / 1.0)) * st->row_lenght * (st->vsize_reg_val + st->vblank_reg_val);
    st->sensor_fps = 1000000.0f / st->time_frame_us;
    st->time_exp_max_us = st->time_frame_us - (1000000 * 1928.0f / (mira220_format_info->xclk / 1.0));
    st->exp_reg_min_val = 0x1;
    st->exp_reg_max_val = (st->time_exp_max_us / st->row_length_us);
    ESP_LOGD(TAG, "VTS = 0x%" PRIx32", fps = %f, exp max reg=0x%" PRIx32", exp_max_us = %f", st->vsize_reg_val, st->sensor_fps, st->exp_reg_max_val, st->time_exp_max_us);

    uint32_t vblank_max = 0xFFFF;
    st->sensor_fps_min = (mira220_format_info->xclk / 1.0) / (st->row_lenght * (st->vsize_reg_val + vblank_max));
    st->vblank_reg_val_min = (1928.0 / st->row_lenght) + 11;
    st->sensor_fps_max = (mira220_format_info->xclk / 1.0) / (st->row_lenght * (st->vsize_reg_val + st->vblank_reg_val_min));
    st->vblank_reg_val_max = 0xFFFF;
}

#if MIRA220_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = s_mira220_exp_min;
static bool s_exp_add = true;
static TimerHandle_t ae_timer_handle = NULL;

static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    if (dev == NULL) {
        return;
    }
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;

    // Exposure mode: s_exp_v is exposure value
    uint32_t min_exp = s_mira220_exp_min;
    uint32_t max_exp = cam_mira220->mira220_para.exposure_max;
    const uint32_t exp_step = 50;  // Step size for exposure adjustment

    // Check boundaries first and reverse direction if needed
    if (s_exp_add) {
        // Incrementing: check if reached maximum
        if (s_exp_v >= max_exp) {
            // Reached maximum, reverse direction to decrement
            s_exp_add = false;
            if (s_exp_v > max_exp) {
                s_exp_v = max_exp;  // Clamp to maximum if overflowed
            }
        } else {
            // Not at maximum, continue incrementing
            s_exp_v += exp_step;
            // Clamp to maximum if step would exceed it
            if (s_exp_v > max_exp) {
                s_exp_v = max_exp;
            }
        }
    } else {
        // Decrementing: check if reached minimum
        if (s_exp_v <= min_exp) {
            // Reached minimum, reverse direction to increment
            s_exp_add = true;
            if (s_exp_v < min_exp) {
                s_exp_v = min_exp;  // Clamp to minimum if underflowed
            }
        } else {
            // Not at minimum, continue decrementing
            if (s_exp_v > exp_step) {
                s_exp_v -= exp_step;
            } else {
                s_exp_v = min_exp;  // Clamp to minimum if step would go below it
            }
        }
    }

    // Apply the exposure value
    mira220_set_exp_val(dev, s_exp_v);
    ESP_LOGI(TAG, "Exposure=0x%" PRIx32 " (direction=%s)", s_exp_v, s_exp_add ? "UP" : "DOWN");
}
#endif

static esp_err_t mira220_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;

    ret = mira220_write(dev->sccb_handle, MIRA220_REG_MODE, enable ? 0x10 : 0x02);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set stream mode");
        return ret;
    }

    delay_ms(10);

    ret = mira220_write(dev->sccb_handle, MIRA220_REG_START, enable ? 0x01 : 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start stream");
        return ret;
    }

    delay_ms(10);
    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);

#if MIRA220_EXPOSURE_TEST_EN
    if (enable) {
        // Start test timer when stream is enabled
        if (ae_timer_handle == NULL) {
            // Initialize test value to minimum
            struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
            s_exp_v = s_mira220_exp_min;  // Start from minimum exposure
            s_exp_add = true;  // Start with increment direction
            ae_timer_handle = xTimerCreate("AE_t", 100 / portTICK_PERIOD_MS, pdTRUE,
                                           (void *)dev, ae_timer_callback);
            if (ae_timer_handle != NULL) {
                xTimerStart(ae_timer_handle, portMAX_DELAY);
            }
        }
    } else {
        // Stop and delete test timer when stream is disabled
        if (ae_timer_handle != NULL) {
            xTimerStop(ae_timer_handle, portMAX_DELAY);
            xTimerDelete(ae_timer_handle, portMAX_DELAY);
            ae_timer_handle = NULL;
        }
    }
#endif
    return ret;
}

static esp_err_t mira220_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return mira220_set_reg_bits(dev->sccb_handle, MIRA220_REG_HFLIP, 0, 1, enable ? 0x01 : 0x00);
}

static esp_err_t mira220_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return mira220_set_reg_bits(dev->sccb_handle, MIRA220_REG_VFLIP, 0, 1, enable ? 0x01 : 0x00);
}

static esp_err_t mira220_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_mira220_exp_min;
        qdesc->number.maximum = cam_mira220->mira220_para.exposure_max;
        qdesc->number.step = 0x1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = EXPOSURE_MIRA220_TO_V4L2(s_mira220_exp_min, dev->cur_format);
        qdesc->number.maximum = EXPOSURE_MIRA220_TO_V4L2(cam_mira220->mira220_para.exposure_max, dev->cur_format);
        qdesc->number.step = MAX(EXPOSURE_MIRA220_TO_V4L2(0x01, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_MIRA220_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    default:
        ESP_LOGD(TAG, "id=%" PRIx32 " is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ret;
}

static esp_err_t mira220_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_mira220->mira220_para.exposure_val;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t mira220_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = mira220_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_MIRA220(u32_val, dev->cur_format);
        ret = mira220_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = mira220_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = mira220_set_mirror(dev, *value);
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

static esp_err_t mira220_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(mira220_format_info);
    formats->format_array = &mira220_format_info[0];
    return ESP_OK;
}

static esp_err_t mira220_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return ESP_OK;
}

static esp_err_t mira220_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &mira220_format_info[CONFIG_CAMERA_MIRA220_MIPI_IF_FORMAT_INDEX_DEFAULT];
    }

    ret = mira220_write_array(dev->sccb_handle, format->regs);
    ESP_LOGD(TAG, "%s", format->name);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_mira220->mira220_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_mira220->mira220_para.exposure_max = EXPOSURE_MIRA220_MAX_VAL(dev->cur_format);
    // Only used to check sensor information, and does not check the return value.
    mira220_get_settings(dev);
    ESP_LOGD(TAG, "exp max reg=%" PRIx32, cam_mira220->mira220_para.exposure_max);

    return ESP_OK;
}

static esp_err_t mira220_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t mira220_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    MIRA220_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = mira220_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = mira220_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = mira220_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        ret = mira220_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        ret = mira220_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = mira220_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, arg);
        ret = mira220_get_sensor_id(dev, arg);
        break;
    default:
        ESP_LOGE(TAG, "cmd=%" PRIx32 " is not supported", cmd);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    MIRA220_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t mira220_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        MIRA220_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);
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

static esp_err_t mira220_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        MIRA220_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t mira220_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del mira220 (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        // Note: caller should set their pointer to NULL after calling this function
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t mira220_ops = {
    .query_para_desc = mira220_query_para_desc,
    .get_para_value = mira220_get_para_value,
    .set_para_value = mira220_set_para_value,
    .query_support_formats = mira220_query_support_formats,
    .query_support_capability = mira220_query_support_capability,
    .set_format = mira220_set_format,
    .get_format = mira220_get_format,
    .priv_ioctl = mira220_priv_ioctl,
    .del = mira220_delete
};

esp_cam_sensor_device_t *mira220_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct mira220_cam *cam_mira220;
    uint8_t revision = 0;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_mira220 = heap_caps_calloc(1, sizeof(struct mira220_cam), MALLOC_CAP_DEFAULT);
    if (!cam_mira220) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)MIRA220_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &mira220_ops;
    dev->cur_format = &mira220_format_info[CONFIG_CAMERA_MIRA220_MIPI_IF_FORMAT_INDEX_DEFAULT];
    // init para
    cam_mira220->mira220_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_mira220->mira220_para.exposure_max = EXPOSURE_MIRA220_MAX_VAL(dev->cur_format);
    dev->priv = cam_mira220;

    // Configure sensor power, clock, and SCCB port
    if (mira220_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (mira220_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != MIRA220_PID) { // todo, Try using a more stable ID
        ESP_LOGE(TAG, "Camera sensor is not MIRA220, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    mira220_get_sensor_device_revision(dev, &revision);
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x, revision=0x%x", dev->id.pid, revision);

    return dev;

err_free_handler:
    // mira220_power_off is safe to call even if power_on failed
    mira220_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_MIRA220_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(mira220_detect, ESP_CAM_SENSOR_MIPI_CSI, MIRA220_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return mira220_detect(config);
}
#endif
