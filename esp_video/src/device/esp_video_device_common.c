/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_check.h"

#include "esp_private/esp_cache_private.h"
#include "esp_video.h"
#include "esp_video_ioctl.h"
#include "esp_video_internal.h"
#include "esp_video_caps.h"
#include "esp_video_device_common.h"

static const char *TAG = "video_common";

static esp_err_t esp_video_device_common_get_input_frame_type(esp_cam_sensor_output_format_t sensor_format,
        cam_ctlr_color_t *in_color, uint32_t *v4l2_format)
{
    switch (sensor_format) {
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE:
        *in_color = CAM_CTLR_COLOR_RGB565;
        *v4l2_format = V4L2_PIX_FMT_RGB565;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565_BE:
        *in_color = CAM_CTLR_COLOR_RGB565;
        *v4l2_format = V4L2_PIX_FMT_RGB565X;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        *in_color = CAM_CTLR_COLOR_YUV422_UYVY;
#else
        *in_color = CAM_CTLR_COLOR_YUV422;
#endif
        *v4l2_format = V4L2_PIX_FMT_UYVY;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        *in_color = CAM_CTLR_COLOR_YUV422_YUYV;
#else
        *in_color = CAM_CTLR_COLOR_YUV422;
#endif
        *v4l2_format = V4L2_PIX_FMT_YUYV;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB888:
        *in_color = CAM_CTLR_COLOR_RGB888;
        *v4l2_format = V4L2_PIX_FMT_RGB24;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_JPEG:
        *in_color = 0;
        *v4l2_format = V4L2_PIX_FMT_JPEG;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        *in_color = CAM_CTLR_COLOR_RAW8;
        *v4l2_format = V4L2_PIX_FMT_SBGGR8;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        *in_color = CAM_CTLR_COLOR_RAW10;
        *v4l2_format = V4L2_PIX_FMT_SBGGR10;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        *in_color = CAM_CTLR_COLOR_RAW12;
        *v4l2_format = V4L2_PIX_FMT_SBGGR12;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV420:
        *in_color = CAM_CTLR_COLOR_YUV420;
        *v4l2_format = V4L2_PIX_FMT_YUV420;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE:
        *in_color = CAM_CTLR_COLOR_GRAY8;
        *v4l2_format = V4L2_PIX_FMT_GREY;
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t update_format_config(esp_video_device_common_t *common, const esp_cam_sensor_format_t *set_fmt)
{
    uint32_t v4l2_fmt = 0;
    struct esp_video *video = common->video;
    const esp_cam_sensor_format_t *sensor_fmt = set_fmt ? set_fmt : common->sensor_format;

    common->in_color = 0;

    esp_video_device_common_init_data_t config = {0};
    if (common->intf->start_init_config) {
        ESP_RETURN_ON_ERROR(common->intf->start_init_config(common, &config), TAG, "start_init_config failed");
    }

    if (config.v4l2_format) {
        v4l2_fmt = config.v4l2_format;
    } else {
        ESP_RETURN_ON_ERROR(esp_video_device_common_get_input_frame_type(sensor_fmt->format, &common->in_color, &v4l2_fmt),
                            TAG, "failed to convert sensor format");
    }

    struct v4l2_format format = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .fmt.pix = {
            .width = sensor_fmt->width,
            .height = sensor_fmt->height,
            .pixelformat = v4l2_fmt,
            .sizeimage = config.sizeimage,
        },
    };
    ESP_RETURN_ON_ERROR(esp_video_config_buffer(video, &format, common->mem_caps), TAG, "failed to configure stream buffer");

    return ESP_OK;
}

bool IRAM_ATTR esp_video_device_common_on_trans_finished(esp_cam_ctlr_handle_t handle,
        esp_cam_ctlr_trans_t *trans, void *user_data)
{
    bool need_yield = false;
    struct esp_video *video = (struct esp_video *)user_data;
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);
    struct esp_video_param *param = CAPTURE_VIDEO_PARAM(video);

    if (common->use_backup_element) {
        if (common->backup_element && (trans->buffer == common->backup_element->buffer)) {
            return false;
        }
    }

    if (!param->skip_count) {
        CAPTURE_VIDEO_DONE_BUF(video, trans->buffer, trans->received_size);
        need_yield = true;
    } else {
        CAPTURE_VIDEO_SKIP_BUF(video, trans->buffer);
    }

    if (param->skip_frames) {
        param->skip_count = (param->skip_count + 1) % param->skip_frames;
    }

    return need_yield;
}

