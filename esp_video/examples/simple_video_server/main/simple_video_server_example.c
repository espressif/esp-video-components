/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "sdkconfig.h"
#include "protocol_examples_common.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "example_video_common.h"

// video frame buffer count, too large value may cause memory allocation fails.
#define EXAMPLE_VIDEO_BUFFER_COUNT   2

#define MEMORY_TYPE                  V4L2_MEMORY_MMAP

#define JPEG_ENC_QUALITY             (80)
#define PART_BOUNDARY                "123456789000000000000987654321"
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#define EXAMPLE_MDNS_INSTANCE "simple video web"
#define EXAMPLE_MDNS_HOST_NAME "esp-web"

/*
 * Web cam control structure
*/
typedef struct web_cam {
    int fd;
    bool is_jpeg;

    example_encoder_handle_t encoder_handle;
    uint8_t *jpeg_out_buf;
    uint32_t jpeg_out_size;

    uint8_t *buffer[EXAMPLE_VIDEO_BUFFER_COUNT];
} web_cam_t;

static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

static uint16_t server_port_offset;
static const char *TAG = "example";

static esp_err_t record_bin_handler(httpd_req_t *req)
{
    esp_err_t res = ESP_FAIL;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_buffer buf;
    web_cam_t *wc = (web_cam_t *)req->user_ctx;

    memset(&buf, 0, sizeof(buf));
    buf.type   = type;
    buf.memory = MEMORY_TYPE;
    res = ioctl(wc->fd, VIDIOC_DQBUF, &buf);
    if (res != 0) {
        return res;
    }

    if (buf.flags & V4L2_BUF_FLAG_DONE) {
        httpd_resp_set_type(req, "application/octet-stream");
        httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=record.bin"); // default name is record.bin
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

        res = httpd_resp_send_chunk(req, (const char *)wc->buffer[buf.index], buf.bytesused);
        if (res != ESP_OK) {
            ESP_LOGW(TAG, "chunk send failed");
        }

        /* Respond with an empty chunk to signal HTTP response completion */
        httpd_resp_send_chunk(req, NULL, 0);
    } else {
        ESP_LOGD(TAG, "buffer is invalid");
    }

    if (ioctl(wc->fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to free video frame");
    }

    return res;
}

/* Note that opening this stream will block the use of other handlers.
You can access other handlers normally only after closing the stream.*/
static esp_err_t stream_handler(httpd_req_t *req)
{
    esp_err_t res = ESP_FAIL;
    struct v4l2_buffer buf;
    uint8_t *jpeg_ptr = NULL;
    size_t jpeg_size = 0;
    bool tx_valid = false;
    uint32_t jpeg_encoded_size = 0;
    web_cam_t *wc = (web_cam_t *)req->user_ctx;

    ESP_ERROR_CHECK(httpd_resp_set_type(req, STREAM_CONTENT_TYPE));

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "10");

    while (1) {
        struct timespec ts = {0};
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = MEMORY_TYPE;

        res = ioctl(wc->fd, VIDIOC_DQBUF, &buf);
        if (res != 0) {
            ESP_LOGE(TAG, "failed to receive video frame");
            break;
        }

        if (buf.flags & V4L2_BUF_FLAG_DONE) {
            res = clock_gettime (CLOCK_MONOTONIC, &ts);
            if (res != 0) {
                ESP_LOGE(TAG, "failed to get time");
            }

            res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Boundary sending failed!");
                if (ioctl(wc->fd, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "failed to free fb");
                }
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send Boundary");
                break;
            }

            if (wc->is_jpeg) {
                jpeg_ptr = wc->buffer[buf.index];
                jpeg_size = buf.bytesused;
                tx_valid = true;
            } else {
                res = example_encoder_process(wc->encoder_handle, wc->buffer[buf.index], buf.bytesused, wc->jpeg_out_buf, wc->jpeg_out_size, &jpeg_encoded_size);
                if (res == ESP_OK) {
                    jpeg_ptr = wc->jpeg_out_buf;
                    jpeg_size = jpeg_encoded_size;
                    tx_valid = true;
                    ESP_LOGD(TAG, "jpeg size = %d", jpeg_size);
                }
            }

            if (tx_valid) {
                int hlen;
                char part_buf[128];

                hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, jpeg_size, ts.tv_sec, ts.tv_nsec);
                res = httpd_resp_send_chunk(req, part_buf, hlen);
                if (res == ESP_OK) {
                    res = httpd_resp_send_chunk(req, (char *)jpeg_ptr, jpeg_size);
                }
            }
        } else {
            ESP_LOGD(TAG, "buffer is invalid");
        }

        if (ioctl(wc->fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to free video frame");
        }

        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Break stream handler");
            break;
        }
    }

    return res;
}

