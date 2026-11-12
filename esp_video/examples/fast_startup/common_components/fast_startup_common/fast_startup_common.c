/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

// #undef LOG_LOCAL_LEVEL
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_log.h"
#include "linux/videodev2.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "fast_startup_common.h"

static const char *TAG = "fast_startup_common";

static esp_err_t set_recv_timeout(int fd, uint32_t timeout_ms)
{
    struct timeval timeout = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };

    if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        ESP_LOGE(TAG, "failed to set receive timeout errno=%d", errno);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "receive timeout=%" PRIu32 " ms", timeout_ms);
    return ESP_OK;
}

static esp_err_t set_jpeg_quality(int fd, int quality)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = V4L2_CID_JPEG_CLASS;
    controls.count = 1;
    controls.controls = control;
    control[0].id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    control[0].value = quality;

    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "failed to set JPEG quality errno=%d", errno);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "JPEG quality=%d", quality);
    return ESP_OK;
}

static esp_err_t init_camera(fast_startup_capture_t *cap, const char *cam_dev, uint32_t recv_timeout_ms)
{
    int fd;
    int fmt_index = 0;
    uint32_t capture_fmt = 0;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    const uint32_t jpeg_input_formats[] = {
        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_UYVY,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_GREY
    };

    fd = open(cam_dev, O_RDWR);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_FAIL, TAG, "failed to open %s errno=%d", cam_dev, errno);
    cap->cap_fd = fd;
    ESP_RETURN_ON_ERROR(set_recv_timeout(fd, recv_timeout_ms), TAG, "set camera receive timeout failed");

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_G_FMT, &format) == 0, ESP_FAIL, TAG, "failed to get camera format");
    ESP_LOGD(TAG, "camera format %" PRIu32 "x%" PRIu32, format.fmt.pix.width, format.fmt.pix.height);

    while (!capture_fmt) {
        struct v4l2_fmtdesc fmtdesc = {
            .index = fmt_index++,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        };

        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
            break;
        }
        for (int i = 0; i < (int)(sizeof(jpeg_input_formats) / sizeof(jpeg_input_formats[0])); i++) {
            if (jpeg_input_formats[i] == fmtdesc.pixelformat) {
                capture_fmt = jpeg_input_formats[i];
                break;
            }
        }
    }
    ESP_RETURN_ON_FALSE(capture_fmt, ESP_ERR_NOT_SUPPORTED, TAG, "camera pixel format is not supported by JPEG");
    cap->capture_fmt = capture_fmt;

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = capture_fmt;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0, ESP_FAIL, TAG, "failed to set camera format");
    ESP_LOGD(TAG, "capture pixelformat=" V4L2_FMT_STR " %" PRIu32 "x%" PRIu32,
             V4L2_FMT_STR_ARG(capture_fmt), format.fmt.pix.width, format.fmt.pix.height);

    memset(&req, 0, sizeof(req));
    req.count = FAST_STARTUP_CAPTURE_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_REQBUFS, &req) == 0, ESP_FAIL, TAG, "failed to request camera buffers");

    for (int i = 0; i < FAST_STARTUP_CAPTURE_BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QUERYBUF, &buf) == 0, ESP_FAIL, TAG, "failed to query camera buffer");
        cap->cap_buffer[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        ESP_RETURN_ON_FALSE(cap->cap_buffer[i] && cap->cap_buffer[i] != MAP_FAILED, ESP_FAIL, TAG, "failed to map camera buffer");
        ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buf) == 0, ESP_FAIL, TAG, "failed to queue camera buffer");
        ESP_LOGD(TAG, "queued camera buffer %d length=%" PRIu32, i, buf.length);
    }

    return ESP_OK;
}