bool IRAM_ATTR esp_video_device_common_on_get_new_trans(esp_cam_ctlr_handle_t handle,
        esp_cam_ctlr_trans_t *trans, void *user_data)
{
    bool ret = false;
    struct esp_video_buffer_element *element;
    struct esp_video *video = (struct esp_video *)user_data;
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    if (common->intf->prepare_on_get_new_trans) {
        common->intf->prepare_on_get_new_trans(common);
    }

    element = CAPTURE_VIDEO_GET_QUEUED_ELEMENT(video);
    if (!element) {
        if (common->use_backup_element && common->backup_element) {
            element = common->backup_element;
        }
    }

    if (element) {
        common->backup_element = element;
        trans->buffer = element->buffer;
        trans->buflen = ELEMENT_SIZE(element);
        ret = true;
    }

    return ret;
}

/**
 * Query extended control information for ISP video device
 *
 * This function implements the VIDIOC_QUERY_EXT_CTRL ioctl handler for V4L2.
 * It supports both direct ID lookup and NEXT_CTRL iteration.
 *
 * @param qctrl_table Pointer to v4l2_query_ext_ctrl table
 * @param qctrl_table_size Size of v4l2_query_ext_ctrl table
 * @param qctrl Pointer to v4l2_query_ext_ctrl structure (in/out parameter)
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t esp_video_device_common_query_ext_ctrl(const struct v4l2_query_ext_ctrl *qctrl_table, int qctrl_table_size, struct v4l2_query_ext_ctrl *qctrl)
{
    uint32_t id;
    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;
    bool is_next_ctrl = false;
    int found_idx = -1;
    uint32_t smallest_larger_id = UINT32_MAX;
    int smallest_larger_idx = -1;

    /* Validate input parameters */
    if (!qctrl_table || !qctrl_table_size || !qctrl) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Handle V4L2_CTRL_FLAG_NEXT_CTRL flag */
    id = qctrl->id;
    if (id & V4L2_CTRL_FLAG_NEXT_CTRL) {
        is_next_ctrl = true;
        id &= ~V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    if (is_next_ctrl) {
        /*
         * Find the control with the smallest ID that is greater than the input ID
         * This implements the V4L2 standard behavior for VIDIOC_QUERY_EXT_CTRL
         */
        for (int i = 0; i < qctrl_table_size; i++) {
            /* Check if current control ID is greater than the input ID */
            if (qctrl_table[i].id > id) {
                /* Track the smallest ID that is greater than input ID */
                if (qctrl_table[i].id < smallest_larger_id) {
                    smallest_larger_id = qctrl_table[i].id;
                    smallest_larger_idx = i;
                }
            }
        }

        /* Return the found control or error if none found */
        if (smallest_larger_idx >= 0) {
            found_idx = smallest_larger_idx;
        } else {
            /* No control with ID greater than the input ID */
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        /* Direct lookup: find control with exact ID match */
        for (int i = 0; i < qctrl_table_size; i++) {
            if (id == qctrl_table[i].id) {
                found_idx = i;
                break;
            }
        }
    }

    /* Copy control information to output structure if found */
    if (found_idx >= 0) {
        /*
         * Use memcpy for efficiency. This is safe because:
         * 1. qctrl_table is a static array that should be fully initialized
         * 2. The structure sizes are guaranteed to match
         * 3. This avoids missing any fields during manual assignment
         */
        memcpy(qctrl, &qctrl_table[found_idx], sizeof(struct v4l2_query_ext_ctrl));
        ret = ESP_OK;
    }

    return ret;
}

/* ------------------------------------------------------------------ */
/*  Common esp_video_ops — dispatch to device hooks or use defaults    */
/* ------------------------------------------------------------------ */

static esp_err_t common_video_init(struct esp_video *video)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    if (common->intf->init) {
        ESP_RETURN_ON_ERROR(common->intf->init(common), TAG, "device init failed");
    }

    esp_err_t ret = update_format_config(common, NULL);
    if (ret != ESP_OK) {
        if (common->intf->init && common->intf->deinit) {
            common->intf->deinit(common);
        }
        return ret;
    }

    return ESP_OK;
}

static esp_err_t common_video_deinit(struct esp_video *video)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    if (common->intf->deinit) {
        return common->intf->deinit(common);
    }

    return ESP_OK;
}

