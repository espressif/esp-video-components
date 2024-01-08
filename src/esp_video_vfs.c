/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/lock.h>
#include <sys/errno.h>
#include <sys/param.h>
#include "linux/videodev2.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "esp_video_vfs.h"
#include "esp_video_log.h"
#ifdef CONFIG_MIPI_CSI_ENABLE
#include "mipi_csi.h"
#endif

static const char *TAG = "esp_video_vfs";

/**
 * Todo: AEG-1094
 */
static const int s_control_id_map_table[][2] = {
    { V4L2_CID_3A_LOCK, CAM_SENSOR_3A_LOCK },
    { V4L2_CID_FLASH_LED_MODE, CAM_SENSOR_FLASH_LED }
};

static const int s_pixel_size_map_table[][2] = {
    { V4L2_PIX_FMT_RGB565, 2 },
    { V4L2_PIX_FMT_JPEG, 1 }
};

#ifdef CONFIG_MIPI_CSI_ENABLE
esp_mipi_csi_handle_t csi_test_handle;
#endif

static int esp_err_to_libc_errno(esp_err_t err)
{
    int libc_errno;

    switch (err) {
    case ESP_OK:
        libc_errno = 0;
        break;
    case ESP_FAIL:
        libc_errno = EIO;
        break;
    case ESP_ERR_NO_MEM:
        libc_errno = ENOMEM;
        break;
    case ESP_ERR_INVALID_ARG:
        libc_errno = EINVAL;
        break;
    case ESP_ERR_INVALID_STATE:
        libc_errno = EBUSY;
        break;
    case ESP_ERR_INVALID_SIZE:
        libc_errno = EINVAL;
        break;
    case ESP_ERR_NOT_FOUND:
        libc_errno = ENOENT;
        break;
    case ESP_ERR_NOT_SUPPORTED:
        libc_errno = ENOTSUP;
        break;
    case ESP_ERR_TIMEOUT:
        libc_errno = ETIMEDOUT;
        break;
    case ESP_ERR_INVALID_RESPONSE:
        libc_errno = EBADMSG;
        break;
    case ESP_ERR_INVALID_VERSION:
        libc_errno = EINVAL;
        break;
    case ESP_ERR_NOT_FINISHED:
        libc_errno = EBUSY;
        break;
    default:
        ESP_LOGE(TAG, "esp_err %x is not supported", err);
        libc_errno = EINVAL;
        break;
    }

    return libc_errno;
}

static uint32_t get_pixel_size_by_format(uint32_t format)
{
    for (int i = 0; i < ARRAY_SIZE(s_pixel_size_map_table); i++) {
        if (s_pixel_size_map_table[i][0] == format) {
            return s_pixel_size_map_table[i][1];
        }
    }

    return 0;
}

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

static int esp_video_vfs_ioctl_querycap(struct esp_video *video, struct v4l2_capability *cap)
{
    memset(cap, 0, sizeof(struct v4l2_capability));

    strlcpy((char *)cap->driver, video->dev_name, sizeof(cap->driver));
    cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;

    return 0;
}

static int esp_video_vfs_ioctl_g_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    esp_err_t ret;
    struct esp_video_format format;

    if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        errno = EINVAL;
        return -1;
    }

    ret = esp_video_get_format(video, &format);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    memset(&fmt->fmt, 0, sizeof(fmt->fmt));
    fmt->fmt.pix.width       = format.width;
    fmt->fmt.pix.height      = format.height;
    fmt->fmt.pix.pixelformat = format.pixel_format;

    return 0;
}

static int esp_video_vfs_ioctl_s_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    esp_err_t ret;
    struct esp_video_format format;

    if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        errno = EINVAL;
        return -1;
    }

    memset(&format, 0, sizeof(struct esp_video_format));
    format.pixel_bytes = get_pixel_size_by_format(fmt->fmt.pix.pixelformat);
    if (!format.pixel_bytes) {
        errno = EINVAL;
        return -1;
    }
    format.width        = fmt->fmt.pix.width;
    format.height       = fmt->fmt.pix.height;
    format.pixel_format = fmt->fmt.pix.pixelformat;

    ret = esp_video_set_format(video, &format);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int esp_video_vfs_ioctl_try_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    return esp_video_vfs_ioctl_s_fmt(video, fmt);
}

