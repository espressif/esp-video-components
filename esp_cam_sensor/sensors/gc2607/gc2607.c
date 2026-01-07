/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/timers.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "gc2607_settings.h"
#include "gc2607.h"

/*
 * GC2607 camera sensor gain control.
 */
typedef struct {
    uint8_t reg_02b3;
    uint8_t reg_02b4;
    uint8_t reg_020c;
    uint8_t reg_020d;
    uint8_t reg_0e2a;
    uint8_t reg_0e2b;
} gc2607_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_abs_gain_index;
    bool stream_en;

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} gc2607_para_t;

struct gc2607_cam {
    gc2607_para_t gc2607_para;
};

#define GC2607_IO_MUX_LOCK(mux)
#define GC2607_IO_MUX_UNLOCK(mux)
#define GC2607_ENABLE_OUT_XCLK(pin,clk)
#define GC2607_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_GC2607(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_GC2607_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#define GC2607_FETCH_EXP_H(val)     (((val) >> 8) & 0x3F)
#define GC2607_FETCH_EXP_L(val)     ((val) & 0xFF)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))

static const uint32_t s_limited_gain = CONFIG_CAMERA_GC2607_ABSOLUTE_GAIN_LIMIT;
static const uint8_t s_gc2607_exp_max_offset = 0x10; // min:1, max:VTS-16
static const uint8_t s_gc2607_exp_min = 0x03;
static const char *TAG = "gc2607";
#define GC2607_EXPOSURE_TEST_EN 0
#define GC2607_EXPOSURE_TEST_EN_GAIN 0

// total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final abs_gain is multiplied by 1000.)
static const uint32_t gc2607_total_gain_val_map[] = {
    1000,
    1188,
    1453,
    1750,
    2047,
    2453,
    2891,
    3469,
    3953,
    4750,
    5734,
    6781,
    7984,
    9500,
    11219,
    13250,
    15813,
};

// GC2607 Gain map
static const gc2607_gain_t gc2607_gain_map[] = {
    //0x02b3,0x02b4,0x020c,0x020d,0x0e2a,0x0e2b
    { 0x00,  0x00,  0x00, 0x40,  0x08,  0x08},
    { 0x05,  0x00,  0x00, 0x4B,  0x08,  0x08},
    { 0x00,  0x01,  0x00, 0x59,  0x08,  0x08},
    { 0x05,  0x01,  0x00, 0x6A,  0x08,  0x08},
    { 0x00,  0x02,  0x00, 0x80,  0x08,  0x08},
    { 0x05,  0x02,  0x00, 0x97,  0x08,  0x08},
    { 0x00,  0x03,  0x00, 0xB3,  0x08,  0x08},
    { 0x05,  0x03,  0x00, 0xD4,  0x08,  0x08},
    { 0x00,  0x04,  0x01, 0x00,  0x08,  0x08},
    { 0x05,  0x04,  0x01, 0x2F,  0x08,  0x08},
    { 0x00,  0x05,  0x01, 0x66,  0x08,  0x08},
    { 0x05,  0x05,  0x01, 0xA8,  0x08,  0x08},
    { 0x00,  0x06,  0x02, 0x00,  0x0c,  0x0c},
    { 0x05,  0x06,  0x02, 0x5E,  0x0c,  0x0c},
    { 0x09,  0x26,  0x02, 0xCC,  0x0c,  0x0c},
    { 0x0C,  0xB6,  0x03, 0x50,  0x0c,  0x0c},
    { 0x10,  0x06,  0x04, 0x00,  0x10,  0x10},
};

#if CONFIG_SOC_MIPI_CSI_SUPPORTED
static const esp_cam_sensor_isp_info_t gc2607_isp_info_mipi[] = {
#if CONFIG_CAMERA_GC2607_MIPI_RAW10_1920X1080_25FPS
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 84000000,
            .vts = 1640,
            .hts = 2048,
            .tline_ns = 24381,
            .gain_def = 0,
            .exp_def = 0x0438,
            .bayer_type = ESP_CAM_SENSOR_BAYER_GRBG,
        }
    },
#endif
};

