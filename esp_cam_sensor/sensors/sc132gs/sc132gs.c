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
#include "sc132gs_settings.h"
#include "sc132gs.h"

/*
 * SC132GS camera sensor gain control.
 */
typedef struct {
    uint8_t again_fine; // analog gain fine
    uint8_t again_coarse; // analog gain coarse
} sc132gs_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index
    size_t limited_abs_gain_index;

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} sc132gs_para_t;

struct sc132gs_cam {
    sc132gs_para_t sc132gs_para;
};

#define SC132GS_IO_MUX_LOCK(mux)
#define SC132GS_IO_MUX_UNLOCK(mux)
#define SC132GS_ENABLE_OUT_XCLK(pin,clk)
#define SC132GS_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_SC132GS(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_SC132GS_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#define SC132GS_FETCH_EXP_H(val)     (((val) >> 16) & 0xF)
#define SC132GS_FETCH_EXP_M(val)     (((val) >> 8) & 0xFF)
#define SC132GS_FETCH_EXP_L(val)     ((val) & 0xFF)

#define SC132GS_FETCH_DGAIN_COARSE(val)  (((val) >> 8) & 0x03)
#define SC132GS_FETCH_DGAIN_FINE(val)    ((val) & 0xFF)

#define SC132GS_GROUP_HOLD_START        0x00
#define SC132GS_GROUP_HOLD_END          0x10
#define SC132GS_GROUP_HOLD_TRIG         0x60
#define SC132GS_GROUP_HOLD_DELAY_FRAMES 0x40

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define SC132GS_SUPPORT_NUM CONFIG_CAMERA_SC132GS_MAX_SUPPORT

/*
 * Exposure register LSB = 1/16 line; isp_info.tline_ns is the time of one LSB.
 * esp_video converts with REG_TO_US = reg * tline_ns / 1000 (integer).
 * min=0x01 truncates to 0 us and fails esp_ipa_agc init
 * ("min_exposure = 0 or max_exposure = 0 ..."). Use 1 full line (0x10).
 */
static const uint8_t s_sc132gs_exp_min = 0x10;
static const uint8_t s_sc132gs_exp_max_offset = 0x08;
static const uint8_t s_sc132gs_exp_step = 0x01;
static const uint32_t s_limited_gain = CONFIG_CAMERA_SC132GS_ABSOLUTE_GAIN_LIMIT;
static const char *TAG = "sc132gs";
#define SC132GS_EXPOSURE_TEST_EN 0
#define SC132GS_EXPOSURE_TEST_EN_GAIN 0

// total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final abs_gain is multiplied by 1000.)
static const uint32_t sc132gs_total_gain_val_map[] = {
    1000,
    1031,
    1063,
    1094,
    1125,
    1156,
    1188,
    1219,
    1250,
    1281,
    1313,
    1344,
    1375,
    1406,
    1438,
    1469,
    1500,
    1531,
    1563,
    1594,
    1625,
    1656,
    1688,
    1719,
    1750,
    1781,
    1813,
    1869,
    1926,
    1982,

    // 2X
    2039,
    2096,
    2152,
    2209,
    2266,
    2322,
    2379,
    2436,
    2492,
    2549,
    2605,
    2662,
    2719,
    2775,
    2832,
    2889,
    2945,

    // 3X
    3002,
    3059,
    3115,
    3172,
    3229,
    3285,
    3342,
    3398,
    3455,
    3512,
    3568,
    3625,
    3738,
    3852,
    3965,

    // 4X
    4078,
    4191,
    4305,
    4418,
    4531,
    4645,
    4758,
    4871,
    4984,

    // 5X
    5098,
    5211,
    5324,
    5438,
    5551,
    5664,
    5777,
    5891,

    // 6X
    6004,
    6117,
    6230,
    6344,
    6457,
    6570,
    6684,
    6797,
    6910,

    // 7X
    7023,
    7137,
    7250,
    7477,
    7703,
    7930,

    // 8X
    8156,
    8383,
    8609,
    8836,

    // 9X
    9063,
    9289,
    9516,
    9742,
    9969,

    // 10X
    10195,
    10422,
    10648,
    10875,

    // 11X
    11102,
    11328,
    11555,
    11781,

    // 12X
    12008,
    12234,
    12461,
    12688,
    12914,

    // 13X
    13141,
    13367,
    13594,
    13820,

    // 14X
    14047,
    14273,
    14500,
    14953,

    // 15X
    15406,
    15859,

    // 16X
    16313,
    16766,

    // 17X
    17219,
    17672,

    // 18X
    18125,
    18578,

    // 19X
    19031,
    19484,
    19938,

    // 20X
    20391,
    20844,

    // 21X
    21297,
    21750,

    // 22X
    22203,
    22656,

    // 23X
    23109,
    23563,

    // 24X
    24016,
    24469,
    24922,

    // 25X
    25375,
    25828,

    // 26X
    26281,
    26734,

    // 27X
    27188,
    27641,

    // 28X
    28093,
    28547,
};

