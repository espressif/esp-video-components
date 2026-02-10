/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/* DESCRIPTION:
 * This example contains code to make ESP32 based device recognizable by USB-hosts as a USB Mass Storage Device.
 * It either allows the embedded application i.e. example to access the partition or Host PC accesses the partition over USB MSC.
 * They can't be allowed to access the partition at the same time.
 * For different scenarios and behaviour, Refer to README of this example.
 */

#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "example_video_common.h"

#if CONFIG_EXAMPLE_FORMAT_MJPEG
#define ENCODE_DEV_PATH        ESP_VIDEO_JPEG_DEVICE_NAME
#define STORAGE_IMAGE_FORMAT   V4L2_PIX_FMT_JPEG
#elif CONFIG_EXAMPLE_FORMAT_H264
#if CONFIG_EXAMPLE_H264_MAX_QP <= CONFIG_EXAMPLE_H264_MIN_QP
#error "CONFIG_EXAMPLE_H264_MAX_QP should larger than CONFIG_EXAMPLE_H264_MIN_QP"
#endif

#define ENCODE_DEV_PATH            ESP_VIDEO_H264_DEVICE_NAME
#define STORAGE_IMAGE_FORMAT       V4L2_PIX_FMT_H264
#define CAPTURE_SECONDS            3
#elif CONFIG_EXAMPLE_FORMAT_NON_ENCODE
#define STORAGE_IMAGE_FORMAT       V4L2_PIX_FMT_UYVY
#endif

#define VIDEO_BUFFER_COUNT         2
#define VIDEO_ENCODER_BUFFER_COUNT 1
#define SKIP_STARTUP_FRAME_COUNT   CONFIG_EXAMPLE_SKIP_STARTUP_FRAME_COUNT

#if CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
#define BASE_PATH                  CONFIG_EXAMPLE_SPI_FLASH_MOUNT_POINT
#else
#define BASE_PATH                  CONFIG_EXAMPLE_SDMMC_MOUNT_POINT
#endif

/**
 * @brief The format of the stored image data
 */
typedef enum {
    MSC_IMG_FORMAT_JPEG,            /*!< JPEG format */
    MSC_IMG_FORMAT_H264,            /*!< H264 format */
    MSC_IMG_FORMAT_NON_ENCODE,      /*!< Non-encoded format */
} usb_msc_image_format_t;

/**
 * @brief The framebuffer type
 */
typedef struct usb_msc_fb {
    uint8_t *buf;
    uint8_t buf_index;
    size_t buf_bytesused;
    usb_msc_image_format_t fmt; /*!< Data format of stored images */
    size_t width;               /*!< Width of the image frame in pixels */
    size_t height;              /*!< Height of the image frame in pixels */
    struct timeval timestamp;   /*!< Timestamp since boot of the frame */
} usb_msc_fb_t;

typedef struct usb_msc_storage {
    int cap_fd;
    uint32_t format;
    uint8_t *cap_buffer[VIDEO_BUFFER_COUNT];
#if !CONFIG_EXAMPLE_FORMAT_NON_ENCODE
    int m2m_fd;
    uint8_t *m2m_cap_buffer;
#endif
    usb_msc_fb_t um_fb;
    uint64_t storage_max_capacity;

    example_storage_handle_t storage_handle;
} usb_msc_storage_t;

static const char *TAG = "example_main";
static usb_msc_storage_t msc_ctrl;

static esp_err_t init_capture_video(usb_msc_storage_t *umsc)
{
    int fd;

    fd = open(EXAMPLE_CAM_DEV_PATH, O_RDONLY);
    assert(fd >= 0);

    umsc->cap_fd = fd;
    umsc->format = STORAGE_IMAGE_FORMAT;

    return 0;
}

#if !CONFIG_EXAMPLE_FORMAT_NON_ENCODE
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

