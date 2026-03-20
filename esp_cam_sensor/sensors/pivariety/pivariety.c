/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "pivariety_settings.h"
#include "v4l2_cid.h"
#include "pivariety.h"

typedef struct {
    uint32_t exposure_val;
    uint32_t exposure_max;
    uint32_t gain_index; // current gain index

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} pivariety_para_t;

struct pivariety_cam {
    pivariety_para_t pivariety_para;
};

#define PIVARIETY_IO_MUX_LOCK(mux)
#define PIVARIETY_IO_MUX_UNLOCK(mux)
#define PIVARIETY_ENABLE_OUT_XCLK(pin,clk)
#define PIVARIETY_DISABLE_OUT_XCLK(pin)

#define PIVARIETY_FETCH_EXP_H(val)     (((val) >> 12) & 0xF)
#define PIVARIETY_FETCH_EXP_M(val)     (((val) >> 4) & 0xFF)
#define PIVARIETY_FETCH_EXP_L(val)     (((val) & 0xF) << 4)

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define PIVARIETY_SUPPORT_NUM CONFIG_CAMERA_PIVARIETY_MAX_SUPPORT

static esp_cam_sensor_format_t *pivariety_format_info;
static esp_cam_sensor_isp_info_t *pivariety_isp_info;
static size_t pivariety_format_info_size;
static const uint32_t s_limited_abs_gain = CONFIG_CAMERA_PIVARIETY_ABSOLUTE_GAIN_LIMIT;
static size_t s_limited_abs_gain_index;
static const char *TAG = "pivariety";

static uint32_t *pivariety_abs_gain_val_map;

static esp_err_t pivariety_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint32_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v32(sccb_handle, reg, read_buf);
}

static esp_err_t pivariety_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint32_t data)
{
    return esp_sccb_transmit_reg_a16v32(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t pivariety_write_array(esp_sccb_io_handle_t sccb_handle, pivariety_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && i < (int)regs_size && regarray[i].reg != PIVARIETY_REG_END) {
        if (regarray[i].reg != PIVARIETY_REG_DELAY) {
            ret = pivariety_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    if (i >= (int)regs_size && regarray[i-1].reg != PIVARIETY_REG_END) {
        ESP_LOGW(TAG, "write_array reached regs_size without PIVARIETY_REG_END; possible malformed regs");
    }
    return ret;
}

static esp_err_t pivariety_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t pivariety_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    return ret;
}

static esp_err_t pivariety_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint32_t pid;

    ret = pivariety_read(dev->sccb_handle, DEVICE_ID_REG, &pid);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = pid ;

    return ret;
}

static esp_err_t pivariety_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = pivariety_write(dev->sccb_handle, STREAM_ON, enable ? 0x01 : 0x00);

    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);

    return ret;
}

static esp_err_t pivariety_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    pivariety_write(dev->sccb_handle, CTRL_ID_REG, V4L2_CID_HFLIP);
    return pivariety_write(dev->sccb_handle, CTRL_VALUE_REG, enable ? 0x01 : 0x00);
}

static esp_err_t pivariety_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    pivariety_write(dev->sccb_handle, CTRL_ID_REG, V4L2_CID_VFLIP);
    return pivariety_write(dev->sccb_handle, CTRL_VALUE_REG, enable ? 0x01 : 0x00);
}

static esp_err_t pivariety_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0x08;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - 6; // max = VTS-6 = height+vblank-6, so when update vblank, exposure_max must be updated
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = s_limited_abs_gain_index;
        qdesc->enumeration.elements = pivariety_abs_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def; // default gain index
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

