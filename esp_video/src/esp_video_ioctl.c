/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "esp_heap_caps.h"
#include "esp_video.h"
#include "esp_video_vfs.h"
#include "esp_video_ioctl.h"

#define BUF_OFF(type, element_index)        (((uint32_t)type << 24) + element_index)
#define BUF_OFF_2_INDEX(buf_off)            ((buf_off) & 0x00ffffff)
#define BUF_OFF_2_TYPE(buf_off)             ((buf_off) >> 24)

struct control_map {
    uint32_t esp_cam_sensor_id;
    uint32_t v4l2_id;
};

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
};

static const struct control_map *get_v4l2_ext_control_map(uint32_t v4l2_id)
{
    for (int i = 0; i < ARRAY_SIZE(s_control_map_table); i++) {
        if (s_control_map_table[i].v4l2_id == v4l2_id) {
            return &s_control_map_table[i];
        }
    }

    return NULL;
}

static esp_err_t esp_video_ioctl_querycap(struct esp_video *video, struct v4l2_capability *cap)
{
    memset(cap, 0, sizeof(struct v4l2_capability));

    snprintf((char *)cap->driver, sizeof(cap->driver), "%s", video->dev_name);
    snprintf((char *)cap->card, sizeof(cap->card), "%s", video->dev_name);
    snprintf((char *)cap->bus_info, sizeof(cap->bus_info), "%s:%s", CONFIG_IDF_TARGET, video->dev_name);
    cap->version = (ESP_VIDEO_VER_MAJOR << 16) | (ESP_VIDEO_VER_MINOR << 8) | ESP_VIDEO_VER_PATCH;
    cap->capabilities = video->caps;
    if (video->caps & V4L2_CAP_DEVICE_CAPS) {
        cap->device_caps = video->device_caps;
    }

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_g_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    esp_err_t ret;
    struct esp_video_format format;

    ret = esp_video_get_format(video, fmt->type, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    memset(&fmt->fmt, 0, sizeof(fmt->fmt));
    fmt->fmt.pix.width       = format.width;
    fmt->fmt.pix.height      = format.height;
    fmt->fmt.pix.pixelformat = format.pixel_format;

    return ret;
}

static esp_err_t esp_video_ioctl_enum_fmt(struct esp_video *video, struct v4l2_fmtdesc *fmt)
{
    esp_err_t ret;
    struct esp_video_format_desc desc;

    ret = esp_video_enum_format(video, fmt->type, fmt->index, &desc);
    if (ret == ESP_OK) {
        fmt->flags = 0;
        fmt->mbus_code = 0;
        fmt->pixelformat = desc.pixel_format;
        memcpy(fmt->description, desc.description, sizeof(fmt->description));
    }

    return ret;
}

static esp_err_t esp_video_ioctl_s_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    esp_err_t ret;
    struct esp_video_format format;

    memset(&format, 0, sizeof(struct esp_video_format));
    format.width        = fmt->fmt.pix.width;
    format.height       = fmt->fmt.pix.height;
    format.pixel_format = fmt->fmt.pix.pixelformat;
    ret = esp_video_set_format(video, fmt->type, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    return ret;
}

static esp_err_t esp_video_ioctl_streamon(struct esp_video *video, int *arg)
{
    esp_err_t ret;
    enum v4l2_buf_type type = *(enum v4l2_buf_type *)arg;

    ret = esp_video_start_capture(video, type);

    return ret;
}

static esp_err_t esp_video_ioctl_streamoff(struct esp_video *video, int *arg)
{
    esp_err_t ret;
    enum v4l2_buf_type type = *(enum v4l2_buf_type *)arg;

    ret = esp_video_stop_capture(video, type);

    return ret;
}

static esp_err_t esp_video_ioctl_reqbufs(struct esp_video *video, struct v4l2_requestbuffers *req_bufs)
{
    esp_err_t ret;

    /* Only support memory buffer MMAP mode */

    if (req_bufs->memory != V4L2_MEMORY_MMAP) {
        return ESP_ERR_INVALID_ARG;
    }

    if (req_bufs->count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_setup_buffer(video, req_bufs->type, req_bufs->count);

    return ret;
}

static esp_err_t esp_video_ioctl_querybuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    struct esp_video_buffer_info info;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_buffer_info(video, vbuf->type, &info);
    if (ret != ESP_OK) {
        return ret;
    }

    if (vbuf->index >= info.count) {
        return ESP_ERR_INVALID_ARG;
    }

    /* offset contains of stream ID and buffer index  */

    vbuf->length = info.size;
    vbuf->m.offset = BUF_OFF(vbuf->type, vbuf->index);

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_mmap(struct esp_video *video, struct esp_video_ioctl_mmap *ioctl_mmap)
{
    esp_err_t ret;
    struct esp_video_buffer_info info;
    uint8_t type = BUF_OFF_2_TYPE(ioctl_mmap->offset);
    int index = BUF_OFF_2_INDEX(ioctl_mmap->offset);

    ret = esp_video_get_buffer_info(video, type, &info);
    if (ret != ESP_OK) {
        return ret;
    }

    if ((ioctl_mmap->length > info.size) || (index >= info.count)) {
        return ESP_ERR_INVALID_ARG;
    }

    ioctl_mmap->mapped_ptr = esp_video_get_element_index_payload(video, type, index);

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_qbuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    struct esp_video_buffer_info info;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_buffer_info(video, vbuf->type, &info);
    if (ret != ESP_OK) {
        return ret;
    }

    if (vbuf->index >= info.count) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_queue_element_index(video, vbuf->type, vbuf->index);

    return ret;
}

static esp_err_t esp_video_ioctl_dqbuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    uint32_t ticks = portMAX_DELAY;
    struct esp_video_buffer_element *element;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        return ESP_ERR_INVALID_ARG;
    }

    element = esp_video_recv_element(video, vbuf->type, ticks);
    if (!element) {
        return ESP_FAIL;
    }

    vbuf->index     = element->index;
    vbuf->bytesused = element->valid_size;

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_op_ext_ctrls(struct esp_video *video, struct v4l2_ext_controls *controls, bool set)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    esp_cam_sensor_device_t *cam_dev = video->cam_dev;

    for (int i = 0; i < controls->count; i++) {
        const struct control_map *control_map;
        struct v4l2_ext_control *ctrl = &controls->controls[i];

        control_map = get_v4l2_ext_control_map(ctrl->id);
        if (!control_map) {
            ret = ESP_ERR_NOT_SUPPORTED;
            break;
        }

        if (set) {
            ret = esp_cam_sensor_set_para_value(cam_dev, control_map->esp_cam_sensor_id, &ctrl->value, sizeof(ctrl->value));
        } else {
            ret = esp_cam_sensor_get_para_value(cam_dev, control_map->esp_cam_sensor_id, &ctrl->value, sizeof(ctrl->value));
        }

        if (ret != ESP_OK) {
            break;
        }
    }

    return ret;
}