static esp_err_t init_codec_video(usb_msc_storage_t *umsc)
{
    int fd;
    const char *devpath = ENCODE_DEV_PATH;

    fd = open(devpath, O_RDONLY);
    assert(fd >= 0);

#if CONFIG_EXAMPLE_FORMAT_MJPEG
    set_codec_control(fd, V4L2_CID_JPEG_CLASS, V4L2_CID_JPEG_COMPRESSION_QUALITY, CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY);
#elif CONFIG_EXAMPLE_FORMAT_H264
    set_codec_control(fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, CONFIG_EXAMPLE_H264_I_PERIOD);
    set_codec_control(fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_BITRATE, CONFIG_EXAMPLE_H264_BITRATE);
    set_codec_control(fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_MIN_QP, CONFIG_EXAMPLE_H264_MIN_QP);
    set_codec_control(fd, V4L2_CID_CODEC_CLASS, V4L2_CID_MPEG_VIDEO_H264_MAX_QP, CONFIG_EXAMPLE_H264_MAX_QP);
#endif

    umsc->m2m_fd = fd;

    return 0;
}
#endif

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

/* mount the partition and show all the files in BASE_PATH */
static void example_print_files(void)
{
    /* List all the files in this directory */
    ESP_LOGI(TAG, "\nls command output:");
    struct dirent *d;
    DIR *dh = opendir(BASE_PATH);
    if (!dh) {
        if (errno == ENOENT) {
            /* If the directory is not found */
            ESP_LOGI(TAG, "Directory %s doesn't exist", BASE_PATH);
        } else {
            /* If the directory is not readable then throw error and exit */
            ESP_LOGE(TAG, "Unable to read directory %s", BASE_PATH);
        }
        return;
    }
    /* While the next entry is not readable we will print directory files */
    while ((d = readdir(dh)) != NULL) {
        printf("%s\n", d->d_name);
    }
    closedir(dh);
    return;
}

static esp_err_t example_video_start(usb_msc_storage_t *umsc)
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
    if (ioctl(umsc->cap_fd, VIDIOC_G_FMT, &init_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        return ESP_FAIL;
    }
    /* Use default length and width, refer to the sensor format options in menuconfig. */
    width = init_format.fmt.pix.width;
    height = init_format.fmt.pix.height;

    if (umsc->format == V4L2_PIX_FMT_JPEG) {
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

            if (ioctl(umsc->cap_fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
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
        umsc->um_fb.fmt = MSC_IMG_FORMAT_JPEG;
    } else if (umsc->format == V4L2_PIX_FMT_H264) {
        /* Todo, fix input format when h264 encoder supports other formats */
        capture_fmt = V4L2_PIX_FMT_YUV420;
        umsc->um_fb.fmt = MSC_IMG_FORMAT_H264;
    } else {
        /* Non-encode format */
        capture_fmt = umsc->format;
        umsc->um_fb.fmt = MSC_IMG_FORMAT_NON_ENCODE;
    }

    /* Configure camera interface capture stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = capture_fmt;
    ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = VIDEO_BUFFER_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_REQBUFS, &req));

    for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        ESP_ERROR_CHECK (ioctl(umsc->cap_fd, VIDIOC_QUERYBUF, &buf));

        umsc->cap_buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                              MAP_SHARED, umsc->cap_fd, buf.m.offset);
        assert(umsc->cap_buffer[i]);

        ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_QBUF, &buf));
    }

#if !CONFIG_EXAMPLE_FORMAT_NON_ENCODE
    /* Configure codec output stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = capture_fmt;
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = VIDEO_ENCODER_BUFFER_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_REQBUFS, &req));

    /* Configure codec capture stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = umsc->format;
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_REQBUFS, &req));

    memset(&buf, 0, sizeof(buf));
    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = 0;
    ESP_ERROR_CHECK (ioctl(umsc->m2m_fd, VIDIOC_QUERYBUF, &buf));

    umsc->m2m_cap_buffer = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                           MAP_SHARED, umsc->m2m_fd, buf.m.offset);
    assert(umsc->m2m_cap_buffer);

    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_QBUF, &buf));

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_STREAMON, &type));
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_STREAMON, &type));
#endif
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_STREAMON, &type));

    /* Skip the first few frames of the image to get a stable image. */
    for (int i = 0; i < SKIP_STARTUP_FRAME_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_DQBUF, &buf));
        ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_QBUF, &buf));
    }

    /* Init frame buffer's basic info. */
    umsc->um_fb.width = width;
    umsc->um_fb.height = height;

    return ESP_OK;
}

