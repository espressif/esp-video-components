/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "mira220_settings.h"
#include "mira220.h"

/*
 * mira220 camera sensor gain control.
 * Note1: The analog gain only has coarse gain, and no fine gain, so in the adjustment of analog gain.
 * Digital gain needs to replace analog fine gain for smooth transition, so as to avoid AGC oscillation.
 * Note2: the analog gain of mira220 will be affected by temperature, it is recommended to increase Dgain first and then Again.
 */


 typedef struct {
    uint8_t dgain_fine; // digital gain fine
    uint8_t dgain_coarse; // digital gain coarse
    uint8_t analog_gain;
} mira220_gain_t;


typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} mira220_para_t;

struct mira220_cam {
    mira220_para_t mira220_para;
};

#define mira220_IO_MUX_LOCK(mux)
#define mira220_IO_MUX_UNLOCK(mux)
#define mira220_ENABLE_OUT_XCLK(pin,clk)
#define mira220_DISABLE_OUT_XCLK(pin)

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_MIRA220(v, sf)          \
    ((uint32_t)(((double)v) * EXPOSURE_V4L2_UNIT_US * 1000 / (((sf)->isp_info->isp_v1_info.tline_ns)) + 0.5))
#define EXPOSURE_MIRA220_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * (((sf)->isp_info->isp_v1_info.tline_ns)) / EXPOSURE_V4L2_UNIT_US / 1000 + 0.5))

#define mira220_VTS_MAX          0x7fff // Max exposure is VTS-6
#define mira220_EXP_MAX_OFFSET   0x06



#define mira220_GROUP_HOLD_START        0x00
#define mira220_GROUP_HOLD_END          0x30
#define mira220_GROUP_HOLD_DELAY_FRAMES 0x01

#define mira220_PID         0xcb3a
#define mira220_SENSOR_NAME "mira220"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define mira220_SUPPORT_NUM CONFIG_CAMERA_MIRA220_MAX_SUPPORT

void mira220_get_settings(esp_cam_sensor_device_t *dev);
static esp_err_t mira220_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val);
void mira220_set_exp_us(esp_cam_sensor_device_t *dev, uint32_t u32_val);
void mira220_set_fps(esp_cam_sensor_device_t *dev, uint32_t u32_val);
// static size_t s_limited_gain_index;
// static const uint32_t s_limited_gain = 1;


static const char *TAG = "mira220";

typedef struct {
        esp_cam_sensor_output_format_t color_fiter;
        esp_cam_sensor_isp_info_v1_t bayer_type;
        int lane_number;
        int mipi_speed;
        int bit_depth;
        
        uint32_t row_lenght;
        uint32_t HSIZE_reg_val;
        uint32_t VSIZE_reg_val;
        uint32_t VBLANK_reg_val;
        uint32_t VBLANK_reg_val_max;
        uint32_t VBLANK_reg_val_min;
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

sensor_settings_t MIRA220;


static const esp_cam_sensor_isp_info_t mira220_isp_info[] = {
    /* For MIPI */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .vts = 11831,  // 600 + 3500
            .hts = 1024,
            .pclk = 65536000,
            .tline_ns = 15500,
            .gain_def = 0,
            .exp_def = 0x1,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    }
};

static const esp_cam_sensor_format_t mira220_format_info[] = {
    /* For MIPI */
    {
        .name = "MIPI_2lane_RAW8_1024_600_6fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 38400000,
        .width = 1024,
        .height = 600,
        .regs = init_reglist_MIPI_2lane_1024_600_6fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_2lane_1024_600_6fps),
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


static const uint32_t mira220_total_gain_val_map[] = {1000};

static esp_err_t mira220_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t mira220_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

static esp_err_t mira220_write_array(esp_sccb_io_handle_t sccb_handle, mira220_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    delay_ms(100);
    while ( (true))
    {   
        if (regarray[i].reg == 0xFFFF && regarray[i].val == 0xFF) {break;}
        ret = mira220_write(sccb_handle, regarray[i].reg, regarray[i].val);
        ESP_LOGI(TAG, "MIRA220 WRITE  0x%x = 0x%x", regarray[i].reg, regarray[i].val);
        i++;
    }

    return ret;
}

static esp_err_t mira220_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;
    return ESP_OK;

    ret = mira220_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = mira220_write(sccb_handle, reg, value);
}

