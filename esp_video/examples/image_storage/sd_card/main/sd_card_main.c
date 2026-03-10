/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "example_video_common.h"

#if CONFIG_EXAMPLE_FORMAT_MJPEG
#define ENCODE_DEV_PATH        ESP_VIDEO_JPEG_DEVICE_NAME
#define ENCODE_OUTPUT_FORMAT   V4L2_PIX_FMT_JPEG
#elif CONFIG_EXAMPLE_FORMAT_H264
#if CONFIG_EXAMPLE_H264_MAX_QP <= CONFIG_EXAMPLE_H264_MIN_QP
#error "CONFIG_EXAMPLE_H264_MAX_QP should larger than CONFIG_EXAMPLE_H264_MIN_QP"
#endif

#define ENCODE_DEV_PATH        ESP_VIDEO_H264_DEVICE_NAME
#define ENCODE_OUTPUT_FORMAT   V4L2_PIX_FMT_H264
#define CAPTURE_SECONDS 3
#endif

#define VIDEO_BUFFER_COUNT         2
#define VIDEO_ENCODER_BUFFER_COUNT 1
#define SKIP_STARTUP_FRAME_COUNT   2
#define EXAMPLE_MAX_CHAR_SIZE      64
#define MOUNT_POINT                CONFIG_EXAMPLE_SDMMC_MOUNT_POINT

/**
 * @brief SD card encoder image data format
 */
typedef enum {
    SD_IMG_FORMAT_JPEG,            /*!< JPEG format */
    SD_IMG_FORMAT_H264,            /*!< H264 format */
} sd_image_format_t;

/* SD Card framebuffer type */
typedef struct sd_card_fb {
    uint8_t *buf;
    size_t buf_bytesused;
    sd_image_format_t fmt;
    size_t width;               /*!< Width of the image frame in pixels */
    size_t height;              /*!< Height of the image frame in pixels */
    struct timeval timestamp;   /*!< Timestamp since boot of the frame */
} sd_card_fb_t;

typedef struct image_sd_card {
    int cap_fd;
    uint32_t format;
    uint8_t *cap_buffer[VIDEO_BUFFER_COUNT];

    int m2m_fd;
    uint8_t *m2m_cap_buffer;

    sd_card_fb_t sd_fb;

    example_storage_handle_t fatfs;
} image_sd_card_t;

static const char *TAG = "example";

static void print_video_device_info(const struct v4l2_capability *capability)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability->version >> 16),
             (uint8_t)(capability->version >> 8),
             (uint8_t)capability->version);
    ESP_LOGI(TAG, "driver:  %s", capability->driver);
    ESP_LOGI(TAG, "card:    %s", capability->card);
    ESP_LOGI(TAG, "bus:     %s", capability->bus_info);
    ESP_LOGI(TAG, "capabilities:");
    if (capability->capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
    }
    if (capability->capabilities & V4L2_CAP_READWRITE) {
        ESP_LOGI(TAG, "\tREADWRITE");
    }
    if (capability->capabilities & V4L2_CAP_ASYNCIO) {
        ESP_LOGI(TAG, "\tASYNCIO");
    }
    if (capability->capabilities & V4L2_CAP_STREAMING) {
        ESP_LOGI(TAG, "\tSTREAMING");
    }
    if (capability->capabilities & V4L2_CAP_META_OUTPUT) {
        ESP_LOGI(TAG, "\tMETA_OUTPUT");
    }
    if (capability->capabilities & V4L2_CAP_DEVICE_CAPS) {
        ESP_LOGI(TAG, "device capabilities:");
        if (capability->device_caps & V4L2_CAP_VIDEO_CAPTURE) {
            ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
        }
        if (capability->device_caps & V4L2_CAP_READWRITE) {
            ESP_LOGI(TAG, "\tREADWRITE");
        }
        if (capability->device_caps & V4L2_CAP_ASYNCIO) {
            ESP_LOGI(TAG, "\tASYNCIO");
        }
        if (capability->device_caps & V4L2_CAP_STREAMING) {
            ESP_LOGI(TAG, "\tSTREAMING");
        }
        if (capability->device_caps & V4L2_CAP_META_OUTPUT) {
            ESP_LOGI(TAG, "\tMETA_OUTPUT");
        }
    }
}