static esp_err_t pic_handler(httpd_req_t *req)
{
    esp_err_t res = ESP_FAIL;
    struct v4l2_buffer buf;
    uint8_t *jpeg_ptr = NULL;
    size_t jpeg_size = 0;
    bool tx_valid = false;
    uint32_t jpeg_encoded_size = 0;
    web_cam_t *wc = (web_cam_t *)req->user_ctx;

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = MEMORY_TYPE;
    res = ioctl(wc->fd, VIDIOC_DQBUF, &buf);
    if (res != 0) {
        return res;
    }

    if (buf.flags & V4L2_BUF_FLAG_DONE) {
        if (wc->is_jpeg) {
            jpeg_ptr = wc->buffer[buf.index];
            jpeg_size = buf.bytesused;
            tx_valid = true;
        } else {
            res = example_encoder_process(wc->encoder_handle, wc->buffer[buf.index], buf.bytesused, wc->jpeg_out_buf, wc->jpeg_out_size, &jpeg_encoded_size);
            if (res == ESP_OK) {
                jpeg_ptr = wc->jpeg_out_buf;
                jpeg_size = jpeg_encoded_size;
                tx_valid = true;
                ESP_LOGD(TAG, "jpeg size = %d", jpeg_size);
            } else {
                ESP_LOGE(TAG, "jpeg encode failed");
            }
        }

        if (tx_valid) {
            res = httpd_resp_send_chunk(req, (const char *)jpeg_ptr, jpeg_size);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "send chunk failed");
            }
        }

        /* Respond with an empty chunk to signal HTTP response completion */
        httpd_resp_send_chunk(req, NULL, 0);
    } else {
        ESP_LOGD(TAG, "buffer is invalid");
    }

    if (ioctl(wc->fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to free video frame");
    }

    return res;
}

static esp_err_t new_web_cam(const char *dev, web_cam_t **ret_wc)
{
    int ret = ESP_FAIL;
    struct v4l2_capability capability;
    struct v4l2_format format;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_requestbuffers req;
    web_cam_t *wc;
    example_encoder_config_t encoder_config = {0};
    example_encoder_handle_t encoder_handle;

    int fd = open(dev, O_RDWR);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_FAIL, TAG, "Open video device %s failed", dev);

    ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_QUERYCAP, &capability), fail0, TAG, "failed to get capability");
    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);

    memset(&req, 0, sizeof(req));
    req.count  = EXAMPLE_VIDEO_BUFFER_COUNT;
    req.type   = type;
    req.memory = MEMORY_TYPE;
    ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_REQBUFS, &req), fail0, TAG, "failed to req buffers");

    wc = malloc(sizeof(web_cam_t));
    ESP_GOTO_ON_FALSE(wc, ESP_ERR_NO_MEM, fail0, TAG, "failed to alloc web cam");
    wc->fd = fd;

    for (int i = 0; i < EXAMPLE_VIDEO_BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type        = type;
        buf.memory      = MEMORY_TYPE;
        buf.index       = i;
        ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_QUERYBUF, &buf), fail1, TAG, "failed to query buffer");

        wc->buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, buf.m.offset);
        ESP_GOTO_ON_FALSE(wc->buffer[i], ESP_FAIL, fail1, TAG, "failed to map buffer");

        ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_QBUF, &buf), fail1, TAG, "failed to queue frame buffer");
    }

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_G_FMT, &format), fail1, TAG, "Failed get fmt");

    ESP_LOGI(TAG, "width=%" PRIu32 " height=%" PRIu32 " format=" V4L2_FMT_STR, format.fmt.pix.width,
             format.fmt.pix.height, V4L2_FMT_STR_ARG(format.fmt.pix.pixelformat));

    wc->is_jpeg = (format.fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG);

    if (!wc->is_jpeg) {
        encoder_config.width = format.fmt.pix.width;
        encoder_config.height = format.fmt.pix.height;
        encoder_config.pixel_format = format.fmt.pix.pixelformat;
        encoder_config.quality = JPEG_ENC_QUALITY;
        ESP_GOTO_ON_ERROR(example_encoder_init(&encoder_config, &encoder_handle), fail1, TAG, "failed to init encoder");

        wc->encoder_handle = encoder_handle;

        ESP_GOTO_ON_ERROR(example_encoder_alloc_output_buffer(encoder_handle, &wc->jpeg_out_buf, &wc->jpeg_out_size), fail2, TAG, "failed to alloc jpeg output buf");
    }

    ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_STREAMON, &type), fail3, TAG, "failed to start stream");


    *ret_wc = wc;

    return ESP_OK;