static esp_err_t mira220_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return mira220_write(dev->sccb_handle, 0x2091, enable ? 0x01 : 0x00);
    return ESP_OK;
}

static esp_err_t mira220_hw_reset(esp_cam_sensor_device_t *dev)
{
    // if (dev->reset_pin >= 0) {
    //     gpio_set_level(dev->reset_pin, 0);
    //     delay_ms(10);
    //     gpio_set_level(dev->reset_pin, 1);
    //     delay_ms(10);
    // }
    return ESP_OK;
}

static esp_err_t mira220_soft_reset(esp_cam_sensor_device_t *dev)
{
    return ESP_OK;
}

static esp_err_t mira220_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{

    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = mira220_read(dev->sccb_handle, mira220_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
    }
    ret = mira220_read(dev->sccb_handle, mira220_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
    }
    id->pid = (pid_h << 8) | pid_l;
    ESP_LOGI(TAG, "MIRA220 READ 0x%x = 0x%x", mira220_REG_SENSOR_ID_H, pid_h);
    ESP_LOGI(TAG, "MIRA220 READ 0x%x = 0x%x", mira220_REG_SENSOR_ID_L, pid_l);

    return ESP_OK;
}

static esp_err_t mira220_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    mira220_get_settings(dev);
    // mira220_set_exp_us(dev, 20000);
    mira220_set_fps(dev, 30);

    ret = mira220_write(dev->sccb_handle, mira220_REG_MODE , enable ? 0x10 : 0x02);
    delay_ms(10);
    ret = mira220_write(dev->sccb_handle, mira220_REG_START, enable ? 0x01 : 0x00);
    delay_ms(10);
    dev->stream_status = enable;
    ESP_LOGI(TAG, "SET Stream=%d", enable);

    return ESP_OK;
}

static esp_err_t mira220_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return mira220_write(dev->sccb_handle, 0x209C, enable ? 0x01 : 0x00);
    return ESP_OK;
}

static esp_err_t mira220_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return mira220_write(dev->sccb_handle, 0x1095, enable ? 0x01 : 0x00);
    return ESP_OK;
}

static esp_err_t mira220_set_exp_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret;
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    uint32_t buf = u32_val; 
    uint16_t value_bufL = buf & 0x00FF;
    uint16_t value_bufH = (buf & 0xFF00)>>8;
    ESP_LOGI(TAG, "SET exposure 0x%x , 0x%x", value_bufL,  value_bufH);
    ret = mira220_write(dev->sccb_handle, mira220_REG_EXP_L,   value_bufL);
    ret = mira220_write(dev->sccb_handle, mira220_REG_EXP_H,   value_bufH);

    if (ret == ESP_OK)
    {
    cam_mira220->mira220_para.exposure_val = u32_val;
    MIRA220.exp_reg_val = u32_val;
    }

    return ESP_OK;
}

static esp_err_t mira220_set_total_gain_val(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    esp_err_t ret = ESP_OK;
    // struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    // u32_val = MIN(u32_val, s_limited_gain_index);

    // if (ret == ESP_OK) {
    //     cam_mira220->mira220_para.gain_index = u32_val;
    // }
    return ret;
}