static esp_err_t init_capture_video(image_sd_card_t *sd)
{
    int fd;
    struct v4l2_capability capability;

    fd = open(EXAMPLE_CAM_DEV_PATH, O_RDONLY);
    assert(fd >= 0);

    ESP_ERROR_CHECK(ioctl(fd, VIDIOC_QUERYCAP, &capability));
    print_video_device_info(&capability);

    sd->cap_fd = fd;

    return 0;
}

static esp_err_t set_codec_control(int fd, uint32_t ctrl_class, uint32_t id, int32_t value)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    controls.ctrl_class = ctrl_class;
    controls.count = 1;
    controls.controls = control;
    control[0].id = id;
    control[0].value = value;

    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set control: %" PRIu32, id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t init_codec_video(image_sd_card_t *sd)
{
    int fd;
    const char *devpath = ENCODE_DEV_PATH;
    struct v4l2_capability capability;

    fd = open(devpath, O_RDONLY);
    assert(fd >= 0);

    ESP_ERROR_CHECK(ioctl(fd, VIDIOC_QUERYCAP, &capability));
    print_video_device_info(&capability);

#if CONFIG_EXAMPLE_FORMAT_MJPEG
    set_codec_control(fd, V4L2_CID_JPEG_CLASS, V4L2_CID_JPEG_COMPRESSION_QUALITY, CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY);
#elif CONFIG_EXAMPLE_FORMAT_H264
    set_codec_control(fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, CONFIG_EXAMPLE_H264_I_PERIOD);
    set_codec_control(fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_BITRATE, CONFIG_EXAMPLE_H264_BITRATE);
    set_codec_control(fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_MIN_QP, CONFIG_EXAMPLE_H264_MIN_QP);
    set_codec_control(fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_MAX_QP, CONFIG_EXAMPLE_H264_MAX_QP);
#endif

    sd->format = ENCODE_OUTPUT_FORMAT;
    sd->m2m_fd = fd;

    return 0;
}

static esp_err_t example_write_file(FILE *f, const uint8_t *data, size_t len)
{
    size_t written;

    do {
        written = fwrite(data, 1, len, f);
        len -= written;
        data += written;
    } while ( written && len );
    fflush(f);

    return ESP_OK;
}

