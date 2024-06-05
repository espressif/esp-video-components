/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "linux/videodev2.h"
#include "esp_video_init.h"

#define BUFFER_COUNT 2
#define CAPTURE_SECONDS 3

static const char *TAG = "example";

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
static const esp_video_init_csi_config_t csi_config[] = {
    {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
                .scl_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
                .sda_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
            },
            .freq = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
    },
};
#endif

static const esp_video_init_config_t cam_config = {
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR > 0
    .csi      = csi_config,
#endif
};

static esp_err_t camera_capture_stream(void)
{
    int fd;
    esp_err_t ret;
    int fmt_index = 0;
    uint32_t frame_size;
    uint32_t frame_count;
    struct v4l2_buffer buf;
    uint8_t *buffer[BUFFER_COUNT];
    struct v4l2_format init_format;
    struct v4l2_requestbuffers req;
    struct v4l2_capability capability;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fd = open("/dev/video0", O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device");
        return ESP_FAIL;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        ret = ESP_FAIL;
        goto exit_0;
    }

    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);
    ESP_LOGI(TAG, "capabilities:");
    if (capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
    }
    if (capability.capabilities & V4L2_CAP_READWRITE) {
        ESP_LOGI(TAG, "\tREADWRITE");
    }
    if (capability.capabilities & V4L2_CAP_ASYNCIO) {
        ESP_LOGI(TAG, "\tASYNCIO");
    }
    if (capability.capabilities & V4L2_CAP_STREAMING) {
        ESP_LOGI(TAG, "\tSTREAMING");
    }
    if (capability.capabilities & V4L2_CAP_META_OUTPUT) {
        ESP_LOGI(TAG, "\tMETA_OUTPUT");
    }
    if (capability.capabilities & V4L2_CAP_DEVICE_CAPS) {
        ESP_LOGI(TAG, "device capabilities:");
        if (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
            ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
        }
        if (capability.device_caps & V4L2_CAP_READWRITE) {
            ESP_LOGI(TAG, "\tREADWRITE");
        }
        if (capability.device_caps & V4L2_CAP_ASYNCIO) {
            ESP_LOGI(TAG, "\tASYNCIO");
        }
        if (capability.device_caps & V4L2_CAP_STREAMING) {
            ESP_LOGI(TAG, "\tSTREAMING");
        }
        if (capability.device_caps & V4L2_CAP_META_OUTPUT) {
            ESP_LOGI(TAG, "\tMETA_OUTPUT");
        }
    }

    memset(&init_format, 0, sizeof(struct v4l2_format));
    init_format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &init_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        ret = ESP_FAIL;
        goto exit_0;
    }

    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_VFLIP;
    control[0].value    = 1;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to mirror the frame horizontally and skip this step");
    }

    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_HFLIP;
    control[0].value    = 1;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to mirror the frame horizontally and skip this step");
    }

    while (1) {
        struct v4l2_fmtdesc fmtdesc = {
            .index = fmt_index++,
            .type = type,
        };

        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
            break;
        }

        struct v4l2_format format = {
            .type = type,
            .fmt.pix.width = init_format.fmt.pix.width,
            .fmt.pix.height = init_format.fmt.pix.height,
            .fmt.pix.pixelformat = fmtdesc.pixelformat,
        };

        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            if (errno == ESRCH) {
                continue;
            } else {
                ESP_LOGE(TAG, "failed to set format");
                ret = ESP_FAIL;
                goto exit_0;
            }
        }

        ESP_LOGI(TAG, "Capture %s format frames for %d seconds:", (char *)fmtdesc.description, CAPTURE_SECONDS);

        memset(&req, 0, sizeof(req));
        req.count  = BUFFER_COUNT;
        req.type   = type;
        req.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
            ESP_LOGE(TAG, "failed to require buffer");
            ret = ESP_FAIL;
            goto exit_0;
        }

        for (int i = 0; i < BUFFER_COUNT; i++) {
            struct v4l2_buffer buf;

            memset(&buf, 0, sizeof(buf));
            buf.type        = type;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = i;
            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to query buffer");
                ret = ESP_FAIL;
                goto exit_0;
            }

            buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, buf.m.offset);
            if (!buffer[i]) {
                ESP_LOGE(TAG, "failed to map buffer");
                ret = ESP_FAIL;
                goto exit_0;
            }

            if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to queue video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }
        }

        if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
            ESP_LOGE(TAG, "failed to start stream");
            ret = ESP_FAIL;
            goto exit_0;
        }

        frame_count = 0;
        frame_size = 0;
        int64_t start_time_us = esp_timer_get_time();
        while (esp_timer_get_time() - start_time_us < (CAPTURE_SECONDS * 1000 * 1000)) {
            memset(&buf, 0, sizeof(buf));
            buf.type   = type;
            buf.memory = V4L2_MEMORY_MMAP;
            if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to receive video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }

            frame_size += buf.bytesused;

            if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to queue video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }

            frame_count++;
        }

        if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "failed to stop stream");
            ret = ESP_FAIL;
            goto exit_0;
        }

        ESP_LOGI(TAG, "\twidth:  %" PRIu32, format.fmt.pix.width);
        ESP_LOGI(TAG, "\theight: %" PRIu32, format.fmt.pix.height);
        ESP_LOGI(TAG, "\tsize:   %" PRIu32, frame_size / frame_count);
        ESP_LOGI(TAG, "\tFPS:    %" PRIu32, frame_count / CAPTURE_SECONDS);
    }

    ret = ESP_OK;

exit_0:
    close(fd);
    return ret;
}

void app_main(void)
{
    esp_err_t ret = ESP_OK;

    ret = esp_video_init(&cam_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
        return;
    }

    ret = camera_capture_stream();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera capture stream failed with error 0x%x", ret);
        return;
    }
}
