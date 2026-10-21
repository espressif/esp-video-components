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
#include "jxh63p_settings.h"
#include "jxh63p.h"

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_gain_index;
} jxh63p_para_t;

struct jxh63p_cam {
    jxh63p_para_t jxh63p_para;
};

#define JXH63P_IO_MUX_LOCK(mux)
#define JXH63P_IO_MUX_UNLOCK(mux)
#define JXH63P_ENABLE_OUT_XCLK(pin,clk)
#define JXH63P_DISABLE_OUT_XCLK(pin)

#define JXH63P_FETCH_EXP_H(val)     (((val) >> 8) & 0xFF)
#define JXH63P_FETCH_EXP_L(val)     ((val) & 0xFF)

#define JXH63P_LANES            1
#define JXH63P_LINK_FREQ        432000000
/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define JXH63P_PIXEL_RATE       (JXH63P_LINK_FREQ * 2 * JXH63P_LANES / 10)

#define JXH63P_MODE_SW_STANDBY      0x40
#define JXH63P_MODE_STREAMING       0x00    /* Streaming mode */

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_JXH63P(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_JXH63P_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

#define JXH63P_EXPOSURE_TEST_EN 0
#define JXH63P_EXPOSURE_TEST_EN_GAIN 0

static const uint8_t s_jxh63p_exp_min = 0x04;
static const uint8_t s_jxh63p_exp_max_offset = 4;
static const char *TAG = "jxh63p";

#ifndef CONFIG_CAMERA_JXH63P_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose default format in menuconfig for JXH63P"
#endif

#if !CONFIG_CAMERA_JXH63P_MIPI_RAW10_1280X720_30FPS && !CONFIG_CAMERA_JXH63P_MIPI_RAW10_640X480_30FPS
#error "Please choose at least one format in menuconfig for JXH63P"
#endif

static const uint8_t jxh63p_format_default_index = CONFIG_CAMERA_JXH63P_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t jxh63p_format_index[] = {
#if CONFIG_CAMERA_JXH63P_MIPI_RAW10_1280X720_30FPS
    0,
#endif
#if CONFIG_CAMERA_JXH63P_MIPI_RAW10_640X480_30FPS
    1,
#endif
};

static uint8_t get_jxh63p_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(jxh63p_format_index); i++) {
        if (jxh63p_format_index[i] == jxh63p_format_default_index) {
            return i;
        }
    }

    return 0;
}

static const uint32_t jxh63p_total_gain_val_map[] = {
    // === PGA[6:4] = 0 (1x 基础段) ===
    1000, 1063, 1125, 1188, 1250, 1313, 1375, 1438, // 0x00 - 0x07
    1500, 1563, 1625, 1688, 1750, 1813, 1875, 1938, // 0x08 - 0x0F

    // === PGA[6:4] = 1 (2x 基础段) ===
    2000, 2125, 2250, 2375, 2500, 2625, 2750, 2875, // 0x10 - 0x17
    3000, 3125, 3250, 3375, 3500, 3625, 3750, 3875, // 0x18 - 0x1F

    // === PGA[6:4] = 2 (4x 基础段) ===
    4000, 4250, 4500, 4750, 5000, 5250, 5500, 5750, // 0x20 - 0x27
    6000, 6250, 6500, 6750, 7000, 7250, 7500, 7750, // 0x28 - 0x2F

    // === PGA[6:4] = 3 (8x 基础段) ===
    8000, 8500, 9000, 9500, 10000, 10500, 11000, 11500, // 0x30 - 0x37
    12000, 12500, 13000, 13500, 14000, 14500, 15000, 15500  // 0x38 - 0x3F
};

static const uint8_t jxh63p_gain_map[] = {
    // === PGA[6:4] = 0 ===
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,

    // === PGA[6:4] = 1 ===
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,

    // === PGA[6:4] = 2 ===
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,

    // === PGA[6:4] = 3 ===
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

static const esp_cam_sensor_isp_info_t jxh63p_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 43200000,
            .hts = 1920,
            .vts = 750,
            .tline_ns = 44444,
            .gain_def = 0x10,
            .exp_def = 0x00E1,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 43200000,
            .hts = 1920,
            .vts = 750,
            .tline_ns = 44444,
            .gain_def = 0x10,
            .exp_def = 0x00E1,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
};