static esp_err_t example_video_start(image_sd_card_t *sd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_buffer buf;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    struct v4l2_format init_format;
    uint32_t capture_fmt = 0;
    uint32_t width, height;

    ESP_LOGD(TAG, "Video start");

    memset(&init_format, 0, sizeof(struct v4l2_format));
    init_format.type = type;
    if (ioctl(sd->cap_fd, VIDIOC_G_FMT, &init_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        return ESP_FAIL;
    }
    /* Use default length and width, refer to the sensor format options in menuconfig. */
    width = init_format.fmt.pix.width;
    height = init_format.fmt.pix.height;

    if (sd->format == V4L2_PIX_FMT_JPEG) {
        int fmt_index = 0;
        const uint32_t jpeg_input_formats[] = {
            V4L2_PIX_FMT_RGB565,
            V4L2_PIX_FMT_UYVY,
            V4L2_PIX_FMT_RGB24,
            V4L2_PIX_FMT_GREY
        };
        int jpeg_input_formats_num = sizeof(jpeg_input_formats) / sizeof(jpeg_input_formats[0]);

        while (!capture_fmt) {
            struct v4l2_fmtdesc fmtdesc = {
                .index = fmt_index++,
                .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            };

            if (ioctl(sd->cap_fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
                break;
            }

            for (int i = 0; i < jpeg_input_formats_num; i++) {
                if (jpeg_input_formats[i] == fmtdesc.pixelformat) {
                    capture_fmt = jpeg_input_formats[i];
                    break;
                }
            }
        }

        if (!capture_fmt) {
            ESP_LOGI(TAG, "The camera sensor output pixel format is not supported by JPEG encoder");
            return ESP_ERR_NOT_SUPPORTED;
        }
    } else if (sd->format == V4L2_PIX_FMT_H264) {
        /* Todo, fix input format when h264 encoder supports other formats */
        capture_fmt = V4L2_PIX_FMT_YUV420;
    }

    /* Configure camera interface capture stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = capture_fmt;
    ESP_ERROR_CHECK(ioctl(sd->cap_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = VIDEO_BUFFER_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(sd->cap_fd, VIDIOC_REQBUFS, &req));

    for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        ESP_ERROR_CHECK (ioctl(sd->cap_fd, VIDIOC_QUERYBUF, &buf));

        sd->cap_buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                            MAP_SHARED, sd->cap_fd, buf.m.offset);
        assert(sd->cap_buffer[i]);

        ESP_ERROR_CHECK(ioctl(sd->cap_fd, VIDIOC_QBUF, &buf));
    }

    /* Configure codec output stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = capture_fmt;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = VIDEO_ENCODER_BUFFER_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_REQBUFS, &req));

    /* Configure codec capture stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = sd->format;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_REQBUFS, &req));

    memset(&buf, 0, sizeof(buf));
    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = 0;
    ESP_ERROR_CHECK (ioctl(sd->m2m_fd, VIDIOC_QUERYBUF, &buf));

    sd->m2m_cap_buffer = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                         MAP_SHARED, sd->m2m_fd, buf.m.offset);
    assert(sd->m2m_cap_buffer);

    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_QBUF, &buf));

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_STREAMON, &type));
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_STREAMON, &type));

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_ERROR_CHECK(ioctl(sd->cap_fd, VIDIOC_STREAMON, &type));

    /* Skip the first few frames of the image to get a stable image. */
    for (int i = 0; i < SKIP_STARTUP_FRAME_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ESP_ERROR_CHECK(ioctl(sd->cap_fd, VIDIOC_DQBUF, &buf));
        ESP_ERROR_CHECK(ioctl(sd->cap_fd, VIDIOC_QBUF, &buf));
    }

    /* Init sd encoder frame buffer's basic info. */
    sd->sd_fb.width = width;
    sd->sd_fb.height = height;
    sd->sd_fb.fmt = sd->format == V4L2_PIX_FMT_JPEG ? SD_IMG_FORMAT_JPEG : SD_IMG_FORMAT_H264;

    return ESP_OK;
}

static void example_video_stop(image_sd_card_t *sd)
{
    int type;

    ESP_LOGD(TAG, "Video stop");

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(sd->cap_fd, VIDIOC_STREAMOFF, &type);

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ioctl(sd->m2m_fd, VIDIOC_STREAMOFF, &type);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(sd->m2m_fd, VIDIOC_STREAMOFF, &type);
}

static sd_card_fb_t *example_video_fb_get(image_sd_card_t *sd)
{
    struct v4l2_buffer cap_buf;
    struct v4l2_buffer m2m_out_buf;
    struct v4l2_buffer m2m_cap_buf;
    int64_t us;

    ESP_LOGD(TAG, "Video get");
    /*
     * V4L2 M2M(Memory to Memory) Workflow:
     *
     **********************   RAW    ********       *************
     * Encoder            * <------- * V4L2 * <---- * Program   *
     * (e.g. jpeg, h.264) * -------> * M2M  * ----> *           *
     **********************  h264    ********       *************
     *
     *The process of obtaining the encoded data can be briefly described as follows:

     **********  DQBUF
     * cap_fd * -------------> Dequeue a filled (capturing) original image data buffer from the cap_fd’s receive queue.
     **********                                                                     |
     *                                                                              |
     *                                                                              v
     *                                                                      QBUF **********
     * Enqueue the original image buf to m2m_fd output buffer queue <----------- * m2m_fd *
     * |                                                                         **********
     * |
     * v
     **********  DQBUF
     * m2m_fd * -------> Dequeue a filled (capturing) buffer to get the encoded image data from m2m_fd's outgoing queue.
     **********
    */
    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(sd->cap_fd, VIDIOC_DQBUF, &cap_buf));

    memset(&m2m_out_buf, 0, sizeof(m2m_out_buf));
    m2m_out_buf.index  = 0;
    m2m_out_buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    m2m_out_buf.memory = V4L2_MEMORY_USERPTR;
    m2m_out_buf.m.userptr = (unsigned long)sd->cap_buffer[cap_buf.index];
    m2m_out_buf.length = cap_buf.bytesused;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_QBUF, &m2m_out_buf));

    memset(&m2m_cap_buf, 0, sizeof(m2m_cap_buf));
    m2m_cap_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_DQBUF, &m2m_cap_buf));

    ESP_ERROR_CHECK(ioctl(sd->cap_fd, VIDIOC_QBUF, &cap_buf));
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_DQBUF, &m2m_out_buf));
    /* Notes: If multiple m2m buffers are used, 'm2m_cap_buf.index' can be used to get the actual address of buf.
    Here, only one m2m buffer is used, so the address of the m2m buffer is directly referenced.*/
    sd->sd_fb.buf = sd->m2m_cap_buffer;
    sd->sd_fb.buf_bytesused = m2m_cap_buf.bytesused;
    us = esp_timer_get_time();
    sd->sd_fb.timestamp.tv_sec = us / 1000000UL;;
    sd->sd_fb.timestamp.tv_usec = us % 1000000UL;

    return &sd->sd_fb;
}

