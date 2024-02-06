/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "linux/videodev2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "protocol_examples_common.h"
#include "esp_video.h"
#ifdef CONFIG_ESP_VIDEO_SW_CODEC
#include "img_converters.h"
#endif

#define PART_BOUNDARY           "123456789000000000000987654321"

typedef struct web_cam {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint8_t *buffer[CONFIG_HTTP_WEB_CAM_FRAME_BUFFER_COUNT];
} web_cam_t;

static const char *TAG = "web_cam";
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

static int s_jpeg_fd;
static uint8_t *s_jpeg_out_buf;
static uint8_t *s_jpeg_cap_buf;

#if CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM > 0
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
#endif

#if CONFIG_HTTP_WEB_CAM_ENABLE_MIPI_CSI && CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM > 0
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

#if CONFIG_HTTP_WEB_CAM_ENABLE_DVP && CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 0
static const esp_camera_dvp_config_t s_dvp_config[CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM] = {
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

#if CONFIG_HTTP_WEB_CAM_ENABLE_SIM
const esp_camera_sim_config_t s_sim_config[] = {
    {
        .id = 0,
    },
#if CONFIG_HTTP_WEB_CAM_SIM_COUNT > 1
    {
        .id = 1,
    }
#endif
};
#endif

static const esp_camera_config_t s_cam_config = {
    .sccb_num = CONFIG_ESP_VIDEO_CAMERA_SCCB_NUM,
    .sccb     = s_sccb_config,
#if CONFIG_HTTP_WEB_CAM_ENABLE_MIPI_CSI && CONFIG_ESP_VIDEO_CAMERA_INTF_CSI_NUM > 0
    .csi      = s_csi_config,
#endif
    .dvp_num  = CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM,
#if CONFIG_HTTP_WEB_CAM_ENABLE_DVP && CONFIG_ESP_VIDEO_CAMERA_INTF_DVP_NUM > 0
    .dvp      = s_dvp_config,
#endif
#if CONFIG_HTTP_WEB_CAM_ENABLE_SIM
    .sim_num  = CONFIG_HTTP_WEB_CAM_SIM_COUNT,
    .sim = s_sim_config,
#endif
};

static esp_err_t stream_handler(httpd_req_t *req)
{
    int ret;
    esp_err_t res;
    int index = 0;
    int size = 0;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    web_cam_t *wc = (web_cam_t *)req->user_ctx;

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "10");

    if (ioctl(wc->fd, VIDIOC_STREAMON, &type)) {
        ESP_LOGE(TAG, "failed to start stream");
        return ESP_FAIL;
    }

    while (1) {
        struct timespec ts;
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type   = type;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(wc->fd, VIDIOC_DQBUF, &buf);
        if (ret != 0) {
            ESP_LOGE(TAG, "failed to receive video frame");
            break;
        }

        res = clock_gettime (CLOCK_MONOTONIC, &ts);
        if (res != 0) {
            ESP_LOGE(TAG, "failed to get time");
        }

        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res == ESP_OK) {
            bool tx_valid;
            uint8_t *jpeg_ptr;
            size_t jpeg_size;

            if (wc->pixel_format == V4L2_PIX_FMT_JPEG) {
                jpeg_ptr = wc->buffer[buf.index];
                jpeg_size = buf.bytesused;
                tx_valid = true;
            }
#ifdef CONFIG_ESP_VIDEO_SW_CODEC
            else if (wc->pixel_format == V4L2_PIX_FMT_RGB565) {
                struct v4l2_buffer jpeg_buf;

                memcpy(s_jpeg_out_buf, wc->buffer[buf.index], buf.bytesused);

                memset(&jpeg_buf, 0, sizeof(jpeg_buf));
                jpeg_buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
                jpeg_buf.memory = V4L2_MEMORY_MMAP;
                jpeg_buf.index  = 0;
                ret = ioctl(s_jpeg_fd, VIDIOC_QBUF, &jpeg_buf);
                assert(ret == 0);

                memset(&jpeg_buf, 0, sizeof(jpeg_buf));
                jpeg_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                jpeg_buf.memory = V4L2_MEMORY_MMAP;
                jpeg_buf.index  = 0;
                ret = ioctl(s_jpeg_fd, VIDIOC_QBUF, &jpeg_buf);
                assert(ret == 0);

                memset(&jpeg_buf, 0, sizeof(jpeg_buf));
                jpeg_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                jpeg_buf.memory = V4L2_MEMORY_MMAP;
                jpeg_buf.index  = 0;
                ret = ioctl(s_jpeg_fd, VIDIOC_DQBUF, &jpeg_buf);
                assert(ret == 0);

                jpeg_ptr = s_jpeg_cap_buf;
                jpeg_size = jpeg_buf.bytesused;
                tx_valid = true;

                memset(&jpeg_buf, 0, sizeof(jpeg_buf));
                jpeg_buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
                jpeg_buf.memory = V4L2_MEMORY_MMAP;
                jpeg_buf.index  = 0;
                ret = ioctl(s_jpeg_fd, VIDIOC_DQBUF, &jpeg_buf);
                assert(ret == 0);
            }
#endif
            else {
                jpeg_ptr = NULL;
                jpeg_size = 0;
                tx_valid = false;
            }

            if (tx_valid) {
                int hlen;
                char part_buf[128];

                hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, jpeg_size, ts.tv_sec, ts.tv_nsec);
                res = httpd_resp_send_chunk(req, part_buf, hlen);
                if (res == ESP_OK) {
                    res = httpd_resp_send_chunk(req, (char *)jpeg_ptr, jpeg_size);
                    if (res == ESP_OK) {
                        size = (int)jpeg_size;
                    }
                }
            }

            if (jpeg_ptr != wc->buffer[buf.index]) {
                // free(jpeg_ptr);
            }
        }

        if (ioctl(wc->fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to free video frame");
        }

        if (res == ESP_OK) {
            ESP_LOGI(TAG, "Send picture index %d size=%d", index++, size);
        } else {
            ESP_LOGE(TAG, "Break stream handler");
            break;
        }
    }

    if (ioctl(wc->fd, VIDIOC_STREAMOFF, &type)) {
        res = ESP_FAIL;
        ESP_LOGE(TAG, "failed to stop stream");
    }

    return res;
}