static const esp_cam_sensor_format_t jxh63p_format_info[] = {
#if CONFIG_CAMERA_JXH63P_MIPI_RAW10_1280X720_30FPS
    {
        .name = "MIPI_1lane_24Minput_RAW10_1280x720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = MIPI_1lane_24Minput_RAW10_1280x720_30fps,
        .regs_size = ARRAY_SIZE(MIPI_1lane_24Minput_RAW10_1280x720_30fps),
        .fps = 30,
        .isp_info = &jxh63p_isp_info[0],
        .mipi_info = {
            .mipi_clk = 432000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_JXH63P_MIPI_RAW10_640X480_30FPS
    {
        .name = "MIPI_1lane_24Minput_RAW10_640x480_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = MIPI_1lane_24Minput_RAW10_640x480_30fps,
        .regs_size = ARRAY_SIZE(MIPI_1lane_24Minput_RAW10_640x480_30fps),
        .fps = 30,
        .isp_info = &jxh63p_isp_info[1],
        .mipi_info = {
            .mipi_clk = 432000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static esp_err_t jxh63p_read(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, read_buf);
}

static esp_err_t jxh63p_write(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a8v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t jxh63p_write_array(esp_sccb_io_handle_t sccb_handle, jxh63p_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && (i < regs_size)) {
        if (regarray[i].reg != JXH63P_REG_DELAY) {
            ret = jxh63p_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGW(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t jxh63p_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = jxh63p_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = jxh63p_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t jxh63p_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return jxh63p_write(dev->sccb_handle, JXH63P_REG_TEST_PATTERN, enable ? 0x01 : 0x00);
}

static esp_err_t jxh63p_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ESP_OK;
}

static esp_err_t jxh63p_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = jxh63p_set_reg_bits(dev->sccb_handle, JXH63P_REG_CTRL_MODE, 7, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t jxh63p_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = jxh63p_read(dev->sccb_handle, JXH63P_REG_CHIP_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = jxh63p_read(dev->sccb_handle, JXH63P_REG_CHIP_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t jxh63p_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct jxh63p_cam *cam_jxh63p = (struct jxh63p_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_jxh63p_exp_min);
    value_buf = MIN(value_buf, cam_jxh63p->jxh63p_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);

    ret = jxh63p_write(dev->sccb_handle,
                       JXH63P_REG_LONG_EXPO_LOW_REG,
                       JXH63P_FETCH_EXP_L(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "exp reg low write failed");
    ret = jxh63p_write(dev->sccb_handle,
                       JXH63P_REG_LONG_EXPO_HIGH_REG,
                       JXH63P_FETCH_EXP_H(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "exp reg high write failed");
    cam_jxh63p->jxh63p_para.exposure_val = value_buf;

    return ret;
}

static esp_err_t jxh63p_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct jxh63p_cam *cam_jxh63p = (struct jxh63p_cam *)dev->priv;
    if (u32_val >= cam_jxh63p->jxh63p_para.limited_gain_index) {
        ESP_LOGW(TAG, "gain_index %" PRIu32 " out of range, clamped to max",
                 u32_val);
        u32_val = cam_jxh63p->jxh63p_para.limited_gain_index - 1;
    }

    ESP_LOGD(TAG, "again index = 0x%" PRIx32, u32_val);
    ret = jxh63p_write(dev->sccb_handle, JXH63P_REG_AEC_PK_LONG_GAIN, jxh63p_gain_map[u32_val]);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg write failed");
    cam_jxh63p->jxh63p_para.gain_index = u32_val;

    return ret;
}

#if JXH63P_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = 0x05;
static bool s_exp_add = true;
TimerHandle_t ae_timer_handle;
static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    struct jxh63p_cam *cam_jxh63p = (struct jxh63p_cam *)dev->priv;
#if JXH63P_EXPOSURE_TEST_EN_GAIN
    if (s_exp_v >= cam_jxh63p->jxh63p_para.limited_gain_index) {
        s_exp_add = false;
    } else if (s_exp_v < 1) {
        s_exp_add = true;
    }
    jxh63p_set_total_gain_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 1;
    } else {
        s_exp_v -= 1;
    }
#else
    if (s_exp_v >= cam_jxh63p->jxh63p_para.exposure_max) {
        s_exp_add = false;
    } else if (s_exp_v < s_jxh63p_exp_min) {
        s_exp_add = true;
    }
    jxh63p_set_exp_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 5;
    } else {
        s_exp_v -= 5;
    }
#endif
    ESP_LOGI(TAG, "E=%" PRIu32, s_exp_v);
}
#endif

static esp_err_t jxh63p_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    bool enable_bool = enable ? true : false;

    /* todo, pm_runtime_get_sync */
    ret = jxh63p_set_reg_bits(dev->sccb_handle, JXH63P_REG_CTRL_MODE, 6, 1, enable_bool ? 0 : 1);
    if (ret == ESP_OK) {
        dev->stream_status = enable_bool;
    }

#if JXH63P_EXPOSURE_TEST_EN
    ae_timer_handle = xTimerCreate("AE_t", 100 / portTICK_PERIOD_MS, pdTRUE,
                                   (void *)dev, ae_timer_callback);
    xTimerStart(ae_timer_handle, portMAX_DELAY);
#endif
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t jxh63p_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return jxh63p_set_reg_bits(dev->sccb_handle, JXH63P_REG_CTRL_MODE, 5, 0x01, enable != 0);
}

static esp_err_t jxh63p_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return jxh63p_set_reg_bits(dev->sccb_handle, JXH63P_REG_CTRL_MODE, 4, 0x01, enable != 0);
}

static esp_err_t jxh63p_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    if (dev->cur_format == NULL || dev->cur_format->isp_info == NULL) {
        ESP_LOGE(TAG, "Device format not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    struct jxh63p_cam *cam_jxh63p = (struct jxh63p_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_jxh63p_exp_min;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - s_jxh63p_exp_max_offset;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = EXPOSURE_JXH63P_TO_V4L2(s_jxh63p_exp_min, dev->cur_format);
        qdesc->number.maximum = EXPOSURE_JXH63P_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - s_jxh63p_exp_max_offset), dev->cur_format);
        qdesc->number.step = MAX(EXPOSURE_JXH63P_TO_V4L2(0x01, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_JXH63P_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_jxh63p->jxh63p_para.limited_gain_index;
        qdesc->enumeration.elements = jxh63p_total_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def; // gain index
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

static esp_err_t jxh63p_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct jxh63p_cam *cam_jxh63p = (struct jxh63p_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_jxh63p->jxh63p_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_jxh63p->jxh63p_para.gain_index;
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t val = cam_jxh63p->jxh63p_para.exposure_val;
        *(uint32_t *)arg = EXPOSURE_JXH63P_TO_V4L2(val, dev->cur_format);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR: {
        /*
         * Read the live CTRL_MODE bits instead of a software cache.
         * Format reload writes {0x12, 0x40}/{0x12, 0x00}, and soft/hard
         * reset or IOC_S_REG(0x12) can also clear bit5/bit4 without going
         * through set_mirror()/set_vflip(). Returning a stale cache would
         * make VIDIOC_G_EXT_CTRLS disagree with the hardware state.
         * STREAMON/STREAMOFF only touches bit6 via RMW and preserves these bits.
         */
        uint8_t ctrl_mode = 0;
        ret = jxh63p_read(dev->sccb_handle, JXH63P_REG_CTRL_MODE, &ctrl_mode);
        if (ret != ESP_OK) {
            break;
        }
        if (id == ESP_CAM_SENSOR_HMIRROR) {
            *(int *)arg = (ctrl_mode >> 5) & 0x01;
        } else {
            *(int *)arg = (ctrl_mode >> 4) & 0x01;
        }
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t jxh63p_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
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
        ret = jxh63p_set_exp_val(dev, u32_val);
        if (ret != ESP_OK) {
            break;
        }
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_JXH63P(u32_val, dev->cur_format);
        ret = jxh63p_set_exp_val(dev, ori_exp);
        if (ret != ESP_OK) {
            break;
        }
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = jxh63p_set_total_gain_val(dev, u32_val);
        if (ret != ESP_OK) {
            break;
        }
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = jxh63p_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = jxh63p_set_mirror(dev, *value);
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

static esp_err_t jxh63p_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(jxh63p_format_info);
    formats->format_array = &jxh63p_format_info[0];
    return ESP_OK;
}

static esp_err_t jxh63p_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return ESP_OK;
}

static esp_err_t jxh63p_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct jxh63p_cam *cam_jxh63p = (struct jxh63p_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &jxh63p_format_info[get_jxh63p_actual_format_index()];
    }

    ret = jxh63p_write_array(dev->sccb_handle, (jxh63p_reginfo_t *)format->regs, format->regs_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_jxh63p->jxh63p_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_jxh63p->jxh63p_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_jxh63p->jxh63p_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - s_jxh63p_exp_max_offset;

    return ret;
}

static esp_err_t jxh63p_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t jxh63p_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    JXH63P_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = jxh63p_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = jxh63p_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = jxh63p_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = jxh63p_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = jxh63p_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = jxh63p_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = jxh63p_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    JXH63P_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t jxh63p_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        JXH63P_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
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

static esp_err_t jxh63p_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        JXH63P_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t jxh63p_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del jxh63p (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        jxh63p_power_off(dev);
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t jxh63p_ops = {
    .query_para_desc = jxh63p_query_para_desc,
    .get_para_value = jxh63p_get_para_value,
    .set_para_value = jxh63p_set_para_value,
    .query_support_formats = jxh63p_query_support_formats,
    .query_support_capability = jxh63p_query_support_capability,
    .set_format = jxh63p_set_format,
    .get_format = jxh63p_get_format,
    .priv_ioctl = jxh63p_priv_ioctl,
    .del = jxh63p_delete
};

esp_cam_sensor_device_t *jxh63p_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct jxh63p_cam *cam_jxh63p;
    bool power_on_success = false;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_jxh63p = heap_caps_calloc(1, sizeof(struct jxh63p_cam), MALLOC_CAP_DEFAULT);
    if (!cam_jxh63p) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    cam_jxh63p->jxh63p_para.limited_gain_index = ARRAY_SIZE(jxh63p_total_gain_val_map);

    dev->name = (char *)JXH63P_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &jxh63p_ops;
    dev->priv = cam_jxh63p;
    dev->cur_format = &jxh63p_format_info[get_jxh63p_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (jxh63p_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }
    power_on_success = true;

    if (jxh63p_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != JXH63P_PID) {
        ESP_LOGE(TAG, "Camera sensor is not JXH63P, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    if (power_on_success) {
        jxh63p_power_off(dev);
    }
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_JXH63P_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(jxh63p_detect, ESP_CAM_SENSOR_MIPI_CSI, JXH63P_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return jxh63p_detect(config);
}
#endif