static int esp_video_vfs_ioctl_streamon(struct esp_video *video, int *arg)
{
    esp_err_t ret;

    ret = esp_video_start_capture(video);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int esp_video_vfs_ioctl_streamoff(struct esp_video *video, int *arg)
{
    esp_err_t ret;

    ret = esp_video_stop_capture(video);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int esp_video_vfs_ioctl_reqbufs(struct esp_video *video, struct v4l2_requestbuffers *req_bufs)
{
    esp_err_t ret;

    /* Only support memory buffer MMAP mode */

    if (req_bufs->memory != V4L2_MEMORY_MMAP) {
        errno = EINVAL;
        return -1;
    }

    /* Only support camera capture */

    if (req_bufs->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        errno = EINVAL;
        return -1;
    }

    if (req_bufs->count == 0) {
        errno = EINVAL;
        return -1;
    }

    ret = esp_video_setup_buffer(video, req_bufs->count);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int esp_video_vfs_ioctl_querybuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    uint32_t count;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        errno = EINVAL;
        return -1;
    }

    /* Only support camera capture */

    if (vbuf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        errno = EINVAL;
        return -1;
    }

    ret = esp_video_get_buffer_count(video, &count);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    if (vbuf->index >= count) {
        errno = EINVAL;
        return -1;
    }

    ret = esp_video_get_buffer_length(video, vbuf->index, &vbuf->length);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    ret = esp_video_get_buffer_offset(video, vbuf->index, &vbuf->m.offset);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int esp_video_vfs_ioctl_mmap(struct esp_video *video, struct esp_video_ioctl_mmap *ioctl_mmap)
{
    uint8_t *buffer;

    if (ioctl_mmap->length > video->buf_info.size) {
        errno = EINVAL;
        return -1;
    }

    buffer = esp_video_get_buffer_by_offset(video, ioctl_mmap->offset);
    if (!buffer) {
        errno = EINVAL;
        return -1;
    }

    ioctl_mmap->mapped_ptr = buffer;

    return 0;
}

static int esp_video_vfs_ioctl_qbuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    uint32_t count;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        errno = EINVAL;
        return -1;
    }

    /* Only support camera capture */

    if (vbuf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        errno = EINVAL;
        return -1;
    }

    ret = esp_video_get_buffer_count(video, &count);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    if (vbuf->index >= count) {
        errno = EINVAL;
        return -1;
    }

    esp_video_free_buffer_index(video, vbuf->index);

#ifdef CONFIG_MIPI_CSI_ENABLE
    esp_mipi_csi_new_buffer_available(csi_test_handle);
#endif
    return 0;
}

static int esp_video_vfs_ioctl_dqbuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    uint32_t count;
    uint32_t recv_size;
    uint32_t offset;
    uint8_t *recv_buf;
    uint32_t ticks = portMAX_DELAY;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        errno = EINVAL;
        return -1;
    }

    /* Only support camera capture */

    if (vbuf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        errno = EINVAL;
        return -1;
    }

    ret = esp_video_get_buffer_count(video, &count);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    if (vbuf->index >= count) {
        errno = EINVAL;
        return -1;
    }

    recv_buf = esp_video_recv_buffer(video, &recv_size, &offset, ticks);
    if (!recv_buf) {
        errno = EIO;
        return -1;
    }

    vbuf->index     = esp_video_get_buffer_index(video, recv_buf);
    vbuf->bytesused = recv_size;
    vbuf->m.offset = offset;

    return 0;
}

static int esp_video_vfs_ioctl_s_param(struct esp_video *video, struct v4l2_streamparm *param)
{
    esp_err_t ret;
    struct esp_video_format format;

    if (param->parm.capture.timeperframe.numerator != 1) {
        errno = EINVAL;
        return -1;
    }

    ret = esp_video_get_format(video, &format);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    format.fps = param->parm.capture.timeperframe.denominator;
    ret = esp_video_set_format(video, &format);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int esp_video_vfs_ioctl_op_ext_ctrls(struct esp_video *video, struct v4l2_ext_controls *controls, bool set)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    esp_camera_device_t *cam_dev = video->cam_dev;

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

    if (ret != ESP_OK) {
        errno = esp_err_to_libc_errno(ret);
        return -1;
    }

    return 0;
}

/**
 * Todo: AEG-1095
 */
static int esp_video_vfs_ioctl_query_ext_ctrls(struct esp_video *video, struct v4l2_query_ext_ctrl *qc)
{
    esp_err_t ret;
    esp_camera_device_t *cam_dev = video->cam_dev;

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
    if (ret != ESP_OK) {
        errno = esp_err_to_libc_errno(ret);
        return -1;
    }

    return 0;
}

static int esp_video_vfs_open(void *ctx, const char *path, int flags, int mode)
{
    struct esp_video *video = (struct esp_video *)ctx;

    /* Open video here to initialize software resource and hardware */

    video = esp_video_open(video->dev_name);
    if (!video) {
        errno = ENOENT;
        return -1;
    }

    return video->id;
}

static ssize_t esp_video_vfs_write(void *ctx, int fd, const void *data, size_t size)
{
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0 && data && size);
    assert(video);

    errno = EPERM;

    return -1;
}

static ssize_t esp_video_vfs_read(void *ctx, int fd, void *data, size_t size)
{
    size_t n;
    uint8_t *vbuf;
    uint32_t recv_size;
    uint32_t offset;
    uint32_t ticks = portMAX_DELAY;
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0 && data && size);
    assert(video);

    vbuf = esp_video_recv_buffer(video, &recv_size, &offset, ticks);
    if (!vbuf) {
        return 0;
    }

    n = MIN(size, recv_size);
    ESP_VIDEO_LOGD("actually read n=%d", n);

    memcpy(data, vbuf + offset, n);

    return (ssize_t)n;
}