static esp_err_t mira220_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    // ESP_LOGI(TAG, "id=%"PRIx32" is ", qdesc->id);

    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        // ESP_LOGI(TAG, "query ESP_CAM_SENSOR_EXPOSURE_VAL", qdesc->id);

        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0x1;
        qdesc->number.maximum = 0xFFFF; 
        qdesc->number.step = 0x1;
        qdesc->default_value = 0X1;
        break;

    case ESP_CAM_SENSOR_EXPOSURE_US:
         ESP_LOGI(TAG, "ESP_CAM_SENSOR_EXPOSURE_US", qdesc->id);

        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = MIRA220.row_length_us;
        qdesc->number.maximum = MIRA220.time_exp_max_us;
        qdesc->number.step = 10;
        qdesc->default_value = MIRA220.row_length_us;
        break;
    case ESP_CAM_SENSOR_GAIN:
        ESP_LOGI(TAG, "ESP_CAM_SENSOR_GAIN", qdesc->id);
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = 1;
        qdesc->enumeration.elements = mira220_total_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def; // gain index
        break;
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN:
        ESP_LOGI(TAG, "ESP_CAM_SENSOR_GROUP_EXP_GAIN", qdesc->id);
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_U8;
        qdesc->u8.size = sizeof(esp_cam_sensor_gh_exp_gain_t);
        break;
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        ESP_LOGI(TAG, "FLIP", qdesc->id);
        break;
    default: 
        ESP_LOGI(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;    
    }
    return ret;
}

static esp_err_t mira220_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    ESP_LOGI(TAG, "mira220_get_para_value");

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_mira220->mira220_para.exposure_val;
        break;
    } 
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_mira220->mira220_para.gain_index;
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

    switch (id) 
    {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        ESP_LOGI(TAG, "mira220_set_para_value ESP_CAM_SENSOR_EXPOSURE_VAL");
        uint32_t u32_val = *(uint32_t *)arg;
        mira220_set_exp_val(dev, u32_val);
        ESP_LOGI(TAG, "MIRA220 set_para ESP_CAM_SENSOR_EXPOSURE_VAL  %d "  , u32_val);

        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        ESP_LOGI(TAG, "mira220_set_para_value ESP_CAM_SENSOR_EXPOSURE_US");
        uint32_t u32_val = *(uint32_t *)arg;
        uint32_t ori_exp = EXPOSURE_V4L2_TO_MIRA220(u32_val, dev->cur_format);
        mira220_set_exp_us(dev, ori_exp);
        ESP_LOGI(TAG, "MIRA220 set_para ESP_CAM_SENSOR_EXPOSURE_US           %d "    , ori_exp);

        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        ESP_LOGI(TAG, "mira220_set_para_value ESP_CAM_SENSOR_GAIN");
        uint32_t u32_val = *(uint32_t *)arg;
        ret = mira220_set_total_gain_val(dev, u32_val);
        break;
    }
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        ESP_LOGI(TAG, "mira220_set_para_value ESP_CAM_SENSOR_GROUP_EXP_GAIN");
        uint32_t u32_val = *(uint32_t *)arg;
        // ret = mira220_set_total_gain_val(dev, u32_val);

        esp_cam_sensor_gh_exp_gain_t *value = (esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t ori_exp = 0;
        if (value->exposure_us != 0) {
            ori_exp = EXPOSURE_V4L2_TO_MIRA220(value->exposure_us, dev->cur_format);
        } else if (value->exposure_val != 0) {
            ori_exp = value->exposure_val;
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = mira220_set_exp_val(dev, ori_exp);
        ret |= mira220_set_total_gain_val(dev, value->gain_index);
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        //ret = mira220_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        //ret = mira220_set_mirror(dev, *value);
        break;
    }
    default: {
        ESP_LOGI(TAG, "set id=%" PRIx32 " is not supported", id);
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
    return 0;
}

static esp_err_t mira220_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct mira220_cam *cam_mira220 = (struct mira220_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    format = &mira220_format_info[CONFIG_CAMERA_MIRA220_MIPI_IF_FORMAT_INDEX_DEFAULT];
    ret = mira220_write_array(dev->sccb_handle, (mira220_reginfo_t *)format->regs);

    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Set format regs fail");
    //     return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    // }

    dev->cur_format = format;
    // init para
    cam_mira220->mira220_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_mira220->mira220_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;
    cam_mira220->mira220_para.exposure_max = dev->cur_format->isp_info->isp_v1_info.vts - 0;

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

    mira220_IO_MUX_LOCK(mux);
    ESP_LOGI(TAG, "mira220_priv_ioctl");
    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        break;
        ret = mira220_hw_reset(dev);
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        break;
        ret = mira220_soft_reset(dev);
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = mira220_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = mira220_set_stream(dev, *(int *)arg);
        ESP_LOGI(TAG, "SET STREAM");
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = mira220_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        break;
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = mira220_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
           sensor_reg->value = regval;
        }
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = mira220_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    mira220_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t mira220_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "MIRA POWER ONNNN");
    return ret;
}