static esp_err_t pivariety_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_pivariety->pivariety_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_pivariety->pivariety_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t pivariety_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    uint32_t u32_val = 0;
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;

    if (arg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    u32_val = *(uint32_t *)arg;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        ESP_LOGD(TAG, "set exposure 0x%" PRIx32, u32_val);
        ret = pivariety_write(dev->sccb_handle, CTRL_ID_REG, V4L2_CID_EXPOSURE);
        if (ret != ESP_OK) break;
        ret = pivariety_write(dev->sccb_handle, CTRL_VALUE_REG, u32_val);

        if (ret == ESP_OK) {
            cam_pivariety->pivariety_para.exposure_val = u32_val;
        }
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        if (u32_val >= s_limited_abs_gain_index) {
            ESP_LOGE(TAG, "gain index out of range: %u >= %zu", u32_val, s_limited_abs_gain_index);
            return ESP_ERR_INVALID_ARG;
        }
        ret = pivariety_write(dev->sccb_handle, CTRL_ID_REG, V4L2_CID_ANALOGUE_GAIN);
        if (ret != ESP_OK) break;
        ret = pivariety_write(dev->sccb_handle, CTRL_VALUE_REG, pivariety_abs_gain_val_map[u32_val]);
        if (ret == ESP_OK) {
            cam_pivariety->pivariety_para.gain_index = u32_val;
        }
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = pivariety_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = pivariety_set_mirror(dev, *value);
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

static esp_err_t pivariety_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    if (pivariety_format_info != NULL && pivariety_format_info_size > 0) {
        formats->count = pivariety_format_info_size;
        formats->format_array = (const esp_cam_sensor_format_t *)&pivariety_format_info[0];
    } else {
        formats->count = 0;
        formats->format_array = NULL;
    }

    return ESP_OK;
}

static esp_err_t pivariety_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t pivariety_get_length_of_set(esp_cam_sensor_device_t *dev, uint16_t reg, uint32_t *length)
{
    uint32_t index = 0;
    uint32_t val = 0;
    uint32_t tmp_len = 0;
    esp_err_t ret = ESP_OK;

    ret = pivariety_read(dev->sccb_handle, reg, &val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get length of set fail");
        return ret;
    }

    while (1) {
        ret += pivariety_write(dev->sccb_handle, reg, index);
        ret += pivariety_read(dev->sccb_handle, reg, &tmp_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "get length of set fail");
            return ret;
        }

        if (tmp_len == ERROR_DATA) {
            break;
        }

        index++;
        /* guard to avoid infinite loop */
        if (index > 10000) {
            ESP_LOGE(TAG, "too many entries when getting length of set");
            return ESP_ERR_INVALID_SIZE;
        }
    }

    *length = index;

    ret = pivariety_write(dev->sccb_handle, reg, val);
    return ret;
}

