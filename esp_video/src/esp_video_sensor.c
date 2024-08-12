/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "esp_video_sensor.h"

#define EXPOSURE_V4L2_UNIT_US                   100
#define EXPOSURE_V4L2_TO_SENSOR(v, sf)          \
    ((int32_t)(((double)v) * (sf)->fps * (sf)->isp_info->isp_v1_info.vts / (1000000 / EXPOSURE_V4L2_UNIT_US) + 0.5))
#define EXPOSURE_SENSOR_TO_V4L2(v, sf)          \
    ((int32_t)(((double)v) * 1000000 / (sf)->fps / (sf)->isp_info->isp_v1_info.vts / EXPOSURE_V4L2_UNIT_US + 0.5))

struct control_map {
    uint32_t esp_cam_sensor_id;
    uint32_t v4l2_id;
};

static const char *TAG = "esp_video_sensor";

/**
 * Todo: AEG-1094
 */
static const struct control_map s_control_map_table[] = {
    {
        .esp_cam_sensor_id = ESP_CAM_SENSOR_JPEG_QUALITY,
        .v4l2_id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
    },
    {
        .esp_cam_sensor_id = ESP_CAM_SENSOR_3A_LOCK,
        .v4l2_id = V4L2_CID_3A_LOCK,
    },
    {
        .esp_cam_sensor_id = ESP_CAM_SENSOR_FLASH_LED,
        .v4l2_id = V4L2_CID_FLASH_LED_MODE,
    },
    {
        .esp_cam_sensor_id = ESP_CAM_SENSOR_VFLIP,
        .v4l2_id = V4L2_CID_VFLIP,
    },
    {
        .esp_cam_sensor_id = ESP_CAM_SENSOR_HMIRROR,
        .v4l2_id = V4L2_CID_HFLIP,
    },
    {
        .esp_cam_sensor_id = ESP_CAM_SENSOR_GAIN,
        .v4l2_id = V4L2_CID_GAIN,
    },
    {
        .esp_cam_sensor_id = ESP_CAM_SENSOR_EXPOSURE_VAL,
        .v4l2_id = V4L2_CID_EXPOSURE_ABSOLUTE,
    },
    {
        .esp_cam_sensor_id = ESP_CAM_SENSOR_EXPOSURE_VAL,
        .v4l2_id = V4L2_CID_EXPOSURE,
    },
};

/**
 * @brief Get control ID map pointer based on V4L2 control ID
 *
 * @param v4l2_id V4L2 control ID
 *
 * @return
 *      - Control ID map pointer on success
 *      - NULL if failed
 */
static const struct control_map *get_v4l2_ext_control_map(uint32_t v4l2_id)
{
    for (int i = 0; i < ARRAY_SIZE(s_control_map_table); i++) {
        if (s_control_map_table[i].v4l2_id == v4l2_id) {
            return &s_control_map_table[i];
        }
    }

    return NULL;
}

