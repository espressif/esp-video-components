/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
#ifndef CONFIG_SIMULATED_INTF
#include "mipi_csi.h"
#endif

static const char *TAG = "esp_video_vfs";

#ifndef CONFIG_SIMULATED_INTF
esp_mipi_csi_handle_t csi_test_handle;
#endif

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

    memset(&format, 0, sizeof(struct esp_video_format));
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

    if (ioctl_mmap->length > video->buffer_size) {
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

#ifndef CONFIG_SIMULATED_INTF
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