static esp_err_t mira220_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    return ret;
}

static esp_err_t mira220_delete(esp_cam_sensor_device_t *dev)
{
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
    // s_limited_gain_index = ARRAY_SIZE(mira220_total_gain_val_map);


    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_mira220 = heap_caps_calloc(1, sizeof(struct mira220_cam), MALLOC_CAP_DEFAULT);
    if (!cam_mira220) {
        free(dev);
        return NULL;
    }

    dev->name = (char *)mira220_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &mira220_ops;
    dev->priv = cam_mira220;
    dev->cur_format = &mira220_format_info[CONFIG_CAMERA_MIRA220_MIPI_IF_FORMAT_INDEX_DEFAULT];


    // for (size_t i = 0; i < ARRAY_SIZE(mira220_total_gain_val_map); i++) {
    //     if (mira220_total_gain_val_map[i] > s_limited_gain) {
    //         s_limited_gain_index = i - 1;
    //         break;
    //     }

    // Configure sensor power, clock, and SCCB port
    // if (mira220_power_on(dev) != ESP_OK) {
    //     goto err_free_handler;
    // }

    // if (mira220_get_sensor_id(dev, &dev->id) != ESP_OK) {
    //     ESP_LOGE(TAG, "Get sensor ID failed");
    //     goto err_free_handler;
    // } else if (dev->id.pid != mira220_PID) {
    //     ESP_LOGE(TAG, "Camera sensor is not mira220, PID=0x%x", dev->id.pid);
    //     goto err_free_handler;
    // }

    return dev;

    // err_free_handler:
    //     mira220_power_off(dev);
    //     free(dev->priv);
    //     free(dev);
    // return NULL;
}