static void example_video_stop(usb_msc_storage_t *umsc)
{
    int type;

    ESP_LOGD(TAG, "Video stop");

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(umsc->cap_fd, VIDIOC_STREAMOFF, &type);
#if !CONFIG_EXAMPLE_FORMAT_NON_ENCODE
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ioctl(umsc->m2m_fd, VIDIOC_STREAMOFF, &type);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(umsc->m2m_fd, VIDIOC_STREAMOFF, &type);
#endif
}

static usb_msc_fb_t *example_video_fb_get(usb_msc_storage_t *umsc)
{
    struct v4l2_buffer cap_buf;
#if !CONFIG_EXAMPLE_FORMAT_NON_ENCODE
    struct v4l2_buffer m2m_out_buf;
    struct v4l2_buffer m2m_cap_buf;
#endif
    int64_t us;

    ESP_LOGD(TAG, "Video get");

    /*
     * V4L2 works in either “capture” (frames being read off a device e.g. camera or webcam) or “output” (frames being sent to a device e.g. a graphics card)
     * M2M combines “capture” and “output” into a single mode which allows arbitrary transformations of frames.
     * The data flow can be described as follows:
     *
     *****************               ********       *************
     * Capture Device* ------------> * V4L2 *-----> * Program   *
     * (e.g. camera) *               *      *       *           *
     *****************               ********       *************
     *
     ************************        ********       *************
     * Output Device        * <----- * V4L2 *-----> * Program   *
     * (e.g. graphics card) *        *      *       *           *
     ************************        ********       *************
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
    ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_DQBUF, &cap_buf));
#if !CONFIG_EXAMPLE_FORMAT_NON_ENCODE
    memset(&m2m_out_buf, 0, sizeof(m2m_out_buf));
    m2m_out_buf.index  = 0;
    m2m_out_buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    m2m_out_buf.memory = V4L2_MEMORY_USERPTR;
    m2m_out_buf.m.userptr = (unsigned long)umsc->cap_buffer[cap_buf.index];
    m2m_out_buf.length = cap_buf.bytesused;
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_QBUF, &m2m_out_buf));

    memset(&m2m_cap_buf, 0, sizeof(m2m_cap_buf));
    m2m_cap_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_DQBUF, &m2m_cap_buf));

    ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_QBUF, &cap_buf));
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_DQBUF, &m2m_out_buf));
    /* Notes: If multiple m2m buffers are used, 'm2m_cap_buf.index' can be used to get the actual address of buf.
    Here, only one m2m buffer is used, so the address of the m2m buffer is directly referenced.*/
    umsc->um_fb.buf = umsc->m2m_cap_buffer;
    umsc->um_fb.buf_bytesused = m2m_cap_buf.bytesused;
    umsc->um_fb.buf_index = m2m_cap_buf.index;
#else
    umsc->um_fb.buf = umsc->cap_buffer[cap_buf.index];
    umsc->um_fb.buf_bytesused = cap_buf.bytesused;
    umsc->um_fb.buf_index = cap_buf.index;
#endif
    us = esp_timer_get_time();
    umsc->um_fb.timestamp.tv_sec = us / 1000000UL;;
    umsc->um_fb.timestamp.tv_usec = us % 1000000UL;

    return &umsc->um_fb;
}

static void example_video_fb_return(usb_msc_storage_t *umsc)
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.index  = umsc->um_fb.buf_index;
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    ESP_LOGD(TAG, "Video return");
#if !CONFIG_EXAMPLE_FORMAT_NON_ENCODE
    ESP_ERROR_CHECK(ioctl(umsc->m2m_fd, VIDIOC_QBUF, &buf));
#else
    ESP_ERROR_CHECK(ioctl(umsc->cap_fd, VIDIOC_QBUF, &buf));
#endif
}