static esp_err_t init_jpeg(fast_startup_capture_t *cap, uint32_t recv_timeout_ms, int jpeg_quality)
{
    int fd;
    struct v4l2_format cam_format;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;

    memset(&cam_format, 0, sizeof(cam_format));
    cam_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_RETURN_ON_FALSE(ioctl(cap->cap_fd, VIDIOC_G_FMT, &cam_format) == 0, ESP_FAIL, TAG, "failed to get capture format");

    fd = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_FAIL, TAG, "failed to open %s errno=%d", ESP_VIDEO_JPEG_DEVICE_NAME, errno);
    cap->m2m_fd = fd;
    ESP_RETURN_ON_ERROR(set_recv_timeout(fd, recv_timeout_ms), TAG, "set JPEG receive timeout failed");
    ESP_RETURN_ON_ERROR(set_jpeg_quality(fd, jpeg_quality), TAG, "set JPEG quality failed");

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = cam_format.fmt.pix.width;
    format.fmt.pix.height = cam_format.fmt.pix.height;
    format.fmt.pix.pixelformat = cap->capture_fmt;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0, ESP_FAIL, TAG, "failed to set JPEG input format");

    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_REQBUFS, &req) == 0, ESP_FAIL, TAG, "failed to request JPEG input buffer");

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = cam_format.fmt.pix.width;
    format.fmt.pix.height = cam_format.fmt.pix.height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0, ESP_FAIL, TAG, "failed to set JPEG output format");

    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_REQBUFS, &req) == 0, ESP_FAIL, TAG, "failed to request JPEG output buffer");

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QUERYBUF, &buf) == 0, ESP_FAIL, TAG, "failed to query JPEG buffer");
    cap->m2m_cap_buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    ESP_RETURN_ON_FALSE(cap->m2m_cap_buffer && cap->m2m_cap_buffer != MAP_FAILED, ESP_FAIL, TAG, "failed to map JPEG buffer");
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buf) == 0, ESP_FAIL, TAG, "failed to queue JPEG buffer");
    ESP_LOGD(TAG, "JPEG buffer length=%" PRIu32, buf.length);

    return ESP_OK;
}

esp_err_t fast_startup_capture_init(fast_startup_capture_t *cap, const char *cam_dev,
                                    uint32_t recv_timeout_ms, int jpeg_quality)
{
    ESP_RETURN_ON_FALSE(cap && cam_dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_ERROR(init_camera(cap, cam_dev, recv_timeout_ms), TAG, "init camera failed");
    ESP_RETURN_ON_ERROR(init_jpeg(cap, recv_timeout_ms, jpeg_quality), TAG, "init JPEG failed");
    return ESP_OK;
}

esp_err_t fast_startup_capture_set_timeout(fast_startup_capture_t *cap, uint32_t timeout_ms)
{
    return set_recv_timeout(cap->cap_fd, timeout_ms);
}

static esp_err_t stream_on_jpeg(fast_startup_capture_t *cap)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ESP_LOGD(TAG, "JPEG capture stream on");
    if (ioctl(cap->m2m_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "JPEG capture stream on failed errno=%d", errno);
        return ESP_FAIL;
    }
    cap->jpeg_cap_streaming = true;

    ESP_LOGD(TAG, "JPEG output stream on");
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(cap->m2m_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "JPEG output stream on failed errno=%d", errno);
        return ESP_FAIL;
    }
    cap->jpeg_out_streaming = true;
    return ESP_OK;
}

esp_err_t fast_startup_capture_start(fast_startup_capture_t *cap, bool jpeg)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (jpeg) {
        ESP_RETURN_ON_ERROR(stream_on_jpeg(cap), TAG, "JPEG stream on failed");
    }
    ESP_LOGD(TAG, "camera stream on");
    ESP_RETURN_ON_FALSE(ioctl(cap->cap_fd, VIDIOC_STREAMON, &type) == 0, ESP_FAIL, TAG,
                        "camera stream on failed errno=%d", errno);
    cap->camera_streaming = true;
    return ESP_OK;
}

static void stream_off(int fd, int type, bool *streaming, const char *name)
{
    if (!*streaming) {
        return;
    }
    ESP_LOGD(TAG, "%s stream off", name);
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "%s stream off failed errno=%d", name, errno);
        return;
    }
    *streaming = false;
}

void fast_startup_capture_stop(fast_startup_capture_t *cap)
{
    stream_off(cap->cap_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, &cap->camera_streaming, "camera");
    stream_off(cap->m2m_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, &cap->jpeg_out_streaming, "JPEG output");
    stream_off(cap->m2m_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, &cap->jpeg_cap_streaming, "JPEG capture");
}

esp_err_t fast_startup_capture_skip_frame(fast_startup_capture_t *cap)
{
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cap->cap_fd, VIDIOC_DQBUF, &buf) != 0) {
        ESP_LOGE(TAG, "DQBUF failed errno=%d", errno);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "recycle frame index=%" PRIu32 " bytesused=%" PRIu32 " sequence=%" PRIu32, buf.index, buf.bytesused, buf.sequence);
    if (ioctl(cap->cap_fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "QBUF failed errno=%d", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *f;
    esp_err_t ret = ESP_OK;

    f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "failed to open %s errno=%d", path, errno);
        return ESP_FAIL;
    }

    while (size) {
        size_t written = fwrite(data, 1, size, f);

        if (written == 0) {
            ESP_LOGE(TAG, "failed to write %s errno=%d", path, errno);
            ret = ESP_FAIL;
            break;
        }
        data += written;
        size -= written;
    }

    if (ret == ESP_OK && fflush(f) != 0) {
        ESP_LOGE(TAG, "failed to flush %s errno=%d", path, errno);
        ret = ESP_FAIL;
    }
    if (fclose(f) != 0) {
        ESP_LOGE(TAG, "failed to close %s errno=%d", path, errno);
        ret = ESP_FAIL;
    }
    if (ret != ESP_OK && remove(path) != 0) {
        ESP_LOGE(TAG, "failed to remove incomplete %s errno=%d", path, errno);
    }
    return ret;
}

