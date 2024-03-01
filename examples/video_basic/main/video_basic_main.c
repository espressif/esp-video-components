/* SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "linux/videodev2.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_camera.h"

static const char *TAG = "camera";

#if CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM >0
static const esp_camera_sccb_config_t sccb_config[CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM] = {
    {
        .i2c_or_i3c = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C,
        .scl_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_SCL_PIN,
        .sda_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_SDA_PIN,
        .port       = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_PORT,
        .freq       = CONFIG_ESP_VIDEO_CAMERA_SCCB0_I2C_I3C_FREQ,
    },
#if CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM > 1
    {
        .i2c_or_i3c = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C,
        .scl_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C_SCL_PIN,
        .sda_pin    = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C_SDA_PIN,
        .port       = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C_PORT,
        .freq       = CONFIG_ESP_VIDEO_CAMERA_SCCB1_I2C_I3C_FREQ,
    },
#endif
};
#endif

#if CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM > 0
static const esp_camera_csi_config_t csi_config[CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM] = {
    {
        .ctrl_cfg = {
            .sccb_config_index = CONFIG_ESP_VIDEO_CAMERA_CSI0_SCCB_INDEX,
            .xclk_pin          = CONFIG_ESP_VIDEO_CAMERA_CSI0_XCLK_PIN,
            .reset_pin         = CONFIG_ESP_VIDEO_CAMERA_CSI0_RESET_PIN,
            .pwdn_pin          = CONFIG_ESP_VIDEO_CAMERA_CSI0_PWDN_PIN,
            .xclk_freq_hz      = CONFIG_ESP_VIDEO_CAMERA_CSI0_XCLK_FREQ,
            .xclk_timer        = LEDC_TIMER_0,
            .xclk_timer_channel = LEDC_CHANNEL_0,
        }
    },
};
#endif

#if CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 0
static const esp_camera_dvp_config_t dvp_config[CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM] = {
    {
        .ctrl_cfg = {
            .sccb_config_index = CONFIG_ESP_VIDEO_CAMERA_DVP0_SCCB_INDEX,
            .reset_pin         = CONFIG_ESP_VIDEO_CAMERA_DVP0_RESET_PIN,
            .pwdn_pin          = CONFIG_ESP_VIDEO_CAMERA_DVP0_PWDN_PIN,
            .xclk_freq_hz      = CONFIG_ESP_VIDEO_CAMERA_DVP0_XCLK_FREQ,
#ifndef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
            .xclk_timer        = LEDC_TIMER_1,
            .xclk_timer_channel = LEDC_CHANNEL_0,
#endif
        }
        ,
        .dvp_pin_cfg = {
            .data_pin = {
                CONFIG_ESP_VIDEO_CAMERA_DVP0_D0_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP0_D1_PIN,
                CONFIG_ESP_VIDEO_CAMERA_DVP0_D2_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP0_D3_PIN,
                CONFIG_ESP_VIDEO_CAMERA_DVP0_D4_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP0_D5_PIN,
                CONFIG_ESP_VIDEO_CAMERA_DVP0_D6_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP0_D7_PIN,
            },
            .vsync_pin = CONFIG_ESP_VIDEO_CAMERA_DVP0_VSYNC_PIN,
            .href_pin = CONFIG_ESP_VIDEO_CAMERA_DVP0_HREF_PIN,
            .pclk_pin = CONFIG_ESP_VIDEO_CAMERA_DVP0_PCLK_PIN,
            .xclk_pin = CONFIG_ESP_VIDEO_CAMERA_DVP0_XCLK_PIN,
        }
    },
#if CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 1
    {
        .ctrl_cfg = {
            .sccb_config_index = CONFIG_ESP_VIDEO_CAMERA_DVP1_SCCB_INDEX,
            .reset_pin         = CONFIG_ESP_VIDEO_CAMERA_DVP1_RESET_PIN,
            .pwdn_pin          = CONFIG_ESP_VIDEO_CAMERA_DVP1_PWDN_PIN,
            .xclk_freq_hz      = CONFIG_ESP_VIDEO_CAMERA_DVP1_XCLK_FREQ,
#ifndef CONFIG_DVP_ENABLE_OUTPUT_CLOCK
            .xclk_timer        = LEDC_TIMER_2,
            .xclk_timer_channel = LEDC_CHANNEL_0,
#endif
        },
        .dvp_pin_cfg = {
            .data_pin = {
                CONFIG_ESP_VIDEO_CAMERA_DVP1_D0_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP1_D1_PIN,
                CONFIG_ESP_VIDEO_CAMERA_DVP1_D2_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP1_D3_PIN,
                CONFIG_ESP_VIDEO_CAMERA_DVP1_D4_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP1_D5_PIN,
                CONFIG_ESP_VIDEO_CAMERA_DVP1_D6_PIN, CONFIG_ESP_VIDEO_CAMERA_DVP1_D7_PIN,
            },
            .vsync_pin = CONFIG_ESP_VIDEO_CAMERA_DVP1_VSYNC_PIN,
            .href_pin = CONFIG_ESP_VIDEO_CAMERA_DVP1_HREF_PIN,
            .pclk_pin = CONFIG_ESP_VIDEO_CAMERA_DVP1_PCLK_PIN,
            .xclk_pin = CONFIG_ESP_VIDEO_CAMERA_DVP1_XCLK_PIN,
        }
    },
#endif
};
#endif
const esp_camera_sim_config_t s_sim_config[2] = {
    {.id = 0},
    {.id = 1}
};

static const esp_camera_config_t cam_config = {
    .sccb_num = CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM,
    .sccb     = sccb_config,
#if CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM > 0
    .csi      = csi_config,
#endif
#if CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 0
    .dvp_num  = CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM,
    .dvp      = dvp_config,
#endif
#ifdef CONFIG_SIMULATED_INTF
    .sim_num  = sizeof(s_sim_config) / sizeof(s_sim_config[0]),
    .sim = s_sim_config,
#endif
};

static esp_err_t camera_capture_stream(void)
{
    int fd;
    esp_err_t ret;
    int index = 0;
    uint8_t *buffer[4];
    char format_desc[5] = {0};
    struct v4l2_buffer buf;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fd = open("/dev/video0", O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device");
        return ESP_ERR_NOT_FOUND;
    }

    format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        ret = ESP_FAIL;
        goto errout_get_fmt;
    }

    memcpy(format_desc, &format.fmt.pix.pixelformat, 4);
    ESP_LOGI(TAG, "Frame width=%" PRIu32 " height=%" PRIu32 " format=%s",
             format.fmt.pix.width, format.fmt.pix.height, format_desc);

    memset(&req, 0, sizeof(req));
    req.count  = ARRAY_SIZE(buffer);
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "failed to require buffer");
        ret = ESP_FAIL;
        goto errout_get_fmt;
    }

    for (int i = 0; i < ARRAY_SIZE(buffer); i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type        = type;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to query buffer");
            ret = ESP_FAIL;
            goto errout_get_fmt;
        }

        buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, buf.m.offset);
        if (!buffer[i]) {
            ESP_LOGE(TAG, "failed to map buffer");
            ret = ESP_FAIL;
            goto errout_get_fmt;
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            ret = ESP_FAIL;
            goto errout_get_fmt;
        }
    }

    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "failed to start stream");
        ret = ESP_FAIL;
        goto errout_get_fmt;
    }

    while (1) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = type;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to receive video frame");
            ret = ESP_FAIL;
            goto errout_get_fmt;
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            ret = ESP_FAIL;
            goto errout_get_fmt;
        }

        ESP_LOGI(TAG, "frame buffer index=%d size=%d", index++, (int)buf.bytesused);
    }

    if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "failed to stop stream");
        ret = ESP_FAIL;
        goto errout_get_fmt;
    }

    ret = ESP_OK;

errout_get_fmt:
    close(fd);
    return ret;
}

void app_main(void)
{
    esp_err_t ret = ESP_OK;

    ret = esp_camera_init(&cam_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
    }

    ret = camera_capture_stream();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera capture stream failed with error 0x%x", ret);
    }
}