static esp_err_t store_image_to_fat(usb_msc_storage_t *umsc)
{
    bool is_in_use = false;

    ESP_ERROR_CHECK(example_msc_storage_in_use_by_usb_host(umsc->storage_handle, &is_in_use));
    if (is_in_use) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't write to storage.");
        return ESP_ERR_INVALID_STATE;
    }

    example_print_files();

    ESP_LOGD(TAG, "write to storage:");
    char string[32] = {0};
    char file_name_str[48] = {0};
    int current_time_us;
    struct timeval current_time;
    usb_msc_fb_t *image_fb = NULL;
    FILE *f = NULL;
    struct stat st;

    ESP_ERROR_CHECK(example_video_start(umsc));
    current_time_us = esp_timer_get_time();
    current_time.tv_sec = current_time_us / 1000000UL;;
    current_time.tv_usec = current_time_us % 1000000UL;
    /* Generate a file name based on the current time */
    itoa((int)current_time.tv_sec, string, 10);
    strcat(strcpy(file_name_str, BASE_PATH"/"), string);
    memset(string, 0x0, sizeof(string));
    itoa((int)current_time.tv_usec, string, 10);
    strcat(file_name_str, "_");
    strcat(file_name_str, string);
#if CONFIG_EXAMPLE_FORMAT_MJPEG
    strcat(file_name_str, ".jpg");
#elif CONFIG_EXAMPLE_FORMAT_H264
    strcat(file_name_str, "_h264.bin");
#elif CONFIG_EXAMPLE_FORMAT_NON_ENCODE
    strcat(file_name_str, ".bin");
#endif

    ESP_LOGI(TAG, "file name:%s", file_name_str);

    if (stat(file_name_str, &st) == 0) {
        /* Delete it if it exists */
        ESP_LOGW(TAG, "Delete original file %s", file_name_str);
        unlink(file_name_str);
    }

#if (CONFIG_EXAMPLE_FORMAT_MJPEG || CONFIG_EXAMPLE_FORMAT_NON_ENCODE)
    f = fopen(file_name_str, "wb");
    if (f == NULL) {
        example_video_stop(umsc);
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    image_fb = example_video_fb_get(umsc);
    if (image_fb->buf_bytesused < umsc->storage_max_capacity) {
        example_write_file(f, image_fb->buf, image_fb->buf_bytesused);
    } else {
        ESP_LOGE(TAG, "Image size is too large, increase storage space");
    }

    example_video_fb_return(umsc);
    fclose(f);
#elif CONFIG_EXAMPLE_FORMAT_H264
    f = fopen(file_name_str, "ab");
    if (f == NULL) {
        example_video_stop(umsc);
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    int64_t start_time_us = esp_timer_get_time();
    uint32_t bytes_written = 0;
    while (esp_timer_get_time() - start_time_us < (CAPTURE_SECONDS * 1000 * 1000)) {
        image_fb = example_video_fb_get(umsc);
        if ((bytes_written += image_fb->buf_bytesused) < umsc->storage_max_capacity) {
            example_write_file(f, image_fb->buf, image_fb->buf_bytesused);
        } else {
            ESP_LOGE(TAG, "Image size is too large, increase storage space");
        }

        example_video_fb_return(umsc);
    }
    fclose(f);
#endif
    ESP_LOGI(TAG, "File written");
    example_video_stop(umsc);

    return ESP_OK;
}

esp_err_t init_usb_msc(usb_msc_storage_t *umsc)
{
    esp_err_t ret = ESP_OK;

#if CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
    ret = example_mount_msc_to_spiflash(&umsc->storage_handle);
#else
    ret = example_mount_msc_to_mmc(&umsc->storage_handle);
#endif
    if (ret == ESP_OK) {
        ret = example_storage_get_capacity(umsc->storage_handle, &umsc->storage_max_capacity);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Storage capacity: %llu bytes", umsc->storage_max_capacity);
        }
    }

    return ret;
}

void app_main(void)
{
    ESP_ERROR_CHECK(example_video_init());
    ESP_ERROR_CHECK(init_capture_video(&msc_ctrl));
#if !CONFIG_EXAMPLE_FORMAT_NON_ENCODE
    ESP_ERROR_CHECK(init_codec_video(&msc_ctrl));
#endif
    ESP_ERROR_CHECK(init_usb_msc(&msc_ctrl));
    ESP_ERROR_CHECK(store_image_to_fat(&msc_ctrl));
}
