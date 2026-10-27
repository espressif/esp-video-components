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
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"

#define VIDEO_BUFFER_COUNT      2
#define MOUNT_POINT             CONFIG_EXAMPLE_SDMMC_MOUNT_POINT

#if !CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#error "Enable ISP Pipeline Controller in menuconfig"
#endif

/**
 * @brief Values kept across deep sleep.
 *
 * image_index is outside esp_ipa_state_t so a later get does not clear it.
 * esp_ipa_state_t is large because every module array is stored at full size.
 */
typedef struct {
    uint32_t image_index;
    esp_ipa_state_t ipa;
} example_rtc_keep_t;

static const char *TAG = "fast_startup";
static RTC_DATA_ATTR example_rtc_keep_t s_rtc;

typedef struct {
    int cap_fd;
    int m2m_fd;
    uint8_t *cap_buffer[VIDEO_BUFFER_COUNT];
    uint8_t *m2m_cap_buffer;
    uint32_t capture_fmt;
    example_storage_handle_t fatfs;
    bool sd_mounted;
    bool camera_streaming;
    bool jpeg_cap_streaming;
    bool jpeg_out_streaming;
} app_t;

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

/**
 * Cap a single DQBUF wait so cold-boot color-temperature polling cannot
 * overrun CONFIG_EXAMPLE_CT_WAIT_MS when CONFIG_EXAMPLE_RECV_TIMEOUT_MS is larger.
 */
static uint32_t clamp_recv_timeout_ms(int64_t remaining_ms, uint32_t recv_timeout_ms)
{
    if (remaining_ms <= 0) {
        return 0;
    }
    if (remaining_ms < (int64_t)recv_timeout_ms) {
        return (uint32_t)remaining_ms;
    }
    return recv_timeout_ms;
}

static esp_err_t set_jpeg_quality(int fd)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = V4L2_CID_JPEG_CLASS;
    controls.count = 1;
    controls.controls = control;
    control[0].id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    control[0].value = CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY;

    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "failed to set JPEG quality errno=%d", errno);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "JPEG quality=%d", CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY);
    return ESP_OK;
}

static esp_err_t init_capture(app_t *app)
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

    fd = open(EXAMPLE_CAM_DEV_PATH, O_RDWR);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_FAIL, TAG, "failed to open %s errno=%d", EXAMPLE_CAM_DEV_PATH, errno);
    app->cap_fd = fd;
    ESP_RETURN_ON_ERROR(set_recv_timeout(fd, CONFIG_EXAMPLE_RECV_TIMEOUT_MS), TAG,
                        "set camera receive timeout failed");

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
    app->capture_fmt = capture_fmt;

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = capture_fmt;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0, ESP_FAIL, TAG, "failed to set camera format");
    ESP_LOGD(TAG, "capture pixelformat=" V4L2_FMT_STR " %" PRIu32 "x%" PRIu32,
             V4L2_FMT_STR_ARG(capture_fmt), format.fmt.pix.width, format.fmt.pix.height);

    memset(&req, 0, sizeof(req));
    req.count = VIDEO_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_REQBUFS, &req) == 0, ESP_FAIL, TAG, "failed to request camera buffers");

    for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QUERYBUF, &buf) == 0, ESP_FAIL, TAG, "failed to query camera buffer");
        app->cap_buffer[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        ESP_RETURN_ON_FALSE(app->cap_buffer[i] && app->cap_buffer[i] != MAP_FAILED, ESP_FAIL, TAG, "failed to map camera buffer");
        ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buf) == 0, ESP_FAIL, TAG, "failed to queue camera buffer");
        ESP_LOGD(TAG, "queued camera buffer %d length=%" PRIu32, i, buf.length);
    }

    return ESP_OK;
}