fail3:
    if (!wc->is_jpeg) {
        example_encoder_free_output_buffer(encoder_handle, wc->jpeg_out_buf);
    }
fail2:
    if (!wc->is_jpeg) {
        example_encoder_deinit(encoder_handle);
    }
fail1:
    free(wc);
fail0:
    close(fd);
    return ret;
}

static esp_err_t http_server_init(int index, web_cam_t *web_cam)
{
    esp_err_t ret;
    httpd_handle_t camera_httpd;
    httpd_handle_t stream_httpd;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    server_port_offset += index;
    config.stack_size = 1024 * 6;
    config.server_port += server_port_offset;
    config.ctrl_port += server_port_offset;

    httpd_uri_t pic_get_uri = {
        .uri = "/pic",
        .method = HTTP_GET,
        .handler = pic_handler,
        .user_ctx = (void *)web_cam
    };
    httpd_uri_t record_file_get_uri = {
        .uri = "/record",
        .method = HTTP_GET,
        .handler = record_bin_handler,
        .user_ctx = (void *)web_cam,
    };
    httpd_uri_t stream_get_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = (void *)web_cam
    };

    ret = httpd_start(&camera_httpd, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ret;
    } else {
        ESP_LOGI(TAG, "Starting camera HTTP server on port: '%d'", config.server_port);
        ESP_ERROR_CHECK(httpd_register_uri_handler(camera_httpd, &pic_get_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(camera_httpd, &record_file_get_uri));
    }

    server_port_offset++;
    config.server_port += 1;
    config.ctrl_port += 1;
    ESP_LOGI(TAG, "Starting stream server on port: '%d'", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_get_uri);
    }

    return ESP_OK;
}

/**
 * @brief   Build a web server with `cam_fd` as the image data source.
 * @param index The index number of the web server.
 * It is allowed to establish multiple servers, and its data port and control port are the default port + index
 * @param cam_fd Cam device descriptor.
 *
 * @return
 *     - ESP_OK   Success
 *     - Others error
 */
static esp_err_t start_cam_web_server(int index, const char *dev)
{
    web_cam_t *web_cam;
    esp_err_t ret = new_web_cam(dev, &web_cam);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to new web cam");
        return ret;
    }
    return http_server_init(index, web_cam);
}

static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set(EXAMPLE_MDNS_HOST_NAME);
    mdns_instance_name_set(EXAMPLE_MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", CONFIG_IDF_TARGET},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name(EXAMPLE_MDNS_HOST_NAME);

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    ESP_ERROR_CHECK(example_video_init());

    ESP_ERROR_CHECK(start_cam_web_server(0, EXAMPLE_CAM_DEV_PATH));
    ESP_LOGI(TAG, "Example Start");
}