#ifndef CONFIG_CAMERA_GC2607_MIPI_IF_FORMAT_INDEX_DEFAULT
#error "Please choose at least one format in menuconfig for GC2607"
#endif

static const uint8_t gc2607_format_default_index = CONFIG_CAMERA_GC2607_MIPI_IF_FORMAT_INDEX_DEFAULT;

static const uint8_t gc2607_format_index[] = {
#if CONFIG_CAMERA_GC2607_MIPI_RAW10_1920X1080_25FPS
    0,
#endif
};

static const esp_cam_sensor_format_t gc2607_format_info_mipi[] = {
    /* For MIPI */
#if CONFIG_CAMERA_GC2607_MIPI_RAW10_1920X1080_25FPS
    {
        .name = "MIPI_2lane_24Minput_raw10_1920x1080_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = gc2607_mipi_2lane_24Minput_raw10_1920x1080_25fps,
        .regs_size = ARRAY_SIZE(gc2607_mipi_2lane_24Minput_raw10_1920x1080_25fps),
        .fps = 25,
        .isp_info = &gc2607_isp_info_mipi[0],
        .mipi_info = {
            .mipi_clk = 672000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
    },
#endif
};

static uint8_t get_gc2607_actual_format_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(gc2607_format_index); i++) {
        if (gc2607_format_index[i] == gc2607_format_default_index) {
            return i;
        }
    }

    return 0;
}
#endif

static esp_err_t gc2607_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t gc2607_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

static esp_err_t gc2607_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = gc2607_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = gc2607_write(sccb_handle, reg, value);
    return ret;
}

/* write a array of registers  */
static esp_err_t gc2607_write_array(esp_sccb_io_handle_t sccb_handle, const gc2607_reginfo_t *regs, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && i < regs_size) {
        if (regs[i].reg == GC2607_REG_DELAY) {
            delay_ms(regs[i].val);
        } else {
            ret = gc2607_write(sccb_handle, regs[i].reg, regs[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "write regs cnt=%d", i);
    return ret;
}

static esp_err_t gc2607_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return gc2607_set_reg_bits(dev->sccb_handle, 0x008c, 2, 1, enable ? 0x01 : 0x00);
}

// Note that gc2607 xshutdown must be used to control reset(Especially in cases where only the SOC loses power.)
static esp_err_t gc2607_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t gc2607_soft_reset(esp_cam_sensor_device_t *dev)
{
    // Todo, check
    esp_err_t ret = gc2607_write(dev->sccb_handle, 0x03fe, 0xf0);
    delay_ms(5);
    return ret;
}

static esp_err_t gc2607_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = gc2607_read(dev->sccb_handle, GC2607_REG_ID_HIGH, &pid_h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ID read h failed");
        return ret;
    }
    ret = gc2607_read(dev->sccb_handle, GC2607_REG_ID_LOW, &pid_l);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ID read l failed");
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t gc2607_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct gc2607_cam *cam_gc2607 = (struct gc2607_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_gc2607_exp_min);
    value_buf = MIN(value_buf, cam_gc2607->gc2607_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);

    ret = gc2607_write(dev->sccb_handle, GC2607_REG_SHUTTER_TIME_H, GC2607_FETCH_EXP_H(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "shutter time high write failed");
    ret = gc2607_write(dev->sccb_handle, GC2607_REG_SHUTTER_TIME_L, GC2607_FETCH_EXP_L(value_buf));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "shutter time low write failed");

    cam_gc2607->gc2607_para.exposure_val = value_buf;
    return ret;
}