static esp_err_t init_jpeg(app_t *app)
{
    int fd;
    struct v4l2_format cam_format;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;

    memset(&cam_format, 0, sizeof(cam_format));
    cam_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_RETURN_ON_FALSE(ioctl(app->cap_fd, VIDIOC_G_FMT, &cam_format) == 0, ESP_FAIL, TAG, "failed to get capture format");

    fd = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_FAIL, TAG, "failed to open %s errno=%d", ESP_VIDEO_JPEG_DEVICE_NAME, errno);
    app->m2m_fd = fd;
    ESP_RETURN_ON_ERROR(set_recv_timeout(fd, CONFIG_EXAMPLE_RECV_TIMEOUT_MS), TAG,
                        "set JPEG receive timeout failed");
    ESP_RETURN_ON_ERROR(set_jpeg_quality(fd), TAG, "set JPEG quality failed");

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = cam_format.fmt.pix.width;
    format.fmt.pix.height = cam_format.fmt.pix.height;
    format.fmt.pix.pixelformat = app->capture_fmt;
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
    app->m2m_cap_buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    ESP_RETURN_ON_FALSE(app->m2m_cap_buffer && app->m2m_cap_buffer != MAP_FAILED, ESP_FAIL, TAG, "failed to map JPEG buffer");
    ESP_RETURN_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buf) == 0, ESP_FAIL, TAG, "failed to queue JPEG buffer");
    ESP_LOGD(TAG, "JPEG buffer length=%" PRIu32, buf.length);

    return ESP_OK;
}

static esp_err_t stream_on_camera(app_t *app)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ESP_LOGD(TAG, "camera stream on");
    ESP_RETURN_ON_FALSE(ioctl(app->cap_fd, VIDIOC_STREAMON, &type) == 0, ESP_FAIL, TAG, "camera stream on failed errno=%d", errno);
    app->camera_streaming = true;
    return ESP_OK;
}

static void stream_off_camera(app_t *app)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (!app->camera_streaming) {
        return;
    }
    ESP_LOGD(TAG, "camera stream off");
    if (ioctl(app->cap_fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "camera stream off failed errno=%d", errno);
    }
    app->camera_streaming = false;
}

static esp_err_t stream_on_jpeg(app_t *app)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ESP_LOGD(TAG, "JPEG capture stream on");
    if (ioctl(app->m2m_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "JPEG capture stream on failed errno=%d", errno);
        return ESP_FAIL;
    }
    app->jpeg_cap_streaming = true;

    ESP_LOGD(TAG, "JPEG output stream on");
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(app->m2m_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "JPEG output stream on failed errno=%d", errno);
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(app->m2m_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "JPEG capture stream off failed errno=%d", errno);
        } else {
            app->jpeg_cap_streaming = false;
        }
        return ESP_FAIL;
    }
    app->jpeg_out_streaming = true;
    return ESP_OK;
}

static void stream_off_jpeg(app_t *app)
{
    int type;

    if (app->jpeg_out_streaming) {
        ESP_LOGD(TAG, "JPEG output stream off");
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (ioctl(app->m2m_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "JPEG output stream off failed errno=%d", errno);
        } else {
            app->jpeg_out_streaming = false;
        }
    }
    if (app->jpeg_cap_streaming) {
        ESP_LOGD(TAG, "JPEG capture stream off");
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(app->m2m_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "JPEG capture stream off failed errno=%d", errno);
        } else {
            app->jpeg_cap_streaming = false;
        }
    }
}

