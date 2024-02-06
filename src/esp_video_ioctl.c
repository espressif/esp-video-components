/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "esp_heap_caps.h"
#include "esp_video.h"
#include "esp_video_log.h"
#include "esp_video_bsp.h"
#include "esp_video_vfs.h"
#include "esp_video_ioctl.h"
#include "esp_color_formats.h"

#define BUF_OFF(type, element_index)        (((uint32_t)type << 24) + element_index)
#define BUF_OFF_2_INDEX(buf_off)            ((buf_off) & 0x00ffffff)
#define BUF_OFF_2_TYPE(buf_off)             ((buf_off) >> 24)

/**
 * Todo: AEG-1094
 */
static const int s_control_id_map_table[][2] = {
    { V4L2_CID_3A_LOCK, CAM_SENSOR_3A_LOCK },
    { V4L2_CID_FLASH_LED_MODE, CAM_SENSOR_FLASH_LED }
};

static esp_err_t v4l2_ext_control_id_map(uint32_t *id)
{
    for (int i = 0; i < ARRAY_SIZE(s_control_id_map_table); i++) {
        if (s_control_id_map_table[i][0] == *id) {
            *id = s_control_id_map_table[i][1];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

static esp_err_t esp_video_ioctl_querycap(struct esp_video *video, struct v4l2_capability *cap)
{
    memset(cap, 0, sizeof(struct v4l2_capability));

    strlcpy((char *)cap->driver, video->dev_name, sizeof(cap->driver));
    strlcpy((char *)cap->card, video->dev_name, sizeof(cap->driver));
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

static esp_err_t esp_video_ioctl_s_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    esp_err_t ret;
    struct esp_video_format format;

    memset(&format, 0, sizeof(struct esp_video_format));
    format.bpp = esp_video_get_bpp_by_format(fmt->fmt.pix.pixelformat);
    if (!format.bpp) {
        return ESP_ERR_INVALID_ARG;
    }
    format.width        = fmt->fmt.pix.width;
    format.height       = fmt->fmt.pix.height;
    format.pixel_format = fmt->fmt.pix.pixelformat;
    ret = esp_video_set_format(video, fmt->type, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    return ret;
}

static esp_err_t esp_video_ioctl_try_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    return esp_video_ioctl_s_fmt(video, fmt);
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

static esp_err_t esp_video_ioctl_s_param(struct esp_video *video, struct v4l2_streamparm *param)
{
    esp_err_t ret;
    struct esp_video_format format;

    if (param->parm.capture.timeperframe.numerator != 1) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_format(video, param->type, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    format.fps = param->parm.capture.timeperframe.denominator;
    ret = esp_video_set_format(video, param->type, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    return ret;
}

static esp_err_t esp_video_ioctl_op_ext_ctrls(struct esp_video *video, struct v4l2_ext_controls *controls, bool set)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    esp_camera_device_t *cam_dev = video->cam_dev;
    //ToDo:
    for (int i = 0; i < controls->count; i++) {
        struct v4l2_ext_control *ctrl = &controls->controls[i];

        if (controls->ctrl_class == V4L2_CTRL_CLASS_PRIV) {
            if (set) {
                ret = esp_camera_set_para_value(cam_dev, ctrl);
            } else {
                ret = esp_camera_get_para_value(cam_dev, ctrl);
            }
        } else {
            uint32_t ctrl_id = ctrl->id;
            uint32_t new_id = ctrl_id;

            ret = v4l2_ext_control_id_map(&new_id);
            if (ret != ESP_OK) {
                break;
            }

            ctrl->id = new_id;
            if (set) {
                ret = esp_camera_set_para_value(cam_dev, ctrl);
            } else {
                ret = esp_camera_get_para_value(cam_dev, ctrl);
            }
            ctrl->id = ctrl_id;

            if (ret != ESP_OK) {
                break;
            }
        }
    }

    return ret;
}

/**
 * Todo: AEG-1095
 */
static esp_err_t esp_video_ioctl_query_ext_ctrls(struct esp_video *video, struct v4l2_query_ext_ctrl *qc)
{
    esp_err_t ret;
    esp_camera_device_t *cam_dev = video->cam_dev;
    //ToDo:
    if (V4L2_CTRL_ID2CLASS(qc->id) == V4L2_CTRL_CLASS_PRIV) {
        ret = esp_camera_query_para_desc(cam_dev, qc);
    } else {
        uint32_t qc_id = qc->id;
        uint32_t new_id = qc_id;

        ret = v4l2_ext_control_id_map(&new_id);
        if (ret != ESP_OK) {
            goto exit;
        }

        qc->id = new_id;
        ret = esp_camera_query_para_desc(cam_dev, qc);
        qc->id = qc_id;

        if (ret != ESP_OK) {
            goto exit;
        }
    }

exit:

    return ret;
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
    case VIDIOC_G_FMT:
        ret = esp_video_ioctl_g_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_S_FMT:
        ret = esp_video_ioctl_s_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_TRY_FMT:
        ret = esp_video_ioctl_try_fmt(video, (struct v4l2_format *)arg_ptr);
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
    case VIDIOC_S_PARM:
        ret = esp_video_ioctl_s_param(video, (struct v4l2_streamparm *)arg_ptr);
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
    case VIDIOC_ENUM_FMT:
    case VIDIOC_ENUM_FRAMESIZES:
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}