#define JPEG_DEVICE_NAME "/dev/video1"

static esp_err_t jpeg_open(uint32_t width, uint32_t height, uint32_t out_format)
{
    int ret;
    int fd;
    struct v4l2_buffer buf;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;

    if (!width || !height) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_format == V4L2_PIX_FMT_JPEG) {
        ESP_LOGI(TAG, "Camera sensor capture stream format is JPEG, so not need to encode it");
        return ESP_OK;
    }

    fd = open(JPEG_DEVICE_NAME, O_RDONLY);
    if (fd < 0) {
        ret = ESP_ERR_NOT_FOUND;
        goto errout_open_dev;
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    if (fd < 0) {
        ret = ESP_FAIL;
        goto errout_set_format;
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
        ret = ESP_FAIL;
        goto errout_set_format;
    }

    memset(&req, 0, sizeof(req));
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_MMAP;
    req.count  = 1;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ret = ESP_FAIL;
        goto errout_set_format;
    }

    memset(&req, 0, sizeof(req));
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count  = 1;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ret = ESP_FAIL;
        goto errout_set_format;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
        ret = ESP_FAIL;
        goto errout_set_format;
    }

    s_jpeg_out_buf = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd, buf.m.offset);
    if (!s_jpeg_out_buf) {
        ret = ESP_FAIL;
        goto errout_set_format;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
        ret = ESP_FAIL;
        goto errout_set_format;
    }

    s_jpeg_cap_buf = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd, buf.m.offset);
    if (!s_jpeg_cap_buf) {
        ret = ESP_FAIL;
        goto errout_set_format;
    }

    s_jpeg_fd = fd;

    return ESP_OK;

errout_set_format:
    close(fd);
errout_open_dev:
    return ret;
}

static esp_err_t camera_open(int port, web_cam_t **o_wc)
{
    int ret;
    char name[16];
    web_cam_t *wc;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = snprintf(name, sizeof(name), "/dev/video%d", port);
    if (ret <= 0) {
        return ESP_FAIL;
    }

    wc = malloc(sizeof(web_cam_t));
    if (!wc) {
        return ESP_ERR_NO_MEM;
    }

    wc->fd = open(name, O_RDONLY);
    if (wc->fd < 0) {
        ret = ESP_ERR_NOT_FOUND;
        goto errout_open_dev;
    }

    format.type = type;
    if (ioctl(wc->fd, VIDIOC_G_FMT, &format) != 0) {
        ret = ESP_FAIL;
        goto errout_get_fmt;
    }

    wc->width = format.fmt.pix.width;
    wc->height = format.fmt.pix.height;
    wc->pixel_format = format.fmt.pix.pixelformat;
#ifndef CONFIG_ESP_VIDEO_SW_CODEC
    if (wc->pixel_format != V4L2_PIX_FMT_JPEG) {
        ESP_LOGE(TAG, "detect camera sensor output frame format is not JPEG, please select option ESP_VIDEO_SW_CODEC enable software JPEG codec");
        ret = ESP_FAIL;
        goto errout_get_fmt;
    }
#endif

    memset(&req, 0, sizeof(req));
    req.count  = ARRAY_SIZE(wc->buffer);
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(wc->fd, VIDIOC_REQBUFS, &req) != 0) {
        ret = ESP_FAIL;
        goto errout_get_fmt;
    }

    for (int i = 0; i < ARRAY_SIZE(wc->buffer); i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type        = type;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        if (ioctl(wc->fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ret = ESP_FAIL;
            goto errout_get_fmt;
        }

        wc->buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, wc->fd, buf.m.offset);
        if (!wc->buffer[i]) {
            ret = ESP_FAIL;
            goto errout_get_fmt;
        }

        if (ioctl(wc->fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            ret = ESP_FAIL;
            goto errout_get_fmt;
        }
    }

    ret = jpeg_open(wc->width, wc->height, wc->pixel_format);
    if (ret != ESP_OK) {
        ret = ESP_FAIL;
        goto errout_get_fmt;
    }

    *o_wc = wc;

    return ESP_OK;

errout_get_fmt:
    close(wc->fd);
errout_open_dev:
    free(wc);
    return ret;
}

static esp_err_t http_server_init(int index, web_cam_t *web_cam)
{
    esp_err_t ret;
    httpd_handle_t stream_httpd;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = (void *)web_cam
    };

    config.stack_size = 5120;
    config.server_port += index;
    config.ctrl_port += index;

    ret = httpd_start(&stream_httpd, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ret;
    }

    ret = httpd_register_uri_handler(stream_httpd, &stream_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add handler to HTTP server");
        return ret;
    }

    ESP_LOGI(TAG, "Starting stream HTTP server on port: '%d'", config.server_port);

    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    ESP_ERROR_CHECK(esp_camera_init(&s_cam_config));

    for (int i = 0; i < CONFIG_HTTP_WEB_CAM_COUNT; i++) {
        esp_err_t ret;
        web_cam_t *web_cam;

        ret = camera_open(i, &web_cam);
        if (ret == ESP_OK) {
            ESP_ERROR_CHECK(http_server_init(i, web_cam));
        }
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "Not found /dev/video%d", i);
            break;
        } else {
            ESP_ERROR_CHECK(ret);
        }
    }
}