static esp_err_t common_video_start(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    assert(common->intf->start); // stop is not required

    if (common->use_backup_element) {
        uint32_t buffer_count = CAPTURE_VIDEO_BUF_COUNT(video);
        if (buffer_count <= 1) {
            ESP_LOGE(TAG, "buffer count=%" PRIu32 " is invalid, it should be greater than 1", buffer_count);
            return ESP_ERR_INVALID_ARG;
        }
    }

    ESP_RETURN_ON_ERROR(common->intf->start(common, &common->cam_ctrl_handle), TAG, "device start failed");

    esp_cam_ctlr_evt_cbs_t cam_ctrl_cbs = {
        .on_get_new_trans = esp_video_device_common_on_get_new_trans,
        .on_trans_finished = esp_video_device_common_on_trans_finished,
    };
    ESP_GOTO_ON_ERROR(esp_cam_ctlr_register_event_callbacks(common->cam_ctrl_handle, &cam_ctrl_cbs, video), fail_0, TAG, "failed to register CAM ctlr event callback");
    ESP_GOTO_ON_ERROR(esp_cam_ctlr_enable(common->cam_ctrl_handle), fail_0, TAG, "failed to enable CAM ctlr");
    ESP_GOTO_ON_ERROR(esp_cam_ctlr_start(common->cam_ctrl_handle), fail_1, TAG, "failed to start CAM ctlr");

    int flags = 1;
    ESP_GOTO_ON_ERROR(esp_cam_sensor_ioctl(common->cam.sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &flags), fail_2, TAG, "failed to start sensor stream");

    return ESP_OK;

fail_2:
    esp_cam_ctlr_stop(common->cam_ctrl_handle);
fail_1:
    esp_cam_ctlr_disable(common->cam_ctrl_handle);
fail_0:
    if (common->intf->stop) {
        common->intf->stop(common);
    }
    esp_cam_ctlr_del(common->cam_ctrl_handle);
    return ret;
}

static esp_err_t common_video_stop(struct esp_video *video, uint32_t type)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    assert(common->cam.sensor);

    int flags = 0;
    ESP_RETURN_ON_ERROR(esp_cam_sensor_ioctl(common->cam.sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &flags),
                        TAG, "failed to stop sensor stream");

    if (common->intf->stop) {
        ESP_RETURN_ON_ERROR(common->intf->stop(common), TAG, "device stop failed");
    }
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_stop(common->cam_ctrl_handle), TAG, "failed to stop CAM ctlr");
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_disable(common->cam_ctrl_handle), TAG, "failed to disable CAM ctlr");
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_del(common->cam_ctrl_handle), TAG, "failed to delete CAM ctlr");
    common->cam_ctrl_handle = NULL;

    return ESP_OK;
}

static esp_err_t common_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (common->intf->enum_format) {
        return common->intf->enum_format(common, index, pixel_format);
    }

    if (index >= 1) {
        return ESP_ERR_INVALID_ARG;
    }

    *pixel_format = CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(video);
    return ESP_OK;
}

static esp_err_t common_video_set_format(struct esp_video *video, const struct v4l2_format *format)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);
    const struct v4l2_pix_format *pix = &format->fmt.pix;

    if (pix->width != CAPTURE_VIDEO_GET_FORMAT_WIDTH(video) ||
            pix->height != CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video)) {
        ESP_LOGE(TAG, "input width=%" PRIu32 ", height=%" PRIu32 " is not supported", pix->width, pix->height);
        return ESP_ERR_INVALID_ARG;
    }

    if (common->intf->check_set_format) {
        ESP_RETURN_ON_ERROR(common->intf->check_set_format(common, format), TAG, "format check failed");
    } else {
        if (pix->pixelformat != CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(video)) {
            ESP_LOGE(TAG, "format=" V4L2_FMT_STR " is not supported", V4L2_FMT_STR_ARG(pix->pixelformat));
            return ESP_ERR_INVALID_ARG;
        }
    }

    CAPTURE_VIDEO_SET_FORMAT(video, pix->width, pix->height, pix->pixelformat);

    return ESP_OK;
}

static esp_err_t common_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    if (common->intf->reprocess) {
        if (event == ESP_VIDEO_DATA_PREPROCESSING) {
            struct esp_video_buffer_element *element = CAPTURE_VIDEO_GET_FIRST_DONE_ELEMENT_PTR(common->video);
            if (element) {
                size_t ret_size;
                esp_err_t ret = common->intf->reprocess(common, element->buffer, element->valid_size,
                                                        element->buffer, CAPTURE_VIDEO_BUF_SIZE(common->video), &ret_size);
                if (ret == ESP_OK) {
                    element->valid_size = ret_size;
                } else {
                    element->valid_size = 0;
                }
            }
        }
    }

    return ESP_OK;
}

