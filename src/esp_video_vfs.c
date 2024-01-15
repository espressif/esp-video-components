/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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
#include "esp_color_formats.h"

static const char *TAG = "esp_video_vfs";

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
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);
#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    return esp_video_media_ioctl(video, cmd, args);
#else
    return esp_video_ioctl(video, cmd, args);
#endif
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