static esp_err_t gc2607_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct gc2607_cam *cam_gc2607 = (struct gc2607_cam *)dev->priv;
    u32_val = MIN(u32_val, cam_gc2607->gc2607_para.limited_abs_gain_index);

    ESP_LOGD(TAG, "gain index = 0x%" PRIx32, u32_val);

    ret = gc2607_write(dev->sccb_handle, 0x02b3, gc2607_gain_map[u32_val].reg_02b3);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg 0x02b3 write failed");
    ret = gc2607_write(dev->sccb_handle, 0x02b4, gc2607_gain_map[u32_val].reg_02b4);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg 0x02b4 write failed");
    ret = gc2607_write(dev->sccb_handle, 0x020c, gc2607_gain_map[u32_val].reg_020c);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg 0x020c write failed");
    ret = gc2607_write(dev->sccb_handle, 0x020d, gc2607_gain_map[u32_val].reg_020d);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg 0x020d write failed");
    ret = gc2607_write(dev->sccb_handle, 0x0e2a, gc2607_gain_map[u32_val].reg_0e2a);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg 0x0e2a write failed");
    ret = gc2607_write(dev->sccb_handle, 0x0e2b, gc2607_gain_map[u32_val].reg_0e2b);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gain reg 0x0e2b write failed");

    cam_gc2607->gc2607_para.gain_index = u32_val;
    ESP_LOGD(TAG, "Gain update done");
    return ret;
}

static esp_err_t gc2607_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    struct gc2607_cam *cam_gc2607 = (struct gc2607_cam *)dev->priv;
    bool enable_bool = enable ? true : false;
    if (cam_gc2607->gc2607_para.stream_en == enable_bool) {
        ESP_LOGW(TAG, "stream has been set to %d", enable);
        return ESP_OK;
    }
    if (enable_bool) {
        ret = gc2607_write_array(dev->sccb_handle, gc2607_stream_on, ARRAY_SIZE(gc2607_stream_on));
    } else {
        ret = gc2607_write_array(dev->sccb_handle, gc2607_stream_off, ARRAY_SIZE(gc2607_stream_off));
    }
    if (ret == ESP_OK) {
        dev->stream_status = enable_bool;
        cam_gc2607->gc2607_para.stream_en = enable_bool;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t gc2607_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return gc2607_set_reg_bits(dev->sccb_handle, 0x0101, 0, 1, enable ? 0x01 : 0x00);
}

static esp_err_t gc2607_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return gc2607_set_reg_bits(dev->sccb_handle, 0x0101, 1, 1, enable ? 0x01 : 0x00);
}

static esp_err_t gc2607_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct gc2607_cam *cam_gc2607 = (struct gc2607_cam *)dev->priv;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_gc2607_exp_min;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - s_gc2607_exp_max_offset;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = MAX(0x01, EXPOSURE_GC2607_TO_V4L2(s_gc2607_exp_min, dev->cur_format)); // The minimum value must be greater than 1
        qdesc->number.maximum = EXPOSURE_GC2607_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.vts - s_gc2607_exp_max_offset), dev->cur_format);
        qdesc->number.step = MAX(0x01, EXPOSURE_GC2607_TO_V4L2(0x01, dev->cur_format));
        qdesc->default_value = EXPOSURE_GC2607_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_gc2607->gc2607_para.limited_abs_gain_index + 1;
        qdesc->enumeration.elements = gc2607_total_gain_val_map;
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

static esp_err_t gc2607_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct gc2607_cam *cam_gc2607 = (struct gc2607_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_gc2607->gc2607_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_gc2607->gc2607_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t gc2607_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = gc2607_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_GC2607(u32_val, dev->cur_format);
        ret = gc2607_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = gc2607_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_GC2607(value->exposure_us, dev->cur_format);
        } else if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = gc2607_set_exp_val(dev, ori_exp);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "group exp/gain: exposure write failed");
        ret = gc2607_set_total_gain_val(dev, value->gain_index);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = gc2607_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = gc2607_set_mirror(dev, *value);
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

static esp_err_t gc2607_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    esp_err_t ret = ESP_FAIL;
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        formats->count = ARRAY_SIZE(gc2607_format_info_mipi);
        formats->format_array = &gc2607_format_info_mipi[0];
        ret = ESP_OK;
    }
#endif

    return ret;
}

static esp_err_t gc2607_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

#if GC2607_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = 0x09;
static bool s_exp_add = true;
TimerHandle_t ae_timer_handle;
static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    struct gc2607_cam *cam_gc2607 = (struct gc2607_cam *)dev->priv;
#if GC2607_EXPOSURE_TEST_EN_GAIN
    if (s_exp_v >= cam_gc2607->gc2607_para.limited_abs_gain_index) {
        s_exp_add = false;
    } else if (s_exp_v < 1) {
        s_exp_add = true;
    }
    gc2607_set_total_gain_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 1;
    } else {
        s_exp_v -= 1;
    }