static esp_err_t common_video_set_ext_ctrl(struct esp_video *video, const struct v4l2_ext_controls *ctrls)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    return esp_video_cam_set_ext_ctrls(&common->cam, ctrls);
}

static esp_err_t common_video_get_ext_ctrl(struct esp_video *video, struct v4l2_ext_controls *ctrls)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    return esp_video_cam_get_ext_ctrls(&common->cam, ctrls);
}

static esp_err_t common_video_query_ext_ctrl(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    return esp_video_cam_query_ext_ctrls(&common->cam, qctrl);
}

static esp_err_t common_video_set_sensor_format(struct esp_video *video, const esp_cam_sensor_format_t *format)
{
    esp_err_t ret = ESP_OK;
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    ESP_RETURN_ON_ERROR(esp_cam_sensor_set_format(common->cam.sensor, format), TAG, "failed to set sensor format");
    ESP_RETURN_ON_ERROR(update_format_config(common, format), TAG, "failed to initialize sensor format");
    common->sensor_format = common->cam.sensor->cur_format;

    return ret;
}

static esp_err_t common_video_get_sensor_format(struct esp_video *video, esp_cam_sensor_format_t *format)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    assert(format);

    return esp_cam_sensor_get_format(common->cam.sensor, format);
}

static esp_err_t common_video_query_menu(struct esp_video *video, struct v4l2_querymenu *qmenu)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    return esp_video_cam_query_menu(&common->cam, qmenu);
}

static esp_err_t common_video_set_motor_format(struct esp_video *video, const esp_cam_motor_format_t *format)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    /**
     * video device can not support motor but camera sensor is required
     */
    if (!common->cam.motor) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return esp_cam_motor_set_format(common->cam.motor, format);
}

static esp_err_t common_video_get_motor_format(struct esp_video *video, esp_cam_motor_format_t *format)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    if (!common->cam.motor) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return esp_cam_motor_get_format(common->cam.motor, format);
}

static esp_err_t common_video_set_parm(struct esp_video *video, struct v4l2_streamparm *stream_parm, struct esp_video_stream *stream)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);
    struct v4l2_captureparm *cp = &stream_parm->parm.capture;
    struct esp_video_param *param = &stream->param;
    const esp_cam_sensor_format_t *sensor_format = common->sensor_format;

    if (!common->cam.sensor) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (!(cp->capability & V4L2_CAP_TIMEPERFRAME)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cp->timeperframe.numerator != 1) {
        ESP_LOGE(TAG, "numerator=%" PRIu32 " is invalid", cp->timeperframe.numerator);
        return ESP_ERR_INVALID_ARG;
    }

    if (cp->timeperframe.denominator == 0) {
        ESP_LOGE(TAG, "denominator=%" PRIu32 " is invalid", cp->timeperframe.denominator);
        return ESP_ERR_INVALID_ARG;
    }

    if ((cp->timeperframe.denominator > sensor_format->fps) ||
            (sensor_format->fps % cp->timeperframe.denominator != 0)) {
        ESP_LOGE(TAG, "denominator=%" PRIu32 " is invalid", cp->timeperframe.denominator);
        return ESP_ERR_INVALID_ARG;
    }

    param->skip_frames = sensor_format->fps / cp->timeperframe.denominator;
    ESP_LOGD(TAG, "skip_frames=%d", param->skip_frames);

    return ESP_OK;
}

static esp_err_t common_video_get_parm(struct esp_video *video, struct v4l2_streamparm *stream_parm, struct esp_video_stream *stream)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);
    struct v4l2_captureparm *cp = &stream_parm->parm.capture;
    struct esp_video_param *param = &stream->param;
    const esp_cam_sensor_format_t *sensor_format = common->sensor_format;

    if (!common->cam.sensor) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    cp->capability |= V4L2_CAP_TIMEPERFRAME;
    cp->timeperframe.numerator = 1;
    cp->timeperframe.denominator = sensor_format->fps;
    if (param->skip_frames > 0) {
        cp->timeperframe.denominator /= param->skip_frames;
    }

    return ESP_OK;
}