// SC132GS Gain map format: [analog gain fine, analog gain coarse]
static const sc132gs_gain_t sc132gs_gain_map[] = {
    // 1X
    {0x20, 0x03},
    {0x21, 0x03},
    {0x22, 0x03},
    {0x23, 0x03},
    {0x24, 0x03},
    {0x25, 0x03},
    {0x26, 0x03},
    {0x27, 0x03},
    {0x28, 0x03},
    {0x29, 0x03},
    {0x2A, 0x03},
    {0x2B, 0x03},
    {0x2C, 0x03},
    {0x2D, 0x03},
    {0x2E, 0x03},
    {0x2F, 0x03},
    {0x30, 0x03},
    {0x31, 0x03},
    {0x32, 0x03},
    {0x33, 0x03},
    {0x34, 0x03},
    {0x35, 0x03},
    {0x36, 0x03},
    {0x37, 0x03},
    {0x38, 0x03},
    {0x39, 0x03},//1781
    {0x20, 0x23},//1813
    {0x21, 0x23},
    {0x22, 0x23},
    {0x23, 0x23},

    // 2X
    {0x24, 0x23},
    {0x25, 0x23},
    {0x26, 0x23},
    {0x27, 0x23},
    {0x28, 0x23},
    {0x29, 0x23},
    {0x2A, 0x23},
    {0x2B, 0x23},
    {0x2C, 0x23},
    {0x2D, 0x23},
    {0x2E, 0x23},
    {0x2F, 0x23},
    {0x30, 0x23},
    {0x31, 0x23},
    {0x32, 0x23},
    {0x33, 0x23},
    {0x34, 0x23},

    // 3X
    {0x35, 0x23},
    {0x36, 0x23},
    {0x37, 0x23},
    {0x38, 0x23},
    {0x39, 0x23},
    {0x3A, 0x23},
    {0x3B, 0x23},
    {0x3C, 0x23},
    {0x3D, 0x23},
    {0x3E, 0x23},
    {0x3F, 0x23},
    {0x20, 0x27},
    {0x21, 0x27},
    {0x22, 0x27},
    {0x23, 0x27},

    // 4X
    {0x24, 0x27},
    {0x25, 0x27},
    {0x26, 0x27},
    {0x27, 0x27},
    {0x28, 0x27},
    {0x29, 0x27},
    {0x2A, 0x27},
    {0x2B, 0x27},
    {0x2C, 0x27},

    // 5X
    {0x2D, 0x27},
    {0x2E, 0x27},
    {0x2F, 0x27},
    {0x30, 0x27},
    {0x31, 0x27},
    {0x32, 0x27},
    {0x33, 0x27},
    {0x34, 0x27},

    // 6X
    {0x35, 0x27},
    {0x36, 0x27},
    {0x37, 0x27},
    {0x38, 0x27},
    {0x39, 0x27},
    {0x3A, 0x27},
    {0x3B, 0x27},
    {0x3C, 0x27},
    {0x3D, 0x27},

    // 7X
    {0x3E, 0x27},
    {0x3F, 0x27},
    {0x20, 0x2F},
    {0x21, 0x2F},
    {0x22, 0x2F},
    {0x23, 0x2F},

    // 8X
    {0x24, 0x2F},
    {0x25, 0x2F},
    {0x26, 0x2F},
    {0x27, 0x2F},

    // 9X
    {0x28, 0x2F},
    {0x29, 0x2F},
    {0x2A, 0x2F},
    {0x2B, 0x2F},
    {0x2C, 0x2F},

    // 10X
    {0x2D, 0x2F},
    {0x2E, 0x2F},
    {0x2F, 0x2F},
    {0x30, 0x2F},

    // 11X
    {0x31, 0x2F},
    {0x32, 0x2F},
    {0x33, 0x2F},
    {0x34, 0x2F},

    // 12X
    {0x35, 0x2F},
    {0x36, 0x2F},
    {0x37, 0x2F},
    {0x38, 0x2F},
    {0x39, 0x2F},

    // 13X
    {0x3A, 0x2F},
    {0x3B, 0x2F},
    {0x3C, 0x2F},
    {0x3D, 0x2F},

    // 14X
    {0x3E, 0x2F},
    {0x3F, 0x2F},
    {0x20, 0x3F},
    {0x21, 0x3F},

    // 15x
    {0x22, 0x3F},
    {0x23, 0x3F},

    // 16x
    {0x24, 0x3F},
    {0x25, 0x3F},

    // 17x
    {0x26, 0x3F},
    {0x27, 0x3F},

    // 18x
    {0x28, 0x3F},
    {0x29, 0x3F},

    // 19x
    {0x2A, 0x3F},
    {0x2B, 0x3F},
    {0x2C, 0x3F},

    // 20x
    {0x2D, 0x3F},
    {0x2E, 0x3F},

    // 21x
    {0x2F, 0x3F},
    {0x30, 0x3F},

    // 22x
    {0x31, 0x3F},
    {0x32, 0x3F},

    // 23x
    {0x33, 0x3F},
    {0x34, 0x3F},

    // 24x
    {0x35, 0x3F},
    {0x36, 0x3F},
    {0x37, 0x3F},

    // 25x
    {0x38, 0x3F},
    {0x39, 0x3F},

    // 26x
    {0x3A, 0x3F},
    {0x3B, 0x3F},

    // 27x
    {0x3C, 0x3F},
    {0x3D, 0x3F},

    // 28x
    {0x3E, 0x3F},
    {0x3F, 0x3F},
};

