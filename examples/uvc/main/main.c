/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "freertos/FreeRTOS.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_dma_utils.h"
#include "usb_device_uvc.h"
#include "uvc_frame_config.h"

#include "linux/videodev2.h"
#include "esp_video.h"
#ifdef CONFIG_UVC_CAM_ENABLE_DVP_JPEG_SW_CODEC
#include "img_converters.h"
#else
#include "hal/jpeg_types.h"
#include "jpeg.h"
#endif

#ifdef CONFIG_UVC_CAM_ENABLE_DVP_JPEG_SW_CODEC
#define WIDTH                      CONFIG_UVC_DEFAULT_FRAMESIZE_WIDTH
#define HEIGHT                     CONFIG_UVC_DEFAULT_FRAMESIZE_HEIGT
#endif

#define JPEG_ENC_QUALITY           (85)

#define UVC_MAX_FRAMESIZE_SIZE     (800*1024)

typedef struct {
#ifndef CONFIG_UVC_CAM_ENABLE_DVP_JPEG_SW_CODEC
    jpeg_encoder_handle_t jpeg_handle;
#endif
    uint8_t *jpeg_last_buf;
    uint8_t *jpeg_next_buf;
    uint8_t *camera_next_buf;
    uint8_t *camera_last_buf;
    uint8_t *jpeg_out_buf;
    uvc_fb_t uvc_fb;

    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
} fb_t;

static const char *TAG = "uvc";
static fb_t s_fb;
static uint8_t *buffer[4];
static int frame_count = 0;
static TimerHandle_t timer;

static const esp_camera_sccb_config_t s_sccb_config[CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM] = {
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

#if CONFIG_UVC_CAM_ENABLE_MIPI_CSI && CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM > 0
static const esp_camera_csi_config_t s_csi_config[CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM] = {
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

#if CONFIG_UVC_CAM_ENABLE_DVP && CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 0
static esp_camera_dvp_config_t s_dvp_config[CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM] = {
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
    }
};
#endif

static const esp_camera_config_t s_cam_config = {
    .sccb_num = CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM,
    .sccb     = s_sccb_config,
#if CONFIG_UVC_CAM_ENABLE_MIPI_CSI && CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM > 0
    .csi      = s_csi_config,
#endif
#if CONFIG_UVC_CAM_ENABLE_DVP && CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 0
    .dvp     = s_dvp_config,
    .dvp_num = 1,
#endif
};

static void camera_stop_cb(void *cb_ctx)
{
    int video_fd = (int)cb_ctx;

    ESP_LOGI(TAG, "Camera Stop");
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMOFF, &type)) {
        ESP_LOGE(TAG, "failed to stop stream");
    }
}

static esp_err_t camera_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
{
    int video_fd = (int)cb_ctx;
    ESP_LOGI(TAG, "Camera Start");
    ESP_LOGI(TAG, "Format: %d, width: %d, height: %d, rate: %d", format, width, height, rate);

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMON, &type)) {
        ESP_LOGE(TAG, "failed to start stream");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static uvc_fb_t *camera_fb_get_cb(void *cb_ctx)
{
    esp_err_t err;
    int video_fd = (int)cb_ctx;

    int camera_img_size = 0;
    int res = 0;

    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    res = ioctl(video_fd, VIDIOC_DQBUF, &buf);
    if (res != 0) {
        ESP_LOGE(TAG, "failed to receive video frame");
        return NULL;
    }

    s_fb.jpeg_next_buf = buffer[buf.index];

#ifdef CONFIG_UVC_CAM_ENABLE_DVP_JPEG_SW_CODEC
    if (s_fb.pixel_format == V4L2_PIX_FMT_JPEG) {
        err = ESP_OK;
        memcpy(s_fb.jpeg_out_buf, s_fb.jpeg_next_buf, buf.bytesused);
        camera_img_size = buf.bytesused;
    } else {
        uint8_t *jpeg_ptr;
        size_t jpeg_size;

        bool tx_valid = fmt2jpg(s_fb.jpeg_next_buf, buf.bytesused, s_fb.width,
                                s_fb.height, PIXFORMAT_RGB565, JPEG_ENC_QUALITY, &jpeg_ptr, &jpeg_size);
        if (tx_valid) {
            err = ESP_OK;
            memcpy(s_fb.jpeg_out_buf, jpeg_ptr, jpeg_size);
            camera_img_size = jpeg_size;
            free(jpeg_ptr);
        } else {
            err = ESP_FAIL;
        }
    }
#else
    err = jpeg_encoder_process(s_fb.jpeg_handle, s_fb.jpeg_next_buf, s_fb.jpeg_out_buf, &camera_img_size);
#endif

    if (ioctl(video_fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to free video frame");
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "jpeg_encoder_process failed (%d)", err);
        return NULL;
    }
    if (s_fb.jpeg_next_buf != s_fb.jpeg_last_buf) {
        s_fb.camera_next_buf = s_fb.jpeg_last_buf;
        s_fb.jpeg_last_buf = s_fb.jpeg_next_buf;
    }
    frame_count++;
    uint64_t us = (uint64_t)esp_timer_get_time();

    s_fb.uvc_fb.buf = s_fb.jpeg_out_buf;
    s_fb.uvc_fb.len = camera_img_size;
#ifdef CONFIG_UVC_CAM_ENABLE_DVP_JPEG_SW_CODEC
    s_fb.uvc_fb.width = s_fb.width;
    s_fb.uvc_fb.height = s_fb.height;
#else
    s_fb.uvc_fb.width = WIDTH;
    s_fb.uvc_fb.height = HEIGHT;
#endif
    s_fb.uvc_fb.format = UVC_FORMAT_JPEG;
    s_fb.uvc_fb.timestamp.tv_sec = us / 1000000UL;
    s_fb.uvc_fb.timestamp.tv_usec = us % 1000000UL;

    if (s_fb.uvc_fb.len > UVC_MAX_FRAMESIZE_SIZE) {
        ESP_LOGE(TAG, "Frame size %d is larger than max frame size %d", s_fb.uvc_fb.len, UVC_MAX_FRAMESIZE_SIZE);
        return NULL;
    }
    return &s_fb.uvc_fb;
}

static void camera_fb_return_cb(uvc_fb_t *fb, void *cb_ctx)
{
    (void)cb_ctx;
    assert(fb == &s_fb.uvc_fb);
}

#ifndef CONFIG_UVC_CAM_ENABLE_DVP_JPEG_SW_CODEC
static esp_err_t jepg_encoder_init(void)
{
    jpeg_encode_config_t enc_config = {
        .src_type = JPEG_ENC_RGB565,
        .sub_sample = JPEG_ENC_SUB_YUV420,
        .image_quality = JPEG_ENC_QUALITY,
        .width = WIDTH,
        .hight = HEIGHT,
    };

    ESP_ERROR_CHECK(jpeg_new_encoder(&enc_config, &s_fb.jpeg_handle));

    return ESP_OK;
}
#endif

static int camera_open(int port)
{
    int ret;
    char name[16];
    int fd = -1;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ESP_ERROR_CHECK(esp_camera_init(&s_cam_config));

    ret = snprintf(name, sizeof(name), "/dev/video%d", port);
    if (ret <= 0) {
        return ESP_FAIL;
    }

    fd = open(name, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Open video %s fail", name);
        return -1;
    }

    format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_G_FMT fail");
        goto errout_get_fmt;
    }

#ifdef CONFIG_UVC_CAM_ENABLE_DVP_JPEG_SW_CODEC
    s_fb.width = format.fmt.pix.width;
    s_fb.height = format.fmt.pix.height;
    s_fb.pixel_format = format.fmt.pix.pixelformat;
#else
    if ((format.fmt.pix.height != HEIGHT) || (format.fmt.pix.width != WIDTH)) {
        format.fmt.pix.height = HEIGHT;
        format.fmt.pix.width = WIDTH;
        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            // Maybe some sensor does not support VIDIOC_S_FMT
            ESP_LOGE(TAG, "VIDIOC_S_FMT fail");
        }
    }