static int esp_video_vfs_fstat(void *ctx, int fd, struct stat *st)
{
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0 && st);
    assert(video);

    memset(st, 0, sizeof(*st));

    return 0;
}

static int esp_video_vfs_close(void *ctx, int fd)
{
    esp_err_t ret;
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);

    ret = esp_video_close(video);
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int esp_video_vfs_fcntl(void *ctx, int fd, int cmd, int arg)
{
    int ret;
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);

    switch (cmd) {
    case F_GETFL:
        ret = O_RDONLY;
        break;
    default:
        ret = -1;
        errno = ENOSYS;
        break;
    }

    return ret;
}

static int esp_video_vfs_fsync(void *ctx, int fd)
{
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);

    return 0;
}

static int esp_video_vfs_ioctl(void *ctx, int fd, int cmd, va_list args)
{
    int ret;
    void *arg_ptr;
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);

    arg_ptr = va_arg(args, void *);
    if (!arg_ptr) {
        errno = EINVAL;
        return -1;
    }

    switch (cmd) {
    case VIDIOC_QBUF:
        ret = esp_video_vfs_ioctl_qbuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_DQBUF:
        ret = esp_video_vfs_ioctl_dqbuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_QUERYCAP:
        ret = esp_video_vfs_ioctl_querycap(video, (struct v4l2_capability *)arg_ptr);
        break;
    case VIDIOC_G_FMT:
        ret = esp_video_vfs_ioctl_g_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_S_FMT:
        ret = esp_video_vfs_ioctl_s_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_TRY_FMT:
        ret = esp_video_vfs_ioctl_try_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_STREAMON:
        ret = esp_video_vfs_ioctl_streamon(video, (int *)arg_ptr);
        break;
    case VIDIOC_STREAMOFF:
        ret = esp_video_vfs_ioctl_streamoff(video, (int *)arg_ptr);
        break;
    case VIDIOC_REQBUFS:
        ret = esp_video_vfs_ioctl_reqbufs(video, (struct v4l2_requestbuffers *)arg_ptr);
        break;
    case VIDIOC_QUERYBUF:
        ret = esp_video_vfs_ioctl_querybuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_S_PARM:
        ret = esp_video_vfs_ioctl_s_param(video, (struct v4l2_streamparm *)arg_ptr);
        break;
    case VIDIOC_MMAP:
        ret = esp_video_vfs_ioctl_mmap(video, (struct esp_video_ioctl_mmap *)arg_ptr);
        break;
    case VIDIOC_G_EXT_CTRLS:
        ret = esp_video_vfs_ioctl_op_ext_ctrls(video, (struct v4l2_ext_controls *)arg_ptr, false);
        break;
    case VIDIOC_S_EXT_CTRLS:
        ret = esp_video_vfs_ioctl_op_ext_ctrls(video, (struct v4l2_ext_controls *)arg_ptr, true);
        break;
    case VIDIOC_QUERY_EXT_CTRL:
        ret = esp_video_vfs_ioctl_query_ext_ctrls(video, (struct v4l2_query_ext_ctrl *)arg_ptr);
        break;
    case VIDIOC_ENUM_FMT:
    case VIDIOC_ENUM_FRAMESIZES:
    default:
        ret = -1;
        errno = EINVAL;
        break;
    }

    return ret;
}

static const esp_vfs_t s_esp_video_vfs = {
    .flags   = ESP_VFS_FLAG_CONTEXT_PTR,
    .open_p  = esp_video_vfs_open,
    .close_p = esp_video_vfs_close,
    .write_p = esp_video_vfs_write,
    .read_p  = esp_video_vfs_read,
    .fcntl_p = esp_video_vfs_fcntl,
    .fsync_p = esp_video_vfs_fsync,
    .fstat_p = esp_video_vfs_fstat,
    .ioctl_p = esp_video_vfs_ioctl
};

/**
 * @brief Register video device into VFS system.
 *
 * @param name Video device name
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_vfs_dev_video_register(const char *name, struct esp_video *video)
{
    esp_err_t ret;
    char *vfs_name;

    ret = asprintf(&vfs_name, "/dev/%s", name);
    if (ret <= 0) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_vfs_register(vfs_name, &s_esp_video_vfs, video);
    free(vfs_name);

    return ret;
}

/**
 * @brief Unregister video device from VFS system.
 *
 * @param name Video device name
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_vfs_dev_video_unregister(const char *name)
{
    esp_err_t ret;
    char *vfs_name;

    ret = asprintf(&vfs_name, "/dev/%s", name);
    if (ret <= 0) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_vfs_unregister(name);
    free(vfs_name);

    return ret;
}