/**
 * Todo: AEG-1095
 */
static esp_err_t esp_video_ioctl_query_ext_ctrls(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    esp_err_t ret = 0;
    const struct control_map *control_map;
    esp_cam_sensor_param_desc_t qdesc;
    esp_cam_sensor_device_t *cam_dev = video->cam_dev;

    control_map = get_v4l2_ext_control_map(qctrl->id);
    if (!control_map) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    qdesc.id = control_map->esp_cam_sensor_id;
    ret = esp_cam_sensor_query_para_desc(cam_dev, &qdesc);
    if (ret != ESP_OK) {
        return ret;
    }

    switch (qdesc.type) {
    case ESP_CAM_SENSOR_PARAM_TYPE_NUMBER:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->maximum = qdesc.number.minimum;
        qctrl->minimum = qdesc.number.maximum;
        qctrl->step = qdesc.number.step;;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = qdesc.default_value;
        break;
    case ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION:
        qctrl->type = V4L2_CTRL_TYPE_MENU;
        qctrl->elem_size = sizeof(uint32_t);
        qctrl->elems = qdesc.enumeration.count;
        qctrl->nr_of_dims = qdesc.enumeration.count;
        for (int i = 0; i < MIN(qctrl->nr_of_dims, V4L2_CTRL_MAX_DIMS); i++) {
            qctrl->dims[i] = qdesc.enumeration.elements[i];
        }
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
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

esp_err_t esp_video_ioctl(struct esp_video *video, int cmd, va_list args)
{
    esp_err_t ret = ESP_OK;
    void *arg_ptr;

    assert(video);

    arg_ptr = va_arg(args, void *);
    if (!arg_ptr) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (cmd) {
    case VIDIOC_QBUF:
        ret = esp_video_ioctl_qbuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_DQBUF:
        ret = esp_video_ioctl_dqbuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_QUERYCAP:
        ret = esp_video_ioctl_querycap(video, (struct v4l2_capability *)arg_ptr);
        break;
    case VIDIOC_ENUM_FMT:
        ret = esp_video_ioctl_enum_fmt(video, (struct v4l2_fmtdesc *)arg_ptr);
        break;
    case VIDIOC_G_FMT:
        ret = esp_video_ioctl_g_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_S_FMT:
        ret = esp_video_ioctl_s_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_STREAMON:
        ret = esp_video_ioctl_streamon(video, (int *)arg_ptr);
        break;
    case VIDIOC_STREAMOFF:
        ret = esp_video_ioctl_streamoff(video, (int *)arg_ptr);
        break;
    case VIDIOC_REQBUFS:
        ret = esp_video_ioctl_reqbufs(video, (struct v4l2_requestbuffers *)arg_ptr);
        break;
    case VIDIOC_QUERYBUF:
        ret = esp_video_ioctl_querybuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_MMAP:
        ret = esp_video_ioctl_mmap(video, (struct esp_video_ioctl_mmap *)arg_ptr);
        break;
    case VIDIOC_G_EXT_CTRLS:
        ret = esp_video_ioctl_op_ext_ctrls(video, (struct v4l2_ext_controls *)arg_ptr, false);
        break;
    case VIDIOC_S_EXT_CTRLS:
        ret = esp_video_ioctl_op_ext_ctrls(video, (struct v4l2_ext_controls *)arg_ptr, true);
        break;
    case VIDIOC_QUERY_EXT_CTRL:
        ret = esp_video_ioctl_query_ext_ctrls(video, (struct v4l2_query_ext_ctrl *)arg_ptr);
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}