void mira220_get_settings(esp_cam_sensor_device_t *dev)
{
    uint8_t row_lenght_MSB, row_lenght_LSB; 
    mira220_read(dev->sccb_handle, 0x102B, &row_lenght_LSB);
    mira220_read(dev->sccb_handle, 0x102C, &row_lenght_MSB);
    uint8_t exp_reg_val_MSB, exp_reg_val_LSB; 
    mira220_read(dev->sccb_handle, 0x100C, &exp_reg_val_LSB);
    mira220_read(dev->sccb_handle, 0x100D, &exp_reg_val_MSB);
    uint8_t VSIZE_reg_val_MSB, VSIZE_reg_val_LSB; 
    mira220_read(dev->sccb_handle, 0x1087, &VSIZE_reg_val_LSB);
    mira220_read(dev->sccb_handle, 0x1088, &VSIZE_reg_val_MSB);
    uint8_t VBLANK_reg_val_MSB, VBLANK_reg_val_LSB; 
    mira220_read(dev->sccb_handle, 0x1012, &VBLANK_reg_val_LSB);
    mira220_read(dev->sccb_handle, 0x1013, &VBLANK_reg_val_MSB);

    MIRA220.row_lenght      = ( row_lenght_MSB*256 +  row_lenght_LSB);
    MIRA220.exp_reg_val     = (exp_reg_val_MSB*256 +  exp_reg_val_LSB);
    MIRA220.VSIZE_reg_val   = (VSIZE_reg_val_MSB*256 +  VSIZE_reg_val_LSB);
    MIRA220.VBLANK_reg_val  = (VBLANK_reg_val_MSB*256 +  VBLANK_reg_val_LSB);
    MIRA220.row_length_us       = MIRA220.row_lenght*1000000 / (mira220_format_info->xclk/1.0);
    MIRA220.time_exposure_us   = MIRA220.row_length_us * MIRA220.exp_reg_val;
    MIRA220.time_frame_us      = (1000000.0f / (mira220_format_info->xclk/1.0)) * MIRA220.row_lenght * (MIRA220.VSIZE_reg_val + MIRA220.VBLANK_reg_val);
    MIRA220.sensor_fps      = 1000000.0f / MIRA220.time_frame_us;
    MIRA220.time_exp_max_us    = MIRA220.time_frame_us -( 1000000*1928.0f / (mira220_format_info->xclk/1.0));
    MIRA220.exp_reg_min_val = 0x1;
    MIRA220.exp_reg_max_val = (MIRA220.time_exp_max_us / MIRA220.row_length_us);

    uint32_t vblank_max = 0xFFFF;
    MIRA220.sensor_fps_min = (mira220_format_info->xclk/1.0)/ ( MIRA220.row_lenght * (MIRA220.VSIZE_reg_val + vblank_max));
    MIRA220.VBLANK_reg_val_min = (1928.0/MIRA220.row_lenght) + 11;
    MIRA220.sensor_fps_max = (mira220_format_info->xclk/1.0) / (MIRA220.row_lenght * (MIRA220.VSIZE_reg_val + MIRA220.VBLANK_reg_val_min));
    MIRA220.VBLANK_reg_val_max = 0xFFFF;

    ESP_LOGI(TAG, "MIRA220 ROW LENGHT           %d "    , MIRA220.row_lenght);
    ESP_LOGI(TAG, "MIRA220 EXP REG              %d "    , MIRA220.exp_reg_val);
    ESP_LOGI(TAG, "MIRA220 VSIZE                %d "    , MIRA220.VSIZE_reg_val);
    ESP_LOGI(TAG, "MIRA220 VBLANK               %d "    , MIRA220.VBLANK_reg_val);
    ESP_LOGI(TAG, "MIRA220 ROW LEN   US         %.6f "  , MIRA220.row_length_us);
    ESP_LOGI(TAG, "MIRA220 TIME EXPOSURE us     %.6f "  , MIRA220.time_exposure_us);
    ESP_LOGI(TAG, "MIRA220 TIME FRAME us        %.6f "  , MIRA220.time_frame_us);
    ESP_LOGI(TAG, "MIRA220 SENSOR FPS           %.6f "  , MIRA220.sensor_fps);
    ESP_LOGI(TAG, "MIRA220 TIME EXPOSURE MAXus  %.6f "  , MIRA220.time_exp_max_us);
    ESP_LOGI(TAG, "MIRA220 MIN EXP REG VALUE    %d "    , MIRA220.exp_reg_min_val);
    ESP_LOGI(TAG, "MIRA220 MAX EXP REG VALUE    %d "    , MIRA220.exp_reg_max_val);
    return ;
}

void mira220_set_exp_us(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    uint32_t EXP_REG_VAL = (u32_val) / MIRA220.row_length_us; 
     mira220_set_exp_val(dev, EXP_REG_VAL); 
    return;
}

void mira220_set_fps(esp_cam_sensor_device_t *dev, uint32_t u32_val)
{
    float t_frame = 1.0/u32_val;
    float vblank = (t_frame / (MIRA220.row_lenght/38400000.0)) - MIRA220.VSIZE_reg_val ;
    uint32_t vblank_val = (vblank);
    ESP_LOGI(TAG, "  VBLANK CURRENT: %d ,  TRY : %d", MIRA220.VBLANK_reg_val, vblank_val);


    if (vblank_val < MIRA220.VBLANK_reg_val_min) {
        return;
    }
    if(vblank_val > MIRA220.VBLANK_reg_val_max) {
        return;
    } else {
        MIRA220.VBLANK_reg_val = vblank_val;
        mira220_write(dev->sccb_handle, 0x1012, vblank_val & 0x00FF);
        mira220_write(dev->sccb_handle, 0x1013, (vblank_val & 0xFF00)>>8);
        ESP_LOGI(TAG, "SET  VBLANK =%d to FPS = %d", MIRA220.VBLANK_reg_val, u32_val);
    }
}

ESP_CAM_SENSOR_DETECT_FN(mira220_detect, ESP_CAM_SENSOR_MIPI_CSI, mira220_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return mira220_detect(config);
}