#endif

    memset(&req, 0, sizeof(req));
    req.count  = ARRAY_SIZE(buffer);
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "VIDIOC_REQBUFS fail");
        goto errout_get_fmt;
    }

    for (int i = 0; i < ARRAY_SIZE(buffer); i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type        = type;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QUERYBUF fail");
            goto errout_get_fmt;
        }

        buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, buf.m.offset);
        if (!buffer[i]) {
            ESP_LOGE(TAG, "mmap fail");
            goto errout_get_fmt;
        }
    }

    return fd;

errout_get_fmt:
    close(fd);

    return -1;
}
static void count_timer_cb( TimerHandle_t xTimer )
{
    ESP_LOGI(TAG, "frame_count=%d", frame_count);
    frame_count = 0;
}

void app_main(void)
{
    uint8_t *uvc_buffer = NULL;

    int fd = camera_open(0);
    if (fd < 0) {
        ESP_LOGE(TAG, "Open camera fail");;
    }

#ifndef CONFIG_UVC_CAM_ENABLE_DVP_JPEG_SW_CODEC
    jepg_encoder_init();
#endif

    ESP_ERROR_CHECK(esp_dma_calloc(1, UVC_MAX_FRAMESIZE_SIZE, ESP_DMA_MALLOC_FLAG_PSRAM, (void *)&s_fb.jpeg_out_buf, NULL));
    ESP_ERROR_CHECK(esp_dma_calloc(1, UVC_MAX_FRAMESIZE_SIZE, ESP_DMA_MALLOC_FLAG_PSRAM, (void *)&uvc_buffer, NULL));

    uvc_device_config_t config = {
        .uvc_buffer = uvc_buffer,
        .uvc_buffer_size = UVC_MAX_FRAMESIZE_SIZE,
        .start_cb = camera_start_cb,
        .fb_get_cb = camera_fb_get_cb,
        .fb_return_cb = camera_fb_return_cb,
        .stop_cb = camera_stop_cb,
        .cb_ctx = (void *)fd,
    };

    ESP_LOGI(TAG, "Format List");
    ESP_LOGI(TAG, "\tFormat(1) = %s", "MJPEG");
    ESP_LOGI(TAG, "Frame List");
    ESP_LOGI(TAG, "\tFrame(1) = %d * %d @%dfps", UVC_FRAMES_INFO[0].width, UVC_FRAMES_INFO[0].height, UVC_FRAMES_INFO[0].rate);

    timer = xTimerCreate("Timer", 1000 / portTICK_PERIOD_MS, pdTRUE, NULL, count_timer_cb);
    xTimerStart(timer, 0 );

    if (fd >= 0) {
        ESP_ERROR_CHECK(uvc_device_init(&config));
    }
}