esp_err_t fast_startup_capture_save_jpeg(fast_startup_capture_t *cap, const char *path)
{
    struct v4l2_buffer cap_buf;
    struct v4l2_buffer m2m_out_buf;
    struct v4l2_buffer m2m_cap_buf;
    esp_err_t ret = ESP_OK;

    ESP_LOGD(TAG, "dequeue frame for %s", path);

    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_RETURN_ON_FALSE(ioctl(cap->cap_fd, VIDIOC_DQBUF, &cap_buf) == 0, ESP_FAIL, TAG,
                        "failed to dequeue frame errno=%d", errno);
    ESP_LOGD(TAG, "frame index=%" PRIu32 " bytesused=%" PRIu32 " flags=0x%" PRIx32 " sequence=%" PRIu32,
             cap_buf.index, cap_buf.bytesused, cap_buf.flags, cap_buf.sequence);
    /*
     * DQBUF returns 0 for an empty or failed frame and only sets V4L2_BUF_FLAG_ERROR.
     * Give the camera buffer back before leaving so stream off can still run.
     */
    if ((cap_buf.flags & V4L2_BUF_FLAG_ERROR) || cap_buf.bytesused == 0) {
        ESP_LOGE(TAG, "invalid camera frame flags=0x%" PRIx32 " bytesused=%" PRIu32, cap_buf.flags, cap_buf.bytesused);
        if (ioctl(cap->cap_fd, VIDIOC_QBUF, &cap_buf) != 0) {
            ESP_LOGE(TAG, "failed to requeue camera buffer errno=%d", errno);
        }
        return ESP_FAIL;
    }

    memset(&m2m_out_buf, 0, sizeof(m2m_out_buf));
    m2m_out_buf.index = 0;
    m2m_out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    m2m_out_buf.memory = V4L2_MEMORY_USERPTR;
    m2m_out_buf.m.userptr = (unsigned long)cap->cap_buffer[cap_buf.index];
    m2m_out_buf.length = cap_buf.bytesused;
    if (ioctl(cap->m2m_fd, VIDIOC_QBUF, &m2m_out_buf) != 0) {
        ESP_LOGE(TAG, "failed to queue JPEG input errno=%d", errno);
        if (ioctl(cap->cap_fd, VIDIOC_QBUF, &cap_buf) != 0) {
            ESP_LOGE(TAG, "failed to requeue camera buffer errno=%d", errno);
        }
        return ESP_FAIL;
    }

    memset(&m2m_cap_buf, 0, sizeof(m2m_cap_buf));
    m2m_cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cap->m2m_fd, VIDIOC_DQBUF, &m2m_cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to dequeue JPEG output errno=%d", errno);
        if (ioctl(cap->cap_fd, VIDIOC_QBUF, &cap_buf) != 0) {
            ESP_LOGE(TAG, "failed to requeue camera buffer errno=%d", errno);
        }
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "JPEG output index=%" PRIu32 " bytesused=%" PRIu32 " flags=0x%" PRIx32,
             m2m_cap_buf.index, m2m_cap_buf.bytesused, m2m_cap_buf.flags);

    /*
     * Encode has finished, so the camera buffer can go back. Then take the JPEG
     * input out of the done list before deciding whether the output is usable.
     */
    if (ioctl(cap->cap_fd, VIDIOC_QBUF, &cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to requeue camera buffer errno=%d", errno);
        ret = ESP_FAIL;
    }
    if (ioctl(cap->m2m_fd, VIDIOC_DQBUF, &m2m_out_buf) != 0) {
        ESP_LOGE(TAG, "failed to dequeue JPEG input errno=%d", errno);
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK && ((m2m_cap_buf.flags & V4L2_BUF_FLAG_ERROR) || m2m_cap_buf.bytesused == 0)) {
        ESP_LOGE(TAG, "invalid JPEG frame flags=0x%" PRIx32 " bytesused=%" PRIu32,
                 m2m_cap_buf.flags, m2m_cap_buf.bytesused);
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK) {
        ret = write_file(path, cap->m2m_cap_buffer, m2m_cap_buf.bytesused);
    }

    m2m_cap_buf.index = 0;
    m2m_cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cap->m2m_fd, VIDIOC_QBUF, &m2m_cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to requeue JPEG buffer errno=%d", errno);
        return ESP_FAIL;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "wrote %s (%" PRIu32 " bytes)", path, m2m_cap_buf.bytesused);
    return ESP_OK;
}