static esp_err_t common_video_set_selection(struct esp_video *video, struct v4l2_selection *selection)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    if (common->intf->set_selection) {
        return common->intf->set_selection(common, selection);
    }

    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t common_video_enum_framesizes(struct esp_video *video, struct v4l2_frmsizeenum *frmsize, struct esp_video_stream *stream)
{
    esp_video_device_common_t *common = VIDEO_DEVICE_COMMON(video);

    if (frmsize->index != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (common->intf->check_enum_framesizes) {
        ESP_RETURN_ON_ERROR(common->intf->check_enum_framesizes(common, frmsize), TAG, "format check failed");
    } else {
        if (frmsize->pixel_format != CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(video)) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    frmsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    frmsize->discrete.width = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video);
    frmsize->discrete.height = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  The single ops table shared by ALL video devices                   */
/* ------------------------------------------------------------------ */

static const struct esp_video_ops s_common_video_ops = {
    .init               = common_video_init,
    .deinit             = common_video_deinit,
    .start              = common_video_start,
    .stop               = common_video_stop,
    .enum_format        = common_video_enum_format,
    .set_format         = common_video_set_format,
    .notify             = common_video_notify,
    .set_ext_ctrl       = common_video_set_ext_ctrl,
    .get_ext_ctrl       = common_video_get_ext_ctrl,
    .query_ext_ctrl     = common_video_query_ext_ctrl,
    .set_sensor_format  = common_video_set_sensor_format,
    .get_sensor_format  = common_video_get_sensor_format,
    .query_menu         = common_video_query_menu,
    .set_motor_format   = common_video_set_motor_format,
    .get_motor_format   = common_video_get_motor_format,
    .set_parm           = common_video_set_parm,
    .get_parm           = common_video_get_parm,
    .set_selection      = common_video_set_selection,
    .enum_framesizes    = common_video_enum_framesizes
};

/* ------------------------------------------------------------------ */
/*  Device creation                                                    */
/* ------------------------------------------------------------------ */

esp_err_t esp_video_device_common_create(const esp_video_device_common_config_t *config, esp_video_device_common_t **common_ret)
{
    esp_video_device_common_t *common;
    size_t buf_alignment;
    uint32_t caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_STREAMING;
    uint32_t device_caps = caps | V4L2_CAP_DEVICE_CAPS;

    /**
     * This is internal video device function, so using assert to check the parameters
     */
    assert(config);
    assert(common_ret);
    assert(config->name);
    assert(config->intf);
    assert(config->cam.sensor);
    assert(config->mem_caps);

    ESP_RETURN_ON_ERROR(esp_cache_get_alignment(config->mem_caps, &buf_alignment), TAG, "failed to get cache alignment");

    common = heap_caps_calloc(1, sizeof(esp_video_device_common_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!common) {
        return ESP_ERR_NO_MEM;
    }

    common->video = esp_video_create(config->name, config->id, &s_common_video_ops, common, caps, device_caps);
    if (!common->video) {
        heap_caps_free(common);
        return ESP_ERR_NO_MEM;
    }

    common->intf = config->intf;
    common->cam = config->cam;
    common->mem_caps = config->mem_caps;
    common->buf_alignment = buf_alignment;
    common->sensor_format = config->cam.sensor->cur_format;
    common->use_backup_element = config->use_backup_element;
    common->backup_element = NULL;
    common->priv = config->priv;

    *common_ret = common;

    return ESP_OK;
}

esp_err_t esp_video_device_common_free(esp_video_device_common_t *common)
{
    assert(common);

    ESP_RETURN_ON_ERROR(esp_video_destroy(common->video), TAG, "failed to destroy video");
    heap_caps_free(common);

    return ESP_OK;
}

esp_err_t esp_video_device_common_get_priv(const char *name, void **priv)
{
    esp_video_device_common_t *common;
    struct esp_video *video;

    assert(name);
    assert(priv);

    video = esp_video_device_get_object(name);
    ESP_RETURN_ON_FALSE(video, ESP_ERR_INVALID_ARG, TAG, "video is NULL");

    common = VIDEO_PRIV_DATA(esp_video_device_common_t *, video);
    *priv = common->priv;

    return ESP_OK;
}

esp_err_t esp_video_device_common_get_video_cam(const char *name, esp_video_cam_t *sensor)
{
    esp_video_device_common_t *common;
    struct esp_video *video;

    assert(name);
    assert(sensor);

    video = esp_video_device_get_object(name);
    ESP_RETURN_ON_FALSE(video, ESP_ERR_INVALID_ARG, TAG, "video is NULL");

    common = VIDEO_PRIV_DATA(esp_video_device_common_t *, video);
    *sensor = common->cam;

    return ESP_OK;
}