#else
    if (s_exp_v >= cam_gc2607->gc2607_para.exposure_max) {
        s_exp_add = false;
    } else if (s_exp_v < s_gc2607_exp_min) {
        s_exp_add = true;
    }
    gc2607_set_exp_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 8;
    } else {
        s_exp_v -= 8;
    }
#endif
    ESP_LOGI(TAG, "E=%" PRIu32, s_exp_v);
}
#endif

static esp_err_t gc2607_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct gc2607_cam *cam_gc2607 = (struct gc2607_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
        if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
            format = &gc2607_format_info_mipi[get_gc2607_actual_format_index()];
        }
#endif
    }
    if (format == NULL) {
        ESP_LOGE(TAG, "No format available");
        return ESP_ERR_NOT_SUPPORTED;
    }

    ret = gc2607_write_array(dev->sccb_handle, (gc2607_reginfo_t *)format->regs, format->regs_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }
    ESP_LOGD(TAG, "Set format %s", format->name);

    dev->cur_format = format;

    // init para
    cam_gc2607->gc2607_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_gc2607->gc2607_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_gc2607->gc2607_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - s_gc2607_exp_max_offset;
#if GC2607_EXPOSURE_TEST_EN
    ae_timer_handle = xTimerCreate("AE_t", 300 / portTICK_PERIOD_MS, pdTRUE,
                                   (void *)dev, ae_timer_callback);
    xTimerStart(ae_timer_handle, portMAX_DELAY);
#endif
    return ret;
}

static esp_err_t gc2607_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t gc2607_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    GC2607_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = gc2607_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = gc2607_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc2607_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = gc2607_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = gc2607_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc2607_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = gc2607_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    GC2607_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t gc2607_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC2607_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(5);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(5);
    }

    return ret;
}

static esp_err_t gc2607_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC2607_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t gc2607_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del gc2607 (%p)", dev);
    if (dev) {
        gc2607_power_off(dev);
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t gc2607_ops = {
    .query_para_desc = gc2607_query_para_desc,
    .get_para_value = gc2607_get_para_value,
    .set_para_value = gc2607_set_para_value,
    .query_support_formats = gc2607_query_support_formats,
    .query_support_capability = gc2607_query_support_capability,
    .set_format = gc2607_set_format,
    .get_format = gc2607_get_format,
    .priv_ioctl = gc2607_priv_ioctl,
    .del = gc2607_delete
};

esp_cam_sensor_device_t *gc2607_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct gc2607_cam *cam_gc2607;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_gc2607 = heap_caps_calloc(1, sizeof(struct gc2607_cam), MALLOC_CAP_DEFAULT);
    if (!cam_gc2607) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    cam_gc2607->gc2607_para.limited_abs_gain_index = ARRAY_SIZE(gc2607_total_gain_val_map) - 1;
    for (size_t i = 0; i < ARRAY_SIZE(gc2607_total_gain_val_map); i++) {
        if (gc2607_total_gain_val_map[i] > s_limited_gain) {
            cam_gc2607->gc2607_para.limited_abs_gain_index = (i > 0) ? (i - 1) : 0;
            break;
        }
    }

    dev->name = (char *)GC2607_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &gc2607_ops;
    dev->priv = cam_gc2607;
#if CONFIG_SOC_MIPI_CSI_SUPPORTED
    if (config->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        dev->cur_format = &gc2607_format_info_mipi[get_gc2607_actual_format_index()];
    }
#endif

    // Configure sensor power, clock, and SCCB port
    if (gc2607_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (gc2607_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != GC2607_PID) {
        ESP_LOGE(TAG, "Camera sensor is not GC2607, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    gc2607_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_GC2607_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(gc2607_detect, ESP_CAM_SENSOR_MIPI_CSI, GC2607_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return gc2607_detect(config);
}
#endif
