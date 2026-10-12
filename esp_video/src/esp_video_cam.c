/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "esp_log.h"
#include "esp_cam_sensor.h"
#include <inttypes.h>
#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
#include "esp_cam_motor.h"
#endif
#if CONFIG_ESP_VIDEO_ENABLE_IRCUT_SUPPORT
#include "esp_cam_ircut.h"
#endif
#include "esp_video_ioctl.h"
#include "esp_video_cam.h"

struct control_map {
    uint32_t esp_cam_priv_id;
    uint32_t v4l2_id;
    const char *name;
};

struct cam_ctrl_ops {
    esp_err_t (*set)(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl);
    esp_err_t (*get)(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl);
    esp_err_t (*query)(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_query_ext_ctrl *qctrl);
    esp_err_t (*query_menu)(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_querymenu *qmenu);
    bool (*supported)(esp_video_cam_t *cam, const struct control_map *map);
};

struct cam_ctrl_subsys {
    const struct control_map *table;
    int size;
    bool enumerable;
    const struct cam_ctrl_ops *ops;
};

static const char *TAG = "esp_video_cam";

static esp_err_t ctrl_buf_from_desc(const esp_cam_sensor_param_desc_t *qdesc, struct v4l2_ext_control *ctrl,
                                    void **buf_ptr, size_t *buf_size)
{
    switch (qdesc->type) {
    case ESP_CAM_SENSOR_PARAM_TYPE_NUMBER:
    case ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION:
    case ESP_CAM_SENSOR_PARAM_TYPE_BITMASK:
        *buf_ptr = &ctrl->value;
        *buf_size = sizeof(ctrl->value);
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_U8:
        *buf_ptr = ctrl->p_u8;
        *buf_size = ctrl->size;
        break;
    default:
        ESP_LOGE(TAG, "sensor description type=%" PRIu32 " is not supported", qdesc->type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t check_ctrl_value(const struct v4l2_ext_control *ctrl, const esp_cam_sensor_param_desc_t *qdesc,
                                  size_t value_size)
{
    int32_t value_buf = ctrl->value;

    switch (qdesc->type) {
    case ESP_CAM_SENSOR_PARAM_TYPE_NUMBER:
        if ((value_buf > qdesc->number.maximum) || (value_buf < qdesc->number.minimum) ||
                (value_buf % qdesc->number.step)) {
            ESP_LOGE(TAG, "number: ctrl id=%" PRIx32 " value=%" PRIi32 " is out of range(max=%" PRIi32 ", min=%" PRIi32 ", step=%"PRIu32")",
                     ctrl->id, value_buf, (int32_t)qdesc->number.maximum, (int32_t)qdesc->number.minimum,
                     (uint32_t)qdesc->number.step);
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION:
        if (value_buf < 0 || value_buf >= qdesc->enumeration.count) {
            ESP_LOGE(TAG, "enum: ctrl id=%" PRIx32 " value=%" PRIu32 " is out of range(from 0 to %" PRIu32 ")",
                     ctrl->id, value_buf, qdesc->enumeration.count);
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_BITMASK:
        if (value_buf & (~qdesc->bitmask.value)) {
            ESP_LOGE(TAG, "mask: ctrl id=%" PRIx32 " value=%" PRIx32 " is out of range(%" PRIx32 ")",
                     ctrl->id, value_buf, qdesc->bitmask.value);
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_U8:
        if (value_size != qdesc->u8.size) {
            ESP_LOGE(TAG, "mask: ctrl id=%" PRIx32 " size=%zu is not equal to sensor data size(%" PRIu32 ")",
                     ctrl->id, value_size, qdesc->u8.size);
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case UINT32_MAX:
        ESP_LOGD(TAG, "can't check ctrl id=%" PRIx32, ctrl->id);
        break;
    default:
        ESP_LOGE(TAG, "sensor description type=%" PRIx32 " is not supported", qdesc->type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/**
 * Todo: AEG-1095
 */
static esp_err_t fill_query_ext_ctrl(struct v4l2_query_ext_ctrl *qctrl, const char *name,
                                     const esp_cam_sensor_param_desc_t *qdesc)
{
    int name_len = snprintf(qctrl->name, sizeof(qctrl->name), "%s", name);

    if (name_len < 0) {
        ESP_LOGE(TAG, "failed to fill name of cmd id=%" PRIx32, qctrl->id);
        return ESP_FAIL;
    } else if (name_len >= sizeof(qctrl->name)) {
        ESP_LOGE(TAG, "name of cmd id=%" PRIx32 " is too long", qctrl->id);
        return ESP_ERR_INVALID_ARG;
    }

    switch (qdesc->type) {
    case ESP_CAM_SENSOR_PARAM_TYPE_NUMBER:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->maximum = qdesc->number.maximum;
        qctrl->minimum = qdesc->number.minimum;
        qctrl->step = qdesc->number.step;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = qdesc->default_value;
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER_MENU;
        qctrl->maximum = qdesc->enumeration.count - 1;
        qctrl->minimum = 0;
        qctrl->step = 1;
        qctrl->elem_size = sizeof(uint32_t);
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->dims[0] = qctrl->elem_size;
        qctrl->default_value = qdesc->default_value;
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_BITMASK:
        qctrl->type = V4L2_CTRL_TYPE_BITMASK;
        qctrl->minimum = 0;
        qctrl->maximum = qdesc->bitmask.value;
        qctrl->step = 1;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = qdesc->default_value;
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_U8:
        qctrl->type = V4L2_CTRL_TYPE_U8;
        qctrl->minimum = 0;
        qctrl->maximum = UINT8_MAX;
        qctrl->step = 1;
        qctrl->elem_size = qdesc->u8.size;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->dims[0] = 1;
        qctrl->default_value = 0;
        break;
    default:
        ESP_LOGD(TAG, "sensor type=%" PRIu32 " is not supported", qdesc->type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

/* Parameter names must not match designated-initializer fields (.v4l2_id / .name). */
#define CAM_CTRL_MAP(_priv_id, _v4l2_id, _ctrl_name) \
    { \
        .esp_cam_priv_id = (_priv_id), \
        .v4l2_id = (_v4l2_id), \
        .name = (_ctrl_name), \
    },

#define CAM_SUBSYS_ENTRY(prefix, enumerable_flag) \
    { \
        .table = s_##prefix##_table, \
        .size = ARRAY_SIZE(s_##prefix##_table), \
        .enumerable = (enumerable_flag), \
        .ops = &s_##prefix##_ops, \
    },

/**
 * Parameter subsystem: query/get/set para API.
 * require_dev is 1 when the device pointer may be NULL (motor).
 */
#define CAM_PARA_SUBSYS_DEFINE(prefix, dev_member, query_fn, get_fn, set_fn, require_dev, map_table) \
    static const struct control_map s_##prefix##_table[] = { \
        map_table \
    }; \
    \
    static bool prefix##_supported(esp_video_cam_t *cam, const struct control_map *map) \
    { \
        esp_cam_sensor_param_desc_t qdesc = { \
            .id = map->esp_cam_priv_id, \
        }; \
        \
        if ((require_dev) && !cam->dev_member) { \
            return false; \
        } \
        \
        return query_fn(cam->dev_member, &qdesc) == ESP_OK; \
    } \
    \
    static esp_err_t prefix##_load(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl, \
                                   esp_cam_sensor_param_desc_t *qdesc, void **buf_ptr, size_t *buf_size) \
    { \
        esp_err_t ret; \
        \
        if ((require_dev) && !cam->dev_member) { \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        qdesc->id = map->esp_cam_priv_id; \
        ret = query_fn(cam->dev_member, qdesc); \
        if (ret == ESP_ERR_NOT_SUPPORTED) { \
            ESP_LOGD(TAG, "sensor %s doesn't support to query parameter description", cam->dev_member->name); \
            *buf_ptr = &ctrl->value; \
            *buf_size = sizeof(ctrl->value); \
            qdesc->type = UINT32_MAX; \
            return ESP_OK; \
        } \
        if (ret != ESP_OK) { \
            ESP_LOGE(TAG, "failed to query ctrl id=%" PRIx32, ctrl->id); \
            return ret; \
        } \
        \
        return ctrl_buf_from_desc(qdesc, ctrl, buf_ptr, buf_size); \
    } \
    \
    static esp_err_t prefix##_set(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl) \
    { \
        esp_err_t ret; \
        void *value_ptr; \
        size_t value_size; \
        esp_cam_sensor_param_desc_t qdesc; \
        \
        ret = prefix##_load(cam, map, ctrl, &qdesc, &value_ptr, &value_size); \
        if (ret != ESP_OK) { \
            return ret; \
        } \
        \
        ret = check_ctrl_value(ctrl, &qdesc, value_size); \
        if (ret != ESP_OK) { \
            return ret; \
        } \
        \
        ret = set_fn(cam->dev_member, qdesc.id, value_ptr, value_size); \
        if (ret != ESP_OK) { \
            ESP_LOGE(TAG, "failed to set ctrl id=%" PRIx32, ctrl->id); \
        } \
        return ret; \
    } \
    \
    static esp_err_t prefix##_get(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl) \
    { \
        esp_err_t ret; \
        void *value_ptr; \
        size_t value_size; \
        esp_cam_sensor_param_desc_t qdesc; \
        \
        ret = prefix##_load(cam, map, ctrl, &qdesc, &value_ptr, &value_size); \
        if (ret != ESP_OK) { \
            return ret; \
        } \
        \
        ret = get_fn(cam->dev_member, qdesc.id, value_ptr, value_size); \
        if (ret != ESP_OK) { \
            ESP_LOGE(TAG, "failed to get ctrl id=%" PRIx32, ctrl->id); \
        } \
        return ret; \
    } \
    \
    static esp_err_t prefix##_query(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_query_ext_ctrl *qctrl) \
    { \
        esp_err_t ret; \
        esp_cam_sensor_param_desc_t qdesc = { \
            .id = map->esp_cam_priv_id, \
        }; \
        \
        if ((require_dev) && !cam->dev_member) { \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        ret = query_fn(cam->dev_member, &qdesc); \
        if (ret != ESP_OK) { \
            ESP_LOGD(TAG, "failed to query sensor id=%" PRIx32, qdesc.id); \
            return ret; \
        } \
        \
        return fill_query_ext_ctrl(qctrl, map->name, &qdesc); \
    } \
    \
    static esp_err_t prefix##_query_menu(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_querymenu *qmenu) \
    { \
        esp_err_t ret; \
        esp_cam_sensor_param_desc_t qdesc = { \
            .id = map->esp_cam_priv_id, \
        }; \
        \
        if ((require_dev) && !cam->dev_member) { \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        ret = query_fn(cam->dev_member, &qdesc); \
        if (ret != ESP_OK) { \
            ESP_LOGE(TAG, "failed to query sensor id=%" PRIx32, qdesc.id); \
            return ret; \
        } \
        \
        if (qdesc.type != ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION) { \
            ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is not menu type", qmenu->id); \
            return ESP_ERR_INVALID_ARG; \
        } \
        \
        if (qmenu->index >= qdesc.enumeration.count) { \
            ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is out of range(max=%" PRIx32 ")", qmenu->id, qdesc.enumeration.count - 1); \
            return ESP_ERR_INVALID_ARG; \
        } \
        \
        qmenu->value = qdesc.enumeration.elements[qmenu->index]; \
        return ESP_OK; \
    } \
    \
    static const struct cam_ctrl_ops s_##prefix##_ops = { \
        .set = prefix##_set, \
        .get = prefix##_get, \
        .query = prefix##_query, \
        .query_menu = prefix##_query_menu, \
        .supported = prefix##_supported, \
    };

#define CAM_SENSOR_IOCTL_SUBSYS_DEFINE(map_table) \
    static const struct control_map s_sensor_ioctl_table[] = { \
        map_table \
    }; \
    \
    static esp_err_t sensor_ioctl_exec(esp_video_cam_t *cam, uint32_t cmd, void *arg, uint32_t v4l2_id, bool is_set) \
    { \
        esp_err_t ret = esp_cam_sensor_ioctl(cam->sensor, cmd, arg); \
        \
        if (ret != ESP_OK) { \
            if (is_set) { \
                ESP_LOGE(TAG, "failed to set ioctl id=%" PRIx32, v4l2_id); \
            } else { \
                ESP_LOGE(TAG, "failed to get ioctl id=%" PRIx32, v4l2_id); \
            } \
        } \
        return ret; \
    } \
    \
    static esp_err_t sensor_class_ioctl(esp_video_cam_t *cam, struct v4l2_ext_control *ctrl, bool is_set) \
    { \
        return sensor_ioctl_exec(cam, ctrl->id, ctrl->p_u8, ctrl->id, is_set); \
    } \
    \
    static esp_err_t sensor_ioctl_set(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl) \
    { \
        if (map->esp_cam_priv_id != ESP_CAM_SENSOR_IOC_S_TEST_PATTERN) { \
            ESP_LOGE(TAG, "sensor command=%" PRIu32 " is not supported", map->esp_cam_priv_id); \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        return sensor_ioctl_exec(cam, map->esp_cam_priv_id, &ctrl->value, ctrl->id, true); \
    } \
    \
    static esp_err_t sensor_ioctl_get(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl) \
    { \
        if (map->esp_cam_priv_id != ESP_CAM_SENSOR_IOC_S_TEST_PATTERN) { \
            ESP_LOGE(TAG, "sensor command=%" PRIu32 " is not supported", map->esp_cam_priv_id); \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        return sensor_ioctl_exec(cam, map->esp_cam_priv_id, &ctrl->value, ctrl->id, false); \
    } \
    \
    static esp_err_t sensor_ioctl_query(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_query_ext_ctrl *qctrl) \
    { \
        esp_err_t ret; \
        esp_cam_sensor_param_desc_t qdesc = { \
            .id = map->esp_cam_priv_id, \
        }; \
        \
        ret = esp_cam_sensor_query_para_desc(cam->sensor, &qdesc); \
        if (ret != ESP_OK) { \
            ESP_LOGD(TAG, "failed to query sensor id=%" PRIx32, qdesc.id); \
            return ret; \
        } \
        \
        return fill_query_ext_ctrl(qctrl, map->name, &qdesc); \
    } \
    \
    static esp_err_t sensor_ioctl_query_menu(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_querymenu *qmenu) \
    { \
        (void)cam; \
        (void)map; \
        \
        ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is ioctl type", qmenu->id); \
        return ESP_ERR_NOT_SUPPORTED; \
    } \
    \
    static const struct cam_ctrl_ops s_sensor_ioctl_ops = { \
        .set = sensor_ioctl_set, \
        .get = sensor_ioctl_get, \
        .query = sensor_ioctl_query, \
        .query_menu = sensor_ioctl_query_menu, \
        .supported = NULL, \
    };

#define CAM_IRCUT_SUBSYS_DEFINE(map_table) \
    static const struct control_map s_ircut_table[] = { \
        map_table \
    }; \
    \
    static bool ircut_supported(esp_video_cam_t *cam, const struct control_map *map) \
    { \
        (void)map; \
        \
        return cam->ircut != NULL; \
    } \
    \
    static esp_err_t ircut_set(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl) \
    { \
        esp_err_t ret; \
        \
        if (map->esp_cam_priv_id != ESP_CAM_IRCUT_IOC_SET_MODE) { \
            ESP_LOGE(TAG, "sensor command=%" PRIu32 " is not supported", map->esp_cam_priv_id); \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        /* UNKNOWN is a readable status. Writable values are the QUERYMENU items. */ \
        if (!ircut_mode_is_writable(ctrl->value)) { \
            ESP_LOGE(TAG, "IRCUT mode=%" PRIi32 " is not writable", ctrl->value); \
            return ESP_ERR_INVALID_ARG; \
        } \
        \
        ret = esp_cam_ircut_ioctl(cam->ircut, map->esp_cam_priv_id, &ctrl->value); \
        if (ret != ESP_OK) { \
            ESP_LOGE(TAG, "failed to set ioctl id=%" PRIx32, ctrl->id); \
        } \
        return ret; \
    } \
    \
    static esp_err_t ircut_get(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_ext_control *ctrl) \
    { \
        esp_err_t ret; \
        uint32_t cmd = map->esp_cam_priv_id; \
        \
        if (cmd != ESP_CAM_IRCUT_IOC_SET_MODE) { \
            ESP_LOGE(TAG, "sensor command=%" PRIu32 " is not supported", cmd); \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        /* V4L2_CID_IRCUT_MODE maps to SET_MODE. Read with GET_MODE so the current value is not applied. */ \
        cmd = ESP_CAM_IRCUT_IOC_GET_MODE; \
        ret = esp_cam_ircut_ioctl(cam->ircut, cmd, &ctrl->value); \
        if (ret != ESP_OK) { \
            ESP_LOGE(TAG, "failed to get ioctl id=%" PRIx32, ctrl->id); \
        } \
        return ret; \
    } \
    \
    static esp_err_t ircut_query(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_query_ext_ctrl *qctrl) \
    { \
        esp_err_t ret; \
        /* min..max includes UNKNOWN so a fresh GET stays in range. MENU items are the writable values. */ \
        esp_cam_sensor_param_desc_t qdesc = { \
            .type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER, \
            .number = { \
                .minimum = ESP_CAM_IRCUT_MODE_UNKNOWN, \
                .maximum = ESP_CAM_IRCUT_MODE_NIGHT, \
                .step = 1, \
            }, \
            .default_value = ESP_CAM_IRCUT_MODE_DAY, \
        }; \
        \
        if (!cam->ircut) { \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        ret = fill_query_ext_ctrl(qctrl, map->name, &qdesc); \
        if (ret != ESP_OK) { \
            return ret; \
        } \
        qctrl->type = V4L2_CTRL_TYPE_MENU; \
        return ESP_OK; \
    } \
    \
    static esp_err_t ircut_query_menu(esp_video_cam_t *cam, const struct control_map *map, struct v4l2_querymenu *qmenu) \
    { \
        const char *menu_name; \
        \
        (void)map; \
        \
        if (!cam->ircut) { \
            return ESP_ERR_NOT_SUPPORTED; \
        } \
        \
        menu_name = ircut_mode_menu_name(qmenu->index); \
        if (!menu_name) { \
            return ESP_ERR_INVALID_ARG; \
        } \
        \
        if (snprintf((char *)qmenu->name, sizeof(qmenu->name), "%s", menu_name) < 0) { \
            return ESP_FAIL; \
        } \
        return ESP_OK; \
    } \
    \
    static const struct cam_ctrl_ops s_ircut_ops = { \
        .set = ircut_set, \
        .get = ircut_get, \
        .query = ircut_query, \
        .query_menu = ircut_query_menu, \
        .supported = ircut_supported, \
    };

/**
 * Todo: AEG-1094
 *
 * @note The order of the control map table is important, because the control map table is used to
 *       get the control ID map pointer based on V4L2 control ID. Put the control ID that is most
 *       frequently used at the beginning of the table which can improve the performance of the
 *       control ID map pointer lookup.
 */
#define SENSOR_PARA_LOOKUP_TABLE \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_GAIN, V4L2_CID_GAIN, "Gain") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_EXPOSURE_VAL, V4L2_CID_EXPOSURE, "Exposure") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_STATS, V4L2_CID_CAMERA_STATS, "Sensor Statistics") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_AE_LEVEL, V4L2_CID_CAMERA_AE_LEVEL, "AE Target Level") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_GROUP_EXP_GAIN, V4L2_CID_CAMERA_GROUP, "Group Operation") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_EXPOSURE_US, V4L2_CID_EXPOSURE_ABSOLUTE, "Exposure Time, Absolute") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_JPEG_QUALITY, V4L2_CID_JPEG_COMPRESSION_QUALITY, "Compression Quality") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_3A_LOCK, V4L2_CID_3A_LOCK, "3A Lock") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_FLASH_LED, V4L2_CID_FLASH_LED_MODE, "LED Mode") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_VFLIP, V4L2_CID_VFLIP, "Vertical Flip") \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_HMIRROR, V4L2_CID_HFLIP, "Horizontal Flip")

CAM_PARA_SUBSYS_DEFINE(sensor_para, sensor, esp_cam_sensor_query_para_desc, esp_cam_sensor_get_para_value,
                       esp_cam_sensor_set_para_value, 0, SENSOR_PARA_LOOKUP_TABLE)

#define SENSOR_IOCTL_LOOKUP_TABLE \
    CAM_CTRL_MAP(ESP_CAM_SENSOR_IOC_S_TEST_PATTERN, V4L2_CID_TEST_PATTERN, "Test Pattern")

CAM_SENSOR_IOCTL_SUBSYS_DEFINE(SENSOR_IOCTL_LOOKUP_TABLE)

#if CONFIG_ESP_VIDEO_ENABLE_CAMERA_MOTOR_CONTROLLER
#define MOTOR_PARA_LOOKUP_TABLE \
    CAM_CTRL_MAP(ESP_CAM_MOTOR_POSITION_CODE, V4L2_CID_FOCUS_ABSOLUTE, "Focus, Absolute") \
    CAM_CTRL_MAP(ESP_CAM_MOTOR_MOVING_START_TIME, V4L2_CID_MOTOR_START_TIME, "Motor Start Time")

CAM_PARA_SUBSYS_DEFINE(motor_para, motor, esp_cam_motor_query_para_desc, esp_cam_motor_get_para_value,
                       esp_cam_motor_set_para_value, 1, MOTOR_PARA_LOOKUP_TABLE)

#define MOTOR_SUBSYS_ENTRY CAM_SUBSYS_ENTRY(motor_para, false)
#else
#define MOTOR_SUBSYS_ENTRY
#endif

#if CONFIG_ESP_VIDEO_ENABLE_IRCUT_SUPPORT
static bool ircut_mode_is_writable(int32_t mode)
{
    return mode == ESP_CAM_IRCUT_MODE_DAY || mode == ESP_CAM_IRCUT_MODE_NIGHT;
}

/* UNKNOWN is readable, not a menu item. Indices outside DAY..NIGHT are absent. */
static const char *ircut_mode_menu_name(uint32_t index)
{
    switch (index) {
    case ESP_CAM_IRCUT_MODE_DAY:
        return "Day";
    case ESP_CAM_IRCUT_MODE_NIGHT:
        return "Night";
    default:
        return NULL;
    }
}

#define IRCUT_LOOKUP_TABLE \
    CAM_CTRL_MAP(ESP_CAM_IRCUT_IOC_SET_MODE, V4L2_CID_IRCUT_MODE, "IRCUT Mode")

CAM_IRCUT_SUBSYS_DEFINE(IRCUT_LOOKUP_TABLE)

#define IRCUT_SUBSYS_ENTRY CAM_SUBSYS_ENTRY(ircut, true)
#else
#define IRCUT_SUBSYS_ENTRY
#endif

static const struct cam_ctrl_subsys s_cam_subsys[] = {
    CAM_SUBSYS_ENTRY(sensor_para, true)
    MOTOR_SUBSYS_ENTRY
    IRCUT_SUBSYS_ENTRY
    CAM_SUBSYS_ENTRY(sensor_ioctl, false)
};

static const struct cam_ctrl_subsys *cam_ctrl_lookup(uint32_t v4l2_id, const struct control_map **map)
{
    for (int s = 0; s < ARRAY_SIZE(s_cam_subsys); s++) {
        const struct cam_ctrl_subsys *subsys = &s_cam_subsys[s];

        for (int i = 0; i < subsys->size; i++) {
            if (subsys->table[i].v4l2_id == v4l2_id) {
                *map = &subsys->table[i];
                return subsys;
            }
        }
    }

    return NULL;
}

/**
 * @brief Find the next supported control, walking enumerable subsystem tables in order
 *
 * @param cam Camera device pointer
 * @param id  In: previous control ID, 0 starts from the first control. Out: next control ID
 *
 * @return true if a supported control was found
 */
static bool find_next_supported_control(esp_video_cam_t *cam, uint32_t *id)
{
    bool matched = (*id == 0);

    for (int s = 0; s < ARRAY_SIZE(s_cam_subsys); s++) {
        const struct cam_ctrl_subsys *subsys = &s_cam_subsys[s];

        if (!subsys->enumerable) {
            continue;
        }

        for (int i = 0; i < subsys->size; i++) {
            if (matched) {
                if (subsys->ops->supported(cam, &subsys->table[i])) {
                    *id = subsys->table[i].v4l2_id;
                    return true;
                }
            } else if (subsys->table[i].v4l2_id == *id) {
                matched = true;
            }
        }
    }

    return false;
}

/**
 * @brief Set control value to camera device
 *
 * @param cam      Camera device pointer
 * @param controls V4L2 external controls pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_cam_set_ext_ctrls(esp_video_cam_t *cam, const struct v4l2_ext_controls *controls)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    for (int i = 0; i < controls->count; i++) {
        const struct control_map *map;
        const struct cam_ctrl_subsys *subsys;
        struct v4l2_ext_control *ctrl = &controls->controls[i];

        if (controls->ctrl_class == V4L2_CTRL_CLASS_ESP_CAM_IOCTL) {
            ret = sensor_class_ioctl(cam, ctrl, true);
        } else {
            subsys = cam_ctrl_lookup(ctrl->id, &map);
            if (!subsys) {
                ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is not supported", ctrl->id);
                ret = ESP_ERR_NOT_SUPPORTED;
                break;
            }

            ret = subsys->ops->set(cam, map, ctrl);
        }
        if (ret != ESP_OK) {
            break;
        }
    }

    return ret;
}

/**
 * @brief Get control value from camera device
 *
 * @param cam      Camera device pointer
 * @param controls V4L2 external controls pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_cam_get_ext_ctrls(esp_video_cam_t *cam, struct v4l2_ext_controls *controls)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    for (int i = 0; i < controls->count; i++) {
        const struct control_map *map;
        const struct cam_ctrl_subsys *subsys;
        struct v4l2_ext_control *ctrl = &controls->controls[i];

        if (controls->ctrl_class == V4L2_CTRL_CLASS_ESP_CAM_IOCTL) {
            ret = sensor_class_ioctl(cam, ctrl, false);
        } else {
            subsys = cam_ctrl_lookup(ctrl->id, &map);
            if (!subsys) {
                ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is not supported", ctrl->id);
                ret = ESP_ERR_NOT_SUPPORTED;
                break;
            }

            ret = subsys->ops->get(cam, map, ctrl);
        }
        if (ret != ESP_OK) {
            break;
        }
    }

    return ret;
}

/**
 * @brief Get control description from camera device
 *
 * @param cam      Camera device pointer
 * @param qctrl    V4L2 external controls query description pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_cam_query_ext_ctrls(esp_video_cam_t *cam, struct v4l2_query_ext_ctrl *qctrl)
{
    const struct control_map *map;
    const struct cam_ctrl_subsys *subsys;

    if (qctrl->id & V4L2_CTRL_FLAG_NEXT_CTRL) {
        uint32_t id;

        /* Remove next flag. id 0 starts at the first supported control. */
        qctrl->id &= ~V4L2_CTRL_FLAG_NEXT_CTRL;
        id = qctrl->id;
        if (!find_next_supported_control(cam, &id)) {
            return ESP_ERR_INVALID_ARG;
        }

        qctrl->id = id;
    }

    subsys = cam_ctrl_lookup(qctrl->id, &map);
    if (!subsys) {
        ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is not supported", qctrl->id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return subsys->ops->query(cam, map, qctrl);
}

/**
 * @brief Query menu value from camera device
 *
 * @param cam      Camera device pointer
 * @param qmenu    Menu value buffer pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_cam_query_menu(esp_video_cam_t *cam, struct v4l2_querymenu *qmenu)
{
    const struct control_map *map;
    const struct cam_ctrl_subsys *subsys;

    subsys = cam_ctrl_lookup(qmenu->id, &map);
    if (!subsys) {
        ESP_LOGE(TAG, "ctrl id=%" PRIx32 " is not supported", qmenu->id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return subsys->ops->query_menu(cam, map, qmenu);
}