static const esp_cam_sensor_isp_info_t sc132gs_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 118800000,
            .vts = 1847,
            .hts = 1096,
            .tline_ns = 564,
            .gain_def = 16,
            .exp_def = 0x72f0,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 120000000,
            .vts = 1400,
            .hts = 750,
            .tline_ns = 744,
            .gain_def = 16,
            .exp_def = 0x5700,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
};

static const esp_cam_sensor_format_t sc132gs_format_info[] = {
#if CONFIG_CAMERA_SC132GS_MIPI_RAW10_544X640_60FPS
    {
        .name = "MIPI_2lane_24Minput_RAW10_544x640_60fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 544,
        .height = 640,
        .regs = sc132gs_mipi_2lane_24Minput_544x640_raw10_60fps,
        .regs_size = ARRAY_SIZE(sc132gs_mipi_2lane_24Minput_544x640_raw10_60fps),
        .fps = 60,
        .isp_info = &sc132gs_isp_info[0],
        .mipi_info = {
            .mipi_clk = 600000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
#if CONFIG_CAMERA_SC132GS_MIPI_RAW10_1080X1280_60FPS
    {
        .name = "MIPI_2lane_24Minput_RAW10_1080x1280_60fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1080,
        .height = 1280,
        .regs = sc132gs_mipi_2lane_24Minput_1080x1280_raw10_60fps,
        .regs_size = ARRAY_SIZE(sc132gs_mipi_2lane_24Minput_1080x1280_raw10_60fps),
        .fps = 60,
        .isp_info = &sc132gs_isp_info[1],
        .mipi_info = {
            .mipi_clk = 600000000,
            .lane_num = 2,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
#endif
};

static const int sc132gs_format_index[] = {
#if CONFIG_CAMERA_SC132GS_MIPI_RAW10_544X640_60FPS
    0,
#endif
#if CONFIG_CAMERA_SC132GS_MIPI_RAW10_1080X1280_60FPS
    1,
#endif
};

static int get_sc132gs_actual_format_index(void)
{
    int default_index = CONFIG_CAMERA_SC132GS_MIPI_IF_FORMAT_INDEX_DEFAULT;
    for (size_t i = 0; i < ARRAY_SIZE(sc132gs_format_index); i++) {
        if (sc132gs_format_index[i] == default_index) {
            return i;
        }
    }
    return 0;
}

static esp_err_t sc132gs_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t sc132gs_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t sc132gs_write_array(esp_sccb_io_handle_t sccb_handle, sc132gs_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != SC132GS_REG_END) {
        if (regarray[i].reg != SC132GS_REG_DELAY) {
            ret = sc132gs_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "sc132gs_write_array ret=%d, i=%d", ret, i);
    return ret;
}

static esp_err_t sc132gs_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sc132gs_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = sc132gs_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t sc132gs_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return sc132gs_set_reg_bits(dev->sccb_handle, 0x4501, 3, 1, enable ? 0x01 : 0x00);
}

static esp_err_t sc132gs_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sc132gs_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = sc132gs_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sc132gs_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = sc132gs_read(dev->sccb_handle, SC132GS_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sc132gs_read(dev->sccb_handle, SC132GS_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sc132gs_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = sc132gs_write(dev->sccb_handle, SC132GS_REG_SLEEP_MODE, enable ? 0x01 : 0x00);
    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t sc132gs_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return sc132gs_set_reg_bits(dev->sccb_handle, 0x3221, 1, 2,  enable ? 0x03 : 0x00);
}

static esp_err_t sc132gs_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return sc132gs_set_reg_bits(dev->sccb_handle, 0x3221, 5, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc132gs_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct sc132gs_cam *cam_sc132gs = (struct sc132gs_cam *)dev->priv;
    uint32_t value_buf = MAX(u32_val, s_sc132gs_exp_min);
    value_buf = MIN(value_buf, cam_sc132gs->sc132gs_para.exposure_max);

    ESP_LOGD(TAG, "set exposure 0x%" PRIx32, value_buf);
    /* 4 least significant bits of expsoure are fractional part */
    ret = sc132gs_write(dev->sccb_handle,
                        SC132GS_REG_SHUTTER_TIME_H,
                        SC132GS_FETCH_EXP_H(value_buf));
    ret |= sc132gs_write(dev->sccb_handle,
                         SC132GS_REG_SHUTTER_TIME_M,
                         SC132GS_FETCH_EXP_M(value_buf));
    ret |= sc132gs_write(dev->sccb_handle,
                         SC132GS_REG_SHUTTER_TIME_L,
                         SC132GS_FETCH_EXP_L(value_buf));
    if (ret == ESP_OK) {
        cam_sc132gs->sc132gs_para.exposure_val = value_buf;
    }
    return ret;
}

static esp_err_t sc132gs_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret = ESP_OK;
    struct sc132gs_cam *cam_sc132gs = (struct sc132gs_cam *)dev->priv;

    if (u32_val > cam_sc132gs->sc132gs_para.limited_abs_gain_index) {
        u32_val = cam_sc132gs->sc132gs_para.limited_abs_gain_index;
    }

    ESP_LOGD(TAG, "again_fine %" PRIx8 ", again_coarse %" PRIx8, sc132gs_gain_map[u32_val].again_fine, sc132gs_gain_map[u32_val].again_coarse);

    ret = sc132gs_write(dev->sccb_handle, SC132GS_REG_FINE_AGAIN, sc132gs_gain_map[u32_val].again_fine);
    ret |= sc132gs_write(dev->sccb_handle, SC132GS_REG_COARSE_AGAIN, sc132gs_gain_map[u32_val].again_coarse);
    if (ret == ESP_OK) {
        cam_sc132gs->sc132gs_para.gain_index = u32_val;
    }
    return ret;
}

static esp_err_t sc132gs_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    struct sc132gs_cam *cam_sc132gs = (struct sc132gs_cam *)dev->priv;

    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = s_sc132gs_exp_min;
        qdesc->number.maximum = (dev->cur_format->isp_info->isp_v1_info.vts - s_sc132gs_exp_max_offset) * 16;
        /* Step must map to >=1 us after REG_TO_US, otherwise IPA gets step_exposure=0 */
        qdesc->number.step = s_sc132gs_exp_step;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        /* IPA rejects min_exposure == 0; clamp after unit conversion */
        qdesc->number.minimum = MAX(1, EXPOSURE_SC132GS_TO_V4L2(s_sc132gs_exp_min, dev->cur_format));
        qdesc->number.maximum = EXPOSURE_SC132GS_TO_V4L2(((dev->cur_format->isp_info->isp_v1_info.vts - s_sc132gs_exp_max_offset) * 16), dev->cur_format);
        qdesc->number.step = MAX(EXPOSURE_SC132GS_TO_V4L2(s_sc132gs_exp_step, dev->cur_format), 1);
        qdesc->default_value = EXPOSURE_SC132GS_TO_V4L2((dev->cur_format->isp_info->isp_v1_info.exp_def), dev->cur_format);
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = cam_sc132gs->sc132gs_para.limited_abs_gain_index + 1;
        qdesc->enumeration.elements = sc132gs_total_gain_val_map;
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

static esp_err_t sc132gs_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct sc132gs_cam *cam_sc132gs = (struct sc132gs_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_sc132gs->sc132gs_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_sc132gs->sc132gs_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t sc132gs_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = sc132gs_set_exp_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_SC132GS(u32_val, dev->cur_format);
        ret = sc132gs_set_exp_val(dev, ori_exp);
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ret = sc132gs_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        /* IPA sets exposure_val and leaves exposure_us=0. Using only exposure_us
        * converts 0 → min shutter and forces low-exp/high-gain forever. */
        if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_SC132GS(value->exposure_us, dev->cur_format);
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }

        ret = sc132gs_set_exp_val(dev, ori_exp);
        ret |= sc132gs_set_total_gain_val(dev, value->gain_index);

        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = sc132gs_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = sc132gs_set_mirror(dev, *value);
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

static esp_err_t sc132gs_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(sc132gs_format_info);
    formats->format_array = &sc132gs_format_info[0];
    return ESP_OK;
}

static esp_err_t sc132gs_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

#if SC132GS_EXPOSURE_TEST_EN
static volatile uint32_t s_exp_v = 0x05;
static bool s_exp_add = true;
TimerHandle_t ae_timer_handle;
static void ae_timer_callback(TimerHandle_t timer)
{
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    struct sc132gs_cam *cam_sc132gs = (struct sc132gs_cam *)dev->priv;
#if SC132GS_EXPOSURE_TEST_EN_GAIN
    if (s_exp_v >= cam_sc132gs->sc132gs_para.limited_abs_gain_index) {
        s_exp_add = false;
    } else if (s_exp_v < 1) {
        s_exp_add = true;
    }
    sc132gs_set_total_gain_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += 1;
    } else {
        s_exp_v -= 1;
    }
#else
    static const uint32_t s_exp_step = 0x0f;
    if (s_exp_v >= cam_sc132gs->sc132gs_para.exposure_max) {
        s_exp_add = false;
    } else if (s_exp_v <= s_sc132gs_exp_min) {
        s_exp_add = true;
    }
    sc132gs_set_exp_val(dev, s_exp_v);
    if (s_exp_add == true) {
        s_exp_v += s_exp_step;
    } else {
        if (s_exp_v >= s_exp_step) {
            s_exp_v -= s_exp_step;
        }
    }
#endif
    ESP_LOGI(TAG, "E=%" PRIu32, s_exp_v);
}
#endif

static esp_err_t sc132gs_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct sc132gs_cam *cam_sc132gs = (struct sc132gs_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &sc132gs_format_info[get_sc132gs_actual_format_index()];
    }

    ret = sc132gs_write_array(dev->sccb_handle, (sc132gs_reginfo_t *)format->regs);

    if (ret != ESP_OK) {
        ESP_LOGE(__func__, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_sc132gs->sc132gs_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_sc132gs->sc132gs_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_sc132gs->sc132gs_para.exposure_max = (dev->cur_format->isp_info->isp_v1_info.vts - s_sc132gs_exp_max_offset) * 16;
#if SC132GS_EXPOSURE_TEST_EN
    ae_timer_handle = xTimerCreate("AE_t", 200 / portTICK_PERIOD_MS, pdTRUE,
                                   (void *)dev, ae_timer_callback);
    xTimerStart(ae_timer_handle, portMAX_DELAY);
#endif
    return ret;
}

static esp_err_t sc132gs_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t sc132gs_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SC132GS_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sc132gs_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sc132gs_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc132gs_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sc132gs_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sc132gs_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc132gs_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sc132gs_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    SC132GS_IO_MUX_UNLOCK(mux);

    return ret;
}

static esp_err_t sc132gs_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC132GS_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "gpio config failed");

        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t sc132gs_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC132GS_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t sc132gs_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sc132gs (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t sc132gs_ops = {
    .query_para_desc = sc132gs_query_para_desc,
    .get_para_value = sc132gs_get_para_value,
    .set_para_value = sc132gs_set_para_value,
    .query_support_formats = sc132gs_query_support_formats,
    .query_support_capability = sc132gs_query_support_capability,
    .set_format = sc132gs_set_format,
    .get_format = sc132gs_get_format,
    .priv_ioctl = sc132gs_priv_ioctl,
    .del = sc132gs_delete
};

esp_cam_sensor_device_t *sc132gs_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct sc132gs_cam *cam_sc132gs;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_sc132gs = heap_caps_calloc(1, sizeof(struct sc132gs_cam), MALLOC_CAP_DEFAULT);
    if (!cam_sc132gs) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)SC132GS_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sc132gs_ops;
    dev->priv = cam_sc132gs;
    cam_sc132gs->sc132gs_para.limited_abs_gain_index = ARRAY_SIZE(sc132gs_total_gain_val_map) - 1;
    for (size_t i = 0; i < ARRAY_SIZE(sc132gs_total_gain_val_map); i++) {
        if (sc132gs_total_gain_val_map[i] > s_limited_gain) {
            cam_sc132gs->sc132gs_para.limited_abs_gain_index = (i > 0) ? (i - 1) : 0;
            break;
        }
    }
    dev->cur_format = &sc132gs_format_info[get_sc132gs_actual_format_index()];

    // Configure sensor power, clock, and SCCB port
    if (sc132gs_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc132gs_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SC132GS_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SC132GS, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGD(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sc132gs_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SC132GS_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc132gs_detect, ESP_CAM_SENSOR_MIPI_CSI, SC132GS_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc132gs_detect(config);
}
#endif