static esp_err_t recycle_one_frame(app_t *app)
{
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(app->cap_fd, VIDIOC_DQBUF, &buf) != 0) {
        ESP_LOGE(TAG, "DQBUF failed errno=%d", errno);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "recycle frame index=%" PRIu32 " bytesused=%" PRIu32 " sequence=%" PRIu32, buf.index, buf.bytesused, buf.sequence);
    if (ioctl(app->cap_fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "QBUF failed errno=%d", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t wait_color_temperature(app_t *app)
{
    int64_t start_us = esp_timer_get_time();
    int32_t ct = 0;

    ESP_LOGD(TAG, "wait ct in [%d, %d], timeout=%d ms, recv=%d ms",
             CONFIG_EXAMPLE_CT_MIN, CONFIG_EXAMPLE_CT_MAX, CONFIG_EXAMPLE_CT_WAIT_MS,
             CONFIG_EXAMPLE_RECV_TIMEOUT_MS);

    while (1) {
        int64_t elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
        int64_t remaining_ms = (int64_t)CONFIG_EXAMPLE_CT_WAIT_MS - elapsed_ms;
        uint32_t recv_ms;
        esp_err_t ret;

        if (remaining_ms <= 0) {
            break;
        }

        recv_ms = clamp_recv_timeout_ms(remaining_ms, CONFIG_EXAMPLE_RECV_TIMEOUT_MS);
        ESP_RETURN_ON_ERROR(set_recv_timeout(app->cap_fd, recv_ms), TAG, "set receive timeout failed");

        ret = recycle_one_frame(app);
        if (ret != ESP_OK) {
            return ret;
        }

        elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
        ret = esp_video_isp_pipeline_get_env_int32("ct", &ct);
        if (ret != ESP_OK) {
            ESP_LOGD(TAG, "ct not ready (%s) elapsed=%" PRId64 " ms", esp_err_to_name(ret), elapsed_ms);
            continue;
        }

        ESP_LOGD(TAG, "ct=%" PRId32 " elapsed=%" PRId64 " ms", ct, elapsed_ms);
        if (ct >= CONFIG_EXAMPLE_CT_MIN && ct <= CONFIG_EXAMPLE_CT_MAX) {
            ESP_LOGI(TAG, "color temperature %" PRId32 " K is in range", ct);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "color temperature %" PRId32 " K stayed outside [%d, %d]",
             ct, CONFIG_EXAMPLE_CT_MIN, CONFIG_EXAMPLE_CT_MAX);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t write_jpeg_file(const char *path, const uint8_t *data, size_t size)
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

static esp_err_t save_first_frame(app_t *app)
{
    char path[128];
    struct v4l2_buffer cap_buf;
    struct v4l2_buffer m2m_out_buf;
    struct v4l2_buffer m2m_cap_buf;
    esp_err_t ret = ESP_OK;
    int n;

    n = snprintf(path, sizeof(path), "%s/%s_no.%04" PRIu32 ".jpg",
                 MOUNT_POINT, CONFIG_EXAMPLE_IMAGE_FILE_NAME, s_rtc.image_index);
    ESP_RETURN_ON_FALSE(n > 0 && n < (int)sizeof(path), ESP_ERR_INVALID_SIZE, TAG, "image path is too long");
    ESP_LOGD(TAG, "dequeue the first frame for %s", path);

    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_RETURN_ON_FALSE(ioctl(app->cap_fd, VIDIOC_DQBUF, &cap_buf) == 0, ESP_FAIL, TAG,
                        "failed to dequeue the first frame errno=%d", errno);
    ESP_LOGD(TAG, "first frame index=%" PRIu32 " bytesused=%" PRIu32 " flags=0x%" PRIx32 " sequence=%" PRIu32,
             cap_buf.index, cap_buf.bytesused, cap_buf.flags, cap_buf.sequence);
    /*
     * DQBUF returns 0 for an empty or failed frame and only sets V4L2_BUF_FLAG_ERROR.
     * Give the camera buffer back before leaving so stream off can still run.
     */
    if ((cap_buf.flags & V4L2_BUF_FLAG_ERROR) || cap_buf.bytesused == 0) {
        ESP_LOGE(TAG, "invalid camera frame flags=0x%" PRIx32 " bytesused=%" PRIu32, cap_buf.flags, cap_buf.bytesused);
        if (ioctl(app->cap_fd, VIDIOC_QBUF, &cap_buf) != 0) {
            ESP_LOGE(TAG, "failed to requeue camera buffer errno=%d", errno);
        }
        return ESP_FAIL;
    }

    memset(&m2m_out_buf, 0, sizeof(m2m_out_buf));
    m2m_out_buf.index = 0;
    m2m_out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    m2m_out_buf.memory = V4L2_MEMORY_USERPTR;
    m2m_out_buf.m.userptr = (unsigned long)app->cap_buffer[cap_buf.index];
    m2m_out_buf.length = cap_buf.bytesused;
    if (ioctl(app->m2m_fd, VIDIOC_QBUF, &m2m_out_buf) != 0) {
        ESP_LOGE(TAG, "failed to queue JPEG input errno=%d", errno);
        if (ioctl(app->cap_fd, VIDIOC_QBUF, &cap_buf) != 0) {
            ESP_LOGE(TAG, "failed to requeue camera buffer errno=%d", errno);
        }
        return ESP_FAIL;
    }

    memset(&m2m_cap_buf, 0, sizeof(m2m_cap_buf));
    m2m_cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(app->m2m_fd, VIDIOC_DQBUF, &m2m_cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to dequeue JPEG output errno=%d", errno);
        if (ioctl(app->cap_fd, VIDIOC_QBUF, &cap_buf) != 0) {
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
    if (ioctl(app->cap_fd, VIDIOC_QBUF, &cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to requeue camera buffer errno=%d", errno);
        ret = ESP_FAIL;
    }
    if (ioctl(app->m2m_fd, VIDIOC_DQBUF, &m2m_out_buf) != 0) {
        ESP_LOGE(TAG, "failed to dequeue JPEG input errno=%d", errno);
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK && ((m2m_cap_buf.flags & V4L2_BUF_FLAG_ERROR) || m2m_cap_buf.bytesused == 0)) {
        ESP_LOGE(TAG, "invalid JPEG frame flags=0x%" PRIx32 " bytesused=%" PRIu32,
                 m2m_cap_buf.flags, m2m_cap_buf.bytesused);
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK) {
        ret = write_jpeg_file(path, app->m2m_cap_buffer, m2m_cap_buf.bytesused);
    }

    m2m_cap_buf.index = 0;
    m2m_cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(app->m2m_fd, VIDIOC_QBUF, &m2m_cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to requeue JPEG buffer errno=%d", errno);
        return ESP_FAIL;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "wrote %s (%" PRIu32 " bytes)", path, m2m_cap_buf.bytesused);
    return ESP_OK;
}

static esp_err_t run_cold(app_t *app)
{
    esp_err_t ret;

    ESP_LOGD(TAG, "cold boot, stream until color temperature is in range");
    ESP_RETURN_ON_ERROR(stream_on_camera(app), TAG, "camera stream on failed");
    ret = wait_color_temperature(app);
    stream_off_camera(app);
    return ret;
}

static esp_err_t run_restore(app_t *app)
{
    esp_err_t ret;

    ESP_LOGD(TAG, "wakeup, set saved IPA state before stream on, image index=%" PRIu32, s_rtc.image_index);
    ret = esp_video_isp_pipeline_set_ipa_state(&s_rtc.ipa);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "saved IPA state rejected (%s)", esp_err_to_name(ret));
        return run_cold(app);
    }

    ret = example_mount_fatfs_to_mmc(&app->fatfs);
    ESP_RETURN_ON_ERROR(ret, TAG, "failed to mount SD card");
    app->sd_mounted = true;

    ret = stream_on_jpeg(app);
    if (ret == ESP_OK) {
        ret = stream_on_camera(app);
    }
    if (ret == ESP_OK) {
        ret = save_first_frame(app);
    }
    stream_off_camera(app);
    stream_off_jpeg(app);

    if (ret == ESP_OK) {
        s_rtc.image_index++;
        ESP_LOGD(TAG, "next image index=%" PRIu32, s_rtc.image_index);
    }
    return ret;
}

static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "enter deep sleep for %d s, image index=%" PRIu32,
             CONFIG_EXAMPLE_SLEEP_TIME_SEC, s_rtc.image_index);
    ESP_LOGD(TAG, "RTC IPA state version=%" PRIu32 " modules=0x%" PRIx32
             " crc=0x%08" PRIx32 " %" PRIu32 "x%" PRIu32,
             s_rtc.ipa.version, s_rtc.ipa.modules, s_rtc.ipa.crc32,
             s_rtc.ipa.width, s_rtc.ipa.height);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_EXAMPLE_SLEEP_TIME_SEC * 1000000ULL);
    esp_deep_sleep_start();
}

void app_main(void)
{
    app_t *app;
    esp_err_t ret;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_causes();
    bool restore = (cause & BIT(ESP_SLEEP_WAKEUP_TIMER)) != 0;

#if LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("ISP", ESP_LOG_DEBUG);
#endif

    ESP_LOGI(TAG, "boot wakeup_cause=%d restore=%d image_index=%" PRIu32,
             (int)cause, restore, s_rtc.image_index);
    ESP_LOGD(TAG, "RTC IPA sizeof=%zu image_index=%" PRIu32, sizeof(s_rtc.ipa),
             s_rtc.image_index);

    app = calloc(1, sizeof(*app));
    ESP_ERROR_CHECK(app ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(example_video_init());
    ESP_ERROR_CHECK(init_capture(app));
    ESP_ERROR_CHECK(init_jpeg(app));

    if (restore) {
        ret = run_restore(app);
    } else {
        ret = run_cold(app);
    }

    if (app->sd_mounted) {
        ESP_LOGD(TAG, "unmount SD card");
        if (example_unmount_fatfs_in_mmc(app->fatfs) != ESP_OK) {
            ESP_LOGE(TAG, "failed to unmount SD card");
            ret = ESP_FAIL;
        }
        app->sd_mounted = false;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "stop before sleep: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGD(TAG, "read IPA state after stream off");
    ESP_ERROR_CHECK(esp_video_isp_pipeline_get_ipa_state(&s_rtc.ipa));
    enter_deep_sleep();
}