static esp_err_t pivariety_map_format(pivariety_pixtype_t pivariety_format_type, esp_cam_sensor_format_t *format_info) 
{
    if (format_info == NULL) {
        ESP_LOGE(TAG, "format_info is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    const char *format_label = NULL;
    switch (pivariety_format_type) {
        case PIVARIETY_RAW8:
            format_info->format = ESP_CAM_SENSOR_PIXFORMAT_RAW8;
            format_label = "RAW8";
            break;
        case PIVARIETY_RAW10:
            format_info->format = ESP_CAM_SENSOR_PIXFORMAT_RAW10;
            format_label = "RAW10";
            break;
        case PIVARIETY_RAW12:
            format_info->format = ESP_CAM_SENSOR_PIXFORMAT_RAW12;
            format_label = "RAW12";
            break;
        case PIVARIETY_YUV420_8BIT:
            format_info->format = ESP_CAM_SENSOR_PIXFORMAT_YUV420;
            format_label = "YUV420_8bit";
            break;
        case PIVARIETY_YUV420_10BIT:
            format_info->format = ESP_CAM_SENSOR_PIXFORMAT_YUV420;
            format_label = "YUV420_10bit";
            break;
        case PIVARIETY_YUV422_8BIT:
            format_info->format = ESP_CAM_SENSOR_PIXFORMAT_YUV422;
            format_label = "YUV422_8bit";
            break;
        case PIVARIETY_JPEG:
            format_info->format = ESP_CAM_SENSOR_PIXFORMAT_JPEG;
            format_label = "JPEG";
            break;
        default:
            ESP_LOGE(TAG, "Unsupported format: 0x%x", pivariety_format_type);
            return ESP_ERR_NOT_SUPPORTED;
    }

    char tmp[128];
    unsigned lanes = (unsigned)format_info->mipi_info.lane_num;
    unsigned xclk_mhz = (unsigned)(format_info->xclk / 1000000U);
    snprintf(tmp, sizeof(tmp), "MIPI_%ulane_%uMinput_%s_%ux%u_%ufps",
                lanes, xclk_mhz, (format_label ? format_label : "UNKNOWN"),
                (unsigned)format_info->width, (unsigned)format_info->height, (unsigned)format_info->fps);

    format_info->name = strdup(tmp);

    return ESP_OK;
}

static esp_err_t pivariety_map_bayerorder(pivariety_bayerorder_t pivariety_bayer_order, esp_cam_sensor_bayer_pattern_t *esp_bayer_order) 
{
    switch (pivariety_bayer_order) {
        case PIVARIETY_BAYER_ORDER_BGGR:
            *esp_bayer_order = ESP_CAM_SENSOR_BAYER_BGGR;
            break;
        case PIVARIETY_BAYER_ORDER_GBRG:
            *esp_bayer_order = ESP_CAM_SENSOR_BAYER_GBRG;
            break;
        case PIVARIETY_BAYER_ORDER_GRBG:
            *esp_bayer_order = ESP_CAM_SENSOR_BAYER_GRBG;
            break;
        case PIVARIETY_BAYER_ORDER_RGGB:
            *esp_bayer_order = ESP_CAM_SENSOR_BAYER_RGGB;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported bayer order: %d", pivariety_bayer_order);
            return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t pivariety_read_ctrl(esp_cam_sensor_device_t *dev, uint32_t v4l2_cid, uint32_t *value, pivariety_ctrltype_t ctrl_type)
{
    esp_err_t ret = pivariety_write(dev->sccb_handle, CTRL_ID_REG, v4l2_cid);
    switch(ctrl_type) {
        case PIVARIETY_CTRL_MIN:
            ret += pivariety_read(dev->sccb_handle, CTRL_MIN_REG, value);
            break;
        case PIVARIETY_CTRL_MAX:
            ret += pivariety_read(dev->sccb_handle, CTRL_MAX_REG, value);
            break;
        case PIVARIETY_CTRL_STEP:
            ret += pivariety_read(dev->sccb_handle, CTRL_STEP_REG, value);
            break;
        case PIVARIETY_CTRL_DEF:
            ret += pivariety_read(dev->sccb_handle, CTRL_DEF_REG, value);
            break;
        case PIVARIETY_CTRL_VALUE:
            ret += pivariety_read(dev->sccb_handle, CTRL_VALUE_REG, value);
            break;
        default:
            ESP_LOGE(TAG, "Unsupported ctrl type: %d", ctrl_type);
            return ESP_ERR_NOT_SUPPORTED;
    }

    return ret;
}

static esp_err_t pivariety_write_ctrl(esp_cam_sensor_device_t *dev, uint32_t v4l2_cid, uint32_t value, pivariety_ctrltype_t ctrl_type)
{
    esp_err_t ret = pivariety_write(dev->sccb_handle, CTRL_ID_REG, v4l2_cid);
    switch(ctrl_type) {
        case PIVARIETY_CTRL_MIN:
            ret += pivariety_write(dev->sccb_handle, CTRL_MIN_REG, value);
            break;
        case PIVARIETY_CTRL_MAX:
            ret += pivariety_write(dev->sccb_handle, CTRL_MAX_REG, value);
            break;
        case PIVARIETY_CTRL_STEP:
            ret += pivariety_write(dev->sccb_handle, CTRL_STEP_REG, value);
            break;
        case PIVARIETY_CTRL_DEF:
            ret += pivariety_write(dev->sccb_handle, CTRL_DEF_REG, value);
            break;
        case PIVARIETY_CTRL_VALUE:
            ret += pivariety_write(dev->sccb_handle, CTRL_VALUE_REG, value);
            break;
        default:
            ESP_LOGE(TAG, "Unsupported ctrl type: %d", ctrl_type);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    return ret;
}

static esp_err_t pivariety_update_ctrl(esp_cam_sensor_device_t *dev, uint32_t v4l2_cid, uint32_t *value, pivariety_ctrltype_t ctrl_type)
{
    esp_err_t ret = pivariety_read_ctrl(dev, v4l2_cid, value, PIVARIETY_CTRL_VALUE);
    ret += pivariety_write_ctrl(dev, v4l2_cid, *value, PIVARIETY_CTRL_VALUE);
    ret += pivariety_read_ctrl(dev, v4l2_cid, value, ctrl_type);

    return ret;
}

static void pivariety_dump_format(esp_cam_sensor_format_t *format_info)
{
    if (format_info == NULL) {
        ESP_LOGI(TAG, "No format info available");
        return;
    }

    ESP_LOGI(TAG, "---------- Format info ----------");
    ESP_LOGI(TAG, "name: %s", (format_info->name ? format_info->name : "(null)"));
    ESP_LOGI(TAG, "format(enum): %d", (int)format_info->format);
    ESP_LOGI(TAG, "port: %d, xclk: %u", (int)format_info->port, (unsigned)format_info->xclk);
    ESP_LOGI(TAG, "width: %u, height: %u, fps: %u", (unsigned)format_info->width, (unsigned)format_info->height, (unsigned)format_info->fps);

    ESP_LOGI(TAG, "mipi_info: mipi_clk=%u, lanes=%u, line_sync_en=%d",
                (unsigned)format_info->mipi_info.mipi_clk,
                (unsigned)format_info->mipi_info.lane_num,
                (int)format_info->mipi_info.line_sync_en);

    ESP_LOGI(TAG, "regs_size: %zu", format_info->regs_size);
    if (format_info->regs && format_info->regs_size > 0) {
        const pivariety_reginfo_t *regs = (const pivariety_reginfo_t *)format_info->regs;
        for (size_t j = 0; j < format_info->regs_size; j++) {
            if (regs[j].reg == PIVARIETY_REG_END) {
                ESP_LOGI(TAG, "regs[%zu]: END", j);
                break;
            }
            if (regs[j].reg == PIVARIETY_REG_DELAY) {
                ESP_LOGI(TAG, "regs[%zu]: DELAY %u ms", j, (unsigned)regs[j].val);
            } else {
                ESP_LOGI(TAG, "regs[%zu]: reg=0x%04x val=0x%08x", j, (unsigned)regs[j].reg, (unsigned)regs[j].val);
            }
        }
    } else {
        ESP_LOGI(TAG, "regs: (null)");
    }
    ESP_LOGI(TAG, "---------------------------------");

    if (format_info->isp_info) {
        ESP_LOGI(TAG, "----------- ISP info -----------");
        ESP_LOGI(TAG, "version: %u", (unsigned)format_info->isp_info->isp_v1_info.version);
        ESP_LOGI(TAG, "pclk: %u, vts: %u, hts: %u, tline_ns: %u",
                    (unsigned)format_info->isp_info->isp_v1_info.pclk,
                    (unsigned)format_info->isp_info->isp_v1_info.vts,
                    (unsigned)format_info->isp_info->isp_v1_info.hts,
                    (unsigned)format_info->isp_info->isp_v1_info.tline_ns);
        ESP_LOGI(TAG, "gain_def: %u, exp_def: %u, bayer_type: %d",
                    (unsigned)format_info->isp_info->isp_v1_info.gain_def,
                    (unsigned)format_info->isp_info->isp_v1_info.exp_def,
                    (int)format_info->isp_info->isp_v1_info.bayer_type);
    } else {
        ESP_LOGI(TAG, "isp_info: (null)");
    }
    ESP_LOGI(TAG, "--------------------------------");
}

static esp_err_t pivariety_enum_format(esp_cam_sensor_device_t *dev)
{
    uint32_t width, height;
    uint32_t num_format = 0;
    uint32_t num_resolution = 0;
    uint32_t format_type, bayer_type;
    esp_err_t ret = ESP_OK;
    esp_cam_sensor_format_t format_info = {
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .mipi_info = {
            .mipi_clk = 640000000,
            .line_sync_en = false,
        },
        .reserved = NULL,
    };
    esp_cam_sensor_isp_info_t isp_info = {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
        },
    };
    
    ret = pivariety_get_length_of_set(dev, PIXFORMAT_INDEX_REG, &num_format);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = pivariety_get_length_of_set(dev, RESOLUTION_INDEX_REG, &num_resolution);
    if (ret != ESP_OK) {
        return ret;
    }

    if (num_format == 0 || num_resolution == 0) {
        ESP_LOGE(TAG, "no formats/resolutions reported by sensor");
        return ESP_ERR_NOT_FOUND;
    }

    if (pivariety_format_info == NULL) {
        size_t total = num_format * num_resolution;
        pivariety_format_info = calloc(total, sizeof(esp_cam_sensor_format_t));
        pivariety_format_info_size = total;
        if (!pivariety_format_info) {
            ESP_LOGE(TAG, "No memory for format info");
            return ESP_ERR_NO_MEM;
        }
    }

    if (pivariety_isp_info == NULL) {
        size_t total = num_format * num_resolution;
        pivariety_isp_info = calloc(total, sizeof(esp_cam_sensor_isp_info_t));
        if (!pivariety_isp_info) {
            ESP_LOGE(TAG, "No memory for isp info");
            return ESP_ERR_NO_MEM;
        }
    }

    // Enumerate formats and resolutions
    for (uint32_t format_index = 0; format_index < num_format; format_index++) {
        ret += pivariety_write(dev->sccb_handle, PIXFORMAT_INDEX_REG, format_index);
        ret += pivariety_read(dev->sccb_handle, PIXFORMAT_TYPE_REG, &format_type);
        ret += pivariety_read(dev->sccb_handle, MIPI_LANES_REG, &format_info.mipi_info.lane_num);
        ret += pivariety_read(dev->sccb_handle, BAYER_ORDER_REG, &bayer_type);
        ret += pivariety_map_bayerorder(bayer_type, &isp_info.isp_v1_info.bayer_type);
        for (uint32_t res_index = 0; res_index < num_resolution; res_index++) {
            uint32_t index = format_index * num_resolution + res_index;
            ret += pivariety_write(dev->sccb_handle, RESOLUTION_INDEX_REG, res_index);
            ret += pivariety_read(dev->sccb_handle, FORMAT_WIDTH_REG, &width);
            ret += pivariety_read(dev->sccb_handle, FORMAT_HEIGHT_REG, &height);
            ret += pivariety_update_ctrl(dev, V4L2_CID_ARDUCAM_FRAME_RATE, (uint32_t*)&format_info.fps, PIVARIETY_CTRL_DEF);
            ret += pivariety_update_ctrl(dev, V4L2_CID_PIXEL_RATE, (uint32_t*)&isp_info.isp_v1_info.pclk, PIVARIETY_CTRL_DEF);
            ret += pivariety_update_ctrl(dev, V4L2_CID_VBLANK, (uint32_t*)&isp_info.isp_v1_info.vts, PIVARIETY_CTRL_DEF);
            ret += pivariety_update_ctrl(dev, V4L2_CID_HBLANK, (uint32_t*)&isp_info.isp_v1_info.hts, PIVARIETY_CTRL_DEF);
            ret += pivariety_update_ctrl(dev, V4L2_CID_ANALOGUE_GAIN, (uint32_t*)&isp_info.isp_v1_info.gain_def, PIVARIETY_CTRL_DEF);
            ret += pivariety_update_ctrl(dev, V4L2_CID_EXPOSURE, (uint32_t*)&isp_info.isp_v1_info.exp_def, PIVARIETY_CTRL_DEF);
            if(ret != ESP_OK){
                return ret;
            }
            
            size_t regs_count = 3;
            pivariety_reginfo_t *regs = calloc(regs_count, sizeof(pivariety_reginfo_t));
            if (regs == NULL) {
                ESP_LOGE(TAG, "No memory for regs for index %u", index);
                return ESP_ERR_NO_MEM;
            }
            regs[0].reg = PIXFORMAT_INDEX_REG;  regs[0].val = format_index;
            regs[1].reg = RESOLUTION_INDEX_REG; regs[1].val = res_index;
            regs[2].reg = PIVARIETY_REG_END;    regs[2].val = 0x00000000;

            format_info.regs = regs;
            format_info.regs_size = regs_count;
            format_info.width = width;
            format_info.height = height;
            isp_info.isp_v1_info.hts += width;
            isp_info.isp_v1_info.vts += height;
            isp_info.isp_v1_info.tline_ns = (uint32_t)(isp_info.isp_v1_info.hts / (isp_info.isp_v1_info.pclk / 1000000.0) * 1000);
            ret += pivariety_map_format(format_type, &format_info);

            memcpy(&pivariety_isp_info[index], &isp_info, sizeof(esp_cam_sensor_isp_info_t));
            memcpy(&pivariety_format_info[index], &format_info, sizeof(esp_cam_sensor_format_t));
            pivariety_format_info[index].isp_info = &pivariety_isp_info[index];

            pivariety_dump_format(&pivariety_format_info[index]);
        }
    }

    ret += pivariety_write(dev->sccb_handle, PIXFORMAT_INDEX_REG, 0);
    ret += pivariety_write(dev->sccb_handle, RESOLUTION_INDEX_REG, 0);
    
    return ret;
}

static esp_err_t pivariety_get_gain_val_map(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    ret = pivariety_read(dev->sccb_handle, IPC_GAIN_TABLE_LENGTH_REG, (uint32_t*)&s_limited_abs_gain_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read gain table length");
        return ret;
    }

    if (pivariety_abs_gain_val_map == NULL) {
        pivariety_abs_gain_val_map = calloc(s_limited_abs_gain_index, sizeof(uint32_t));
        if (!pivariety_abs_gain_val_map) {
            ESP_LOGE(TAG, "No memory for gain val map");
            return ESP_ERR_NO_MEM;
        }
    }

    for (size_t i = 0; i < s_limited_abs_gain_index; i++) {
        ret += pivariety_read(dev->sccb_handle, IPC_GAIN_TABLE_DATA_REG, &pivariety_abs_gain_val_map[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read gain value");
            return ret;
        }
    }

    return ret;
}

static esp_err_t pivariety_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    struct pivariety_cam *cam_pivariety = (struct pivariety_cam *)dev->priv;
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &pivariety_format_info[CONFIG_CAMERA_PIVARIETY_MIPI_IF_FORMAT_INDEX_DEFAULT];
    }

    ret = pivariety_write_array(dev->sccb_handle, (pivariety_reginfo_t *)format->regs, format->regs_size);
    delay_ms(500);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_pivariety->pivariety_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_pivariety->pivariety_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;

    return ret;
}

static esp_err_t pivariety_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t pivariety_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint32_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    PIVARIETY_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = pivariety_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = pivariety_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = pivariety_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = pivariety_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = pivariety_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = pivariety_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    PIVARIETY_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t pivariety_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        PIVARIETY_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
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

static esp_err_t pivariety_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        PIVARIETY_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t pivariety_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del pivariety (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    if (pivariety_format_info) {
        for (size_t i = 0; i < pivariety_format_info_size; i++) {
            if (pivariety_format_info[i].regs) {
                free((void *)pivariety_format_info[i].regs);
                pivariety_format_info[i].regs = NULL;
            }
        }
        free(pivariety_format_info);
        pivariety_format_info = NULL;
        pivariety_format_info_size = 0;
    }
    if (pivariety_isp_info) {
        free(pivariety_isp_info);
        pivariety_isp_info = NULL;
    }
    if (pivariety_abs_gain_val_map) {
        free(pivariety_abs_gain_val_map);
        pivariety_abs_gain_val_map = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t pivariety_ops = {
    .query_para_desc = pivariety_query_para_desc,
    .get_para_value = pivariety_get_para_value,
    .set_para_value = pivariety_set_para_value,
    .query_support_formats = pivariety_query_support_formats,
    .query_support_capability = pivariety_query_support_capability,
    .set_format = pivariety_set_format,
    .get_format = pivariety_get_format,
    .priv_ioctl = pivariety_priv_ioctl,
    .del = pivariety_delete
};

esp_cam_sensor_device_t *pivariety_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct pivariety_cam *cam_pivariety;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_pivariety = heap_caps_calloc(1, sizeof(struct pivariety_cam), MALLOC_CAP_DEFAULT);
    if (!cam_pivariety) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)PIVARIETY_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &pivariety_ops;
    dev->priv = cam_pivariety;
    
    if (pivariety_enum_format(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Enum format fail");
        goto err_free_handler;
    }
    dev->cur_format = (const esp_cam_sensor_format_t *)&pivariety_format_info[CONFIG_CAMERA_PIVARIETY_MIPI_IF_FORMAT_INDEX_DEFAULT];

    if (pivariety_get_gain_val_map(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Get gain val map fail");
        goto err_free_handler;
    }

    for (size_t i = 0; i < s_limited_abs_gain_index; i++) {
        if (pivariety_abs_gain_val_map[i] > s_limited_abs_gain) {
            s_limited_abs_gain_index = i; // number of allowed entries is i
            break;
        }
    }

    // Configure sensor power, clock, and SCCB port
    if (pivariety_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (pivariety_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != PIVARIETY_PID) {
        ESP_LOGE(TAG, "Camera sensor is not PIVARIETY, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    pivariety_power_off(dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }
    if (pivariety_format_info) {
        for (size_t i = 0; i < pivariety_format_info_size; i++) {
            if (pivariety_format_info[i].regs) {
                free((void *)pivariety_format_info[i].regs);
                pivariety_format_info[i].regs = NULL;
            }
        }
        free(pivariety_format_info);
        pivariety_format_info = NULL;
        pivariety_format_info_size = 0;
    }
    if (pivariety_isp_info) {
        free(pivariety_isp_info);
        pivariety_isp_info = NULL;
    }
    if (pivariety_abs_gain_val_map) {
        free(pivariety_abs_gain_val_map);
        pivariety_abs_gain_val_map = NULL;
    }

    return NULL;
}

#if CONFIG_CAMERA_PIVARIETY_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(pivariety_detect, ESP_CAM_SENSOR_MIPI_CSI, PIVARIETY_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return pivariety_detect(config);
}
#endif