/**
 * @brief Get control internal operation parameters
 *
 * @param cam_dev  Camera sensor device pointer
 * @param ctrl     V4L2 external control pointer
 * @param qdesc    Camera sensor device query description pointer
 * @param buf_ptr  Actual data buffer pointer
 * @param buf_size Actual size buffer pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
static esp_err_t get_opt_value_desc(esp_cam_sensor_device_t *cam_dev, struct v4l2_ext_control *ctrl, esp_cam_sensor_param_desc_t *qdesc, void **buf_ptr, size_t *buf_size)
{
    esp_err_t ret;
    const struct control_map *control_map;

    control_map = get_v4l2_ext_control_map(ctrl->id);
    if (!control_map) {
        ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is not supported", ctrl->id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    qdesc->id = control_map->esp_cam_sensor_id;
    ret = esp_cam_sensor_query_para_desc(cam_dev, qdesc);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGD(TAG, "sensor %s doesn't support to query parameter description", cam_dev->name);

        *buf_ptr = &ctrl->value;
        *buf_size = sizeof(ctrl->value);

        qdesc->type = UINT32_MAX;
    } else if (ret == ESP_OK) {
        switch (qdesc->type) {
        case ESP_CAM_SENSOR_PARAM_TYPE_NUMBER:
        case ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION:
        case ESP_CAM_SENSOR_PARAM_TYPE_BITMASK:
            *buf_ptr = &ctrl->value;
            *buf_size = sizeof(ctrl->value);
            break;
        default:
            ESP_LOGE(TAG, "sensor description type=%" PRIu32 " is not supported", qdesc->type);
            return ESP_ERR_NOT_SUPPORTED;
        }
    } else {
        ESP_LOGE(TAG, "failed to query ctrl id=%" PRIx32, ctrl->id);
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Set control value to camera sensor device
 *
 * @param cam_dev  Camera sensor device pointer
 * @param controls V4L2 external controls pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_ext_ctrls_to_sensor(esp_cam_sensor_device_t *cam_dev, const struct v4l2_ext_controls *controls)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    for (int i = 0; i < controls->count; i++) {
        int32_t value_buf;
        size_t value_size;
        void *value_ptr;
        esp_cam_sensor_param_desc_t qdesc;
        struct v4l2_ext_control *ctrl = &controls->controls[i];

        ret = get_opt_value_desc(cam_dev, ctrl, &qdesc, &value_ptr, &value_size);
        if (ret != ESP_OK) {
            break;
        }

        if (ctrl->id == V4L2_CID_EXPOSURE_ABSOLUTE) {
            value_buf = EXPOSURE_V4L2_TO_SENSOR(ctrl->value, cam_dev->cur_format);
            value_ptr = &value_buf;
            value_size = sizeof(value_buf);
        } else {
            value_buf = ctrl->value;
        }

        switch (qdesc.type) {
        case ESP_CAM_SENSOR_PARAM_TYPE_NUMBER:
            if ((value_buf > qdesc.number.maximum) || (value_buf < qdesc.number.minimum) ||
                    (value_buf % qdesc.number.step)) {
                ESP_LOGE(TAG, "ctrl id=%" PRIx32 " value is out of range", ctrl->id);
                return ESP_ERR_INVALID_ARG;
            }
            break;
        case ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION: {
            if (value_buf < 0 || value_buf >= qdesc.enumeration.count) {
                ESP_LOGE(TAG, "ctrl id=%" PRIx32 " value is out of range", ctrl->id);
                return ESP_ERR_INVALID_ARG;
            }
            break;
        }
        case ESP_CAM_SENSOR_PARAM_TYPE_BITMASK:
            if (value_buf & (~qdesc.bitmask.value)) {
                ESP_LOGE(TAG, "ctrl id=%" PRIx32 " value is out of range", ctrl->id);
                return ESP_ERR_INVALID_ARG;
            }
            break;
        case UINT32_MAX:
            ESP_LOGD(TAG, "can't check ctrl id=%" PRIx32, ctrl->id);
            break;
        default:
            ESP_LOGE(TAG, "sensor description type=%" PRIx32 " is not supported", qdesc.type);
            return ESP_ERR_NOT_SUPPORTED;
        }

        ret = esp_cam_sensor_set_para_value(cam_dev, qdesc.id, value_ptr, value_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to set ctrl id=%" PRIx32, ctrl->id);
            break;
        }
    }

    return ret;
}

/**
 * @brief Get control value from camera sensor device
 *
 * @param cam_dev  Camera sensor device pointer
 * @param controls V4L2 external controls pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_ext_ctrls_from_sensor(esp_cam_sensor_device_t *cam_dev, struct v4l2_ext_controls *controls)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    for (int i = 0; i < controls->count; i++) {
        size_t value_size;
        void *value_ptr;
        esp_cam_sensor_param_desc_t qdesc;
        struct v4l2_ext_control *ctrl = &controls->controls[i];

        ret = get_opt_value_desc(cam_dev, ctrl, &qdesc, &value_ptr, &value_size);
        if (ret != ESP_OK) {
            break;
        }

        ret = esp_cam_sensor_get_para_value(cam_dev, qdesc.id, value_ptr, value_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to get ctrl id=%" PRIx32, ctrl->id);
            break;
        }

        if (ctrl->id == V4L2_CID_EXPOSURE_ABSOLUTE) {
            ctrl->value = EXPOSURE_SENSOR_TO_V4L2(ctrl->value, cam_dev->cur_format);
        }
    }

    return ret;
}

/**
 * @brief Get control description from camera sensor device
 *
 * @param cam_dev  Camera sensor device pointer
 * @param qctrl    V4L2 external controls query description pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_query_ext_ctrls_from_sensor(esp_cam_sensor_device_t *cam_dev, struct v4l2_query_ext_ctrl *qctrl)
{
    esp_err_t ret;
    const struct control_map *control_map;
    esp_cam_sensor_param_desc_t qdesc;

    control_map = get_v4l2_ext_control_map(qctrl->id);
    if (!control_map) {
        ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is not supported", qctrl->id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    qdesc.id = control_map->esp_cam_sensor_id;
    ret = esp_cam_sensor_query_para_desc(cam_dev, &qdesc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to query sensor id=%" PRIx32, qdesc.id);
        return ret;
    }

    /**
     * Todo: AEG-1095
     */
    switch (qdesc.type) {
    case ESP_CAM_SENSOR_PARAM_TYPE_NUMBER:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->maximum = qdesc.number.maximum;
        qctrl->minimum = qdesc.number.minimum;
        qctrl->step = qdesc.number.step;;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = qdesc.default_value;
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION:
        qctrl->type = V4L2_CTRL_TYPE_MENU;
        qctrl->maximum = qdesc.enumeration.count - 1;
        qctrl->minimum = 0;
        qctrl->step = 1;
        qctrl->elem_size = sizeof(uint32_t);
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->dims[0] = qctrl->elem_size;
        qctrl->default_value = qdesc.default_value;
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_BITMASK:
        qctrl->type = V4L2_CTRL_TYPE_BITMASK;
        qctrl->minimum = 0;
        qctrl->maximum = qdesc.bitmask.value;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = qdesc.default_value;
        break;
    default:
        ESP_LOGE(TAG, "sensor type=%" PRIu32 " is not supported", qdesc.type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (qctrl->id == V4L2_CID_EXPOSURE_ABSOLUTE) {
        qctrl->minimum = EXPOSURE_SENSOR_TO_V4L2(qctrl->minimum, cam_dev->cur_format);
        qctrl->maximum = EXPOSURE_SENSOR_TO_V4L2(qctrl->maximum, cam_dev->cur_format);
        qctrl->step = MAX(EXPOSURE_SENSOR_TO_V4L2(qctrl->step, cam_dev->cur_format), 1);
        qctrl->default_value = EXPOSURE_SENSOR_TO_V4L2(qctrl->default_value, cam_dev->cur_format);
    }

    return ESP_OK;
}