static void example_video_fb_return(image_sd_card_t *sd)
{
    struct v4l2_buffer m2m_cap_buf;

    ESP_LOGD(TAG, "Video return");

    m2m_cap_buf.index  = 0;
    m2m_cap_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(sd->m2m_fd, VIDIOC_QBUF, &m2m_cap_buf));
}

static esp_err_t store_image_to_sd_card(image_sd_card_t *sd)
{
    char string_usec[32] = {0};
    char string_sec[32] = {0};
    char file_name_str[128] = {0};
    int current_time_us;
    struct timeval current_time;
    sd_card_fb_t *image_fb = NULL;
    FILE *f = NULL;
    struct stat st;
#if CONFIG_EXAMPLE_FORMAT_MJPEG
    const char *file_extension = "jpg";
#elif CONFIG_EXAMPLE_FORMAT_H264
    const char *file_extension = "bin";
#endif

    ESP_ERROR_CHECK(example_video_start(sd));
    current_time_us = esp_timer_get_time();
    current_time.tv_sec = current_time_us / 1000000UL;;
    current_time.tv_usec = current_time_us % 1000000UL;
    /* Generate a file name based on the current time */
    memset(string_sec, 0x0, sizeof(string_sec));
    itoa((int)current_time.tv_sec, string_sec, 10);

    memset(string_usec, 0x0, sizeof(string_usec));
    itoa((int)current_time.tv_usec, string_usec, 10);

    int ret = snprintf(file_name_str, sizeof(file_name_str), "%s/%s_%s.%s", MOUNT_POINT, string_sec, string_usec, file_extension);
    if (ret < 0 || ret >= sizeof(file_name_str)) {
        ESP_LOGE(TAG, "File path too long or formatting error");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "file name:%s", file_name_str);

    if (stat(file_name_str, &st) == 0) {
        /* Delete it if it exists */
        ESP_LOGW(TAG, "Delete original file %s", file_name_str);
        unlink(file_name_str);
    }

#if CONFIG_EXAMPLE_FORMAT_MJPEG
    f = fopen(file_name_str, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    image_fb = example_video_fb_get(sd);
    example_write_file(f, image_fb->buf, image_fb->buf_bytesused);
    example_video_fb_return(sd);
    fclose(f);
#elif CONFIG_EXAMPLE_FORMAT_H264
    f = fopen(file_name_str, "ab");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    int64_t start_time_us = esp_timer_get_time();
    while (esp_timer_get_time() - start_time_us < (CAPTURE_SECONDS * 1000 * 1000)) {
        image_fb = example_video_fb_get(sd);
        example_write_file(f, image_fb->buf, image_fb->buf_bytesused);
        example_video_fb_return(sd);
    }
    fclose(f);
#endif
    ESP_LOGI(TAG, "File written");
    example_video_stop(sd);

    return ESP_OK;
}

static esp_err_t init_sd_card(image_sd_card_t *sd)
{
    return example_mount_fatfs_to_mmc(&sd->fatfs);
}

static esp_err_t deinit_sd_card(image_sd_card_t *sd)
{
    return example_unmount_fatfs_in_mmc(sd->fatfs);
}

void app_main(void)
{
    image_sd_card_t *sdc = calloc(1, sizeof(image_sd_card_t));
    assert(sdc);

    ESP_ERROR_CHECK(example_video_init());
    ESP_ERROR_CHECK(init_capture_video(sdc));
    ESP_ERROR_CHECK(init_codec_video(sdc));
    ESP_ERROR_CHECK(init_sd_card(sdc));
    ESP_ERROR_CHECK(store_image_to_sd_card(sdc));
    ESP_ERROR_CHECK(deinit_sd_card(sdc));
    free(sdc);
}
