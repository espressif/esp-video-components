/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "unity.h"
#include "esp_log_buffer.h"
#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_DEC_VIDEO_DEVICE
#include "driver/jpeg_decode.h"
#endif

#include "example_video_common.h"
#include "esp_video_ioctl.h"

#define VIDEO_BUFFER_NUM 2

#define TEST_APP_VIDEO_DEVICE EXAMPLE_CAM_DEV_PATH

#define HEAP_RECORD_NUM 32

#define TEST_JPEG_WIDTH      128
#define TEST_JPEG_HEIGHT     128

void setUp(void);

TEST_CASE("V4L2 init/deinit", "[video]")
{
    int count = 20;

    for (int i = 0; i < count; i++) {
        TEST_ESP_OK(example_video_init());

        int fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
        TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
        close(fd);

        TEST_ESP_OK(example_video_deinit());
    }
}

TEST_CASE("V4L2 Command", "[video]")
{
    int fd;
    int ret;
    uint16_t width;
    uint16_t height;
    uint32_t pixelformat;
    struct v4l2_format format;
    struct v4l2_capability cap;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    /* QUERYCAP */
    memset(&cap, 0, sizeof(cap));
    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_CAP_VIDEO_CAPTURE, cap.capabilities & V4L2_CAP_VIDEO_CAPTURE);

    /* G_FMT */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    TEST_ESP_OK(ret);

    width = format.fmt.pix.width;
    height = format.fmt.pix.height;

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
    pixelformat = V4L2_PIX_FMT_RGB565;
#else
    pixelformat = format.fmt.pix.pixelformat;
#endif

    /* S_FMT: valid */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = pixelformat;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ESP_OK(ret);

    /* S_FMT: invalid width */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width - 1;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = pixelformat;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    /* S_FMT: invalid height */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height - 1;
    format.fmt.pix.pixelformat = pixelformat;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    /* S_FMT: invalid pixelformat */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = 0xDEADBEEF;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("V4L2 query operations", "[video]")
{
    int fd;
    int ret;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    /* ENUM_FMT: index 0 should succeed */
    struct v4l2_fmtdesc fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;
    ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
    TEST_ESP_OK(ret);
    TEST_ASSERT_NOT_EQUAL(0, fmtdesc.pixelformat);

    /* ENUM_FMT: out-of-range index should fail */
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 255;
    ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    /* G_FMT to get current format for subsequent tests */
    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    TEST_ESP_OK(ret);

    /* ENUM_FRAMESIZES: index 0 with current format should succeed */
    struct v4l2_frmsizeenum frmsize;
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frmsize.index = 0;
    frmsize.pixel_format = format.fmt.pix.pixelformat;
    ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_FRMSIZE_TYPE_DISCRETE, frmsize.type);
    TEST_ASSERT_EQUAL_INT(format.fmt.pix.width, frmsize.discrete.width);
    TEST_ASSERT_EQUAL_INT(format.fmt.pix.height, frmsize.discrete.height);

    /* ENUM_FRAMESIZES: index 1 should fail */
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.index = 1;
    frmsize.pixel_format = format.fmt.pix.pixelformat;
    ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    /* G_SENSOR_FMT */
    esp_cam_sensor_format_t sensor_fmt;
    memset(&sensor_fmt, 0, sizeof(sensor_fmt));
    ret = ioctl(fd, VIDIOC_G_SENSOR_FMT, &sensor_fmt);
    TEST_ESP_OK(ret);
    TEST_ASSERT_GREATER_THAN(0, sensor_fmt.width);
    TEST_ASSERT_GREATER_THAN(0, sensor_fmt.height);

    /* G_PARM */
    struct v4l2_streamparm sparm;
    memset(&sparm, 0, sizeof(sparm));
    sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_G_PARM, &sparm);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_CAP_TIMEPERFRAME, sparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME);
    TEST_ASSERT_EQUAL_INT(1, sparm.parm.capture.timeperframe.numerator);
    TEST_ASSERT_GREATER_THAN(0, sparm.parm.capture.timeperframe.denominator);
    printf("FPS=%0.2f\n", (float)sparm.parm.capture.timeperframe.denominator / sparm.parm.capture.timeperframe.numerator);

    /* QUERY_EXT_CTRL + G_EXT_CTRLS + S_EXT_CTRLS */
    struct v4l2_query_ext_ctrl qctrl;
    memset(&qctrl, 0, sizeof(qctrl));
    qctrl.id = V4L2_CID_BRIGHTNESS;
    ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
    if (ret == 0) {
        printf("V4L2_CID_BRIGHTNESS: min=%" PRId64 " max=%" PRId64 " step=%" PRIu64 "\n",
               qctrl.minimum, qctrl.maximum, qctrl.step);

        struct v4l2_ext_control ctrl;
        struct v4l2_ext_controls ctrls;
        memset(&ctrl, 0, sizeof(ctrl));
        memset(&ctrls, 0, sizeof(ctrls));
        ctrl.id = V4L2_CID_BRIGHTNESS;
        ctrls.count = 1;
        ctrls.controls = &ctrl;
        ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
        TEST_ESP_OK(ret);
        printf("V4L2_CID_BRIGHTNESS value=%" PRId32 "\n", ctrl.value);

        ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
        TEST_ESP_OK(ret);
    }

    /* QUERYMENU */
    struct v4l2_querymenu qmenu;
    memset(&qmenu, 0, sizeof(qmenu));
    qmenu.id = V4L2_CID_BRIGHTNESS;
    qmenu.index = 0;
    ret = ioctl(fd, VIDIOC_QUERYMENU, &qmenu);
    /* May or may not be supported, both are valid */

    /* S_SENSOR_FMT: set current sensor format back */
    ret = ioctl(fd, VIDIOC_S_SENSOR_FMT, &sensor_fmt);
    TEST_ESP_OK(ret);

    /* SET_OWNER */
    int owner = 0;
    ret = ioctl(fd, VIDIOC_SET_OWNER, &owner);
    TEST_ESP_OK(ret);

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("V4L2 enum frame size and interval type", "[video]")
{
    int fd;
    int ret;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum frmsize;
    struct v4l2_frmivalenum frmival;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
    TEST_ESP_OK(ret);

    /* frmsize.type is an output field and must not be used as a V4L2 buffer type. */
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.index = 0;
    frmsize.pixel_format = fmtdesc.pixelformat;
    ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_FRMSIZE_TYPE_DISCRETE, frmsize.type);
    TEST_ASSERT_GREATER_THAN_UINT32(0, frmsize.discrete.width);
    TEST_ASSERT_GREATER_THAN_UINT32(0, frmsize.discrete.height);

    /* frmival.type is also an output field; devices without frame interval enumeration may reject this ioctl. */
    memset(&frmival, 0, sizeof(frmival));
    frmival.index = 0;
    frmival.pixel_format = fmtdesc.pixelformat;
    frmival.width = frmsize.discrete.width;
    frmival.height = frmsize.discrete.height;
    ret = ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);
    if (ret == 0) {
        switch (frmival.type) {
        case V4L2_FRMIVAL_TYPE_DISCRETE:
        case V4L2_FRMIVAL_TYPE_STEPWISE:
        case V4L2_FRMIVAL_TYPE_CONTINUOUS:
            break;
        default:
            TEST_FAIL_MESSAGE("invalid frame interval enum type");
        }
    }

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("V4L2 Video Buffer Sequence", "[video]")
{
    int fd;
    int ret;
    int val;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req;
    int buffer_count = 5;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&req, 0, sizeof(req));
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count  = buffer_count;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int test_count = 0; test_count < 4; test_count++) {
        for (int i = 0; i < buffer_count; i++) {
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
            TEST_ESP_OK(ret);

            ret = ioctl(fd, VIDIOC_QBUF, &buf);
            TEST_ESP_OK(ret);
        }

        val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(fd, VIDIOC_STREAMON, &val);
        TEST_ESP_OK(ret);

        for (int i = 0; i < buffer_count; i++) {
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            ret = ioctl(fd, VIDIOC_DQBUF, &buf);
            TEST_ESP_OK(ret);

            TEST_ASSERT_EQUAL_INT(i, buf.index);

            ret = ioctl(fd, VIDIOC_QBUF, &buf);
            TEST_ESP_OK(ret);
        }

        val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(fd, VIDIOC_STREAMOFF, &val);
        TEST_ESP_OK(ret);
    }

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}

#if CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE
TEST_CASE("V4L2 M2M device", "[video]")
{
    int fd;
    int ret;
    int val;
    uint16_t width = 320;
    uint16_t height = 240;
    struct v4l2_buffer buf;
    struct v4l2_format format;
    struct v4l2_capability cap;
    struct v4l2_requestbuffers req;
    uint8_t *out_buf[VIDEO_BUFFER_NUM];
    uint8_t *cap_buf[VIDEO_BUFFER_NUM];

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&cap, 0, sizeof(cap));
    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_CAP_VIDEO_M2M, cap.capabilities & V4L2_CAP_VIDEO_M2M);

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ESP_OK(ret);

    memset(&req, 0, sizeof(req));
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_MMAP;
    req.count  = VIDEO_BUFFER_NUM;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < VIDEO_BUFFER_NUM; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        TEST_ASSERT_EQUAL_INT(width * height * 2, buf.length);

        out_buf[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, buf.m.offset);
        TEST_ASSERT_NOT_NULL(out_buf[i]);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ESP_OK(ret);

    memset(&req, 0, sizeof(req));
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count  = VIDEO_BUFFER_NUM;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < VIDEO_BUFFER_NUM; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        cap_buf[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, buf.m.offset);
        TEST_ASSERT_NOT_NULL(cap_buf[i]);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    val = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(fd, VIDIOC_STREAMON, &val);
    TEST_ESP_OK(ret);

    val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMON, &val);
    TEST_ESP_OK(ret);

    for (int i = 0; i < 100; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_DQBUF, &buf);
        TEST_ESP_OK(ret);

        TEST_ASSERT_EQUAL_HEX8(0xff, cap_buf[buf.index][0]);
        TEST_ASSERT_EQUAL_HEX8(0xd8, cap_buf[buf.index][1]);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);

        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_DQBUF, &buf);
        TEST_ESP_OK(ret);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    val = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(fd, VIDIOC_STREAMOFF, &val);
    TEST_ESP_OK(ret);

    val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMOFF, &val);
    TEST_ESP_OK(ret);

    ret = close(fd);
    TEST_ESP_OK(ret);

    TEST_ESP_OK(example_video_deinit());
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE */

#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_DEC_VIDEO_DEVICE && CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE

static void fill_rgb_image(uint8_t *buf, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int offset = (y * width + x) * 3;
            buf[offset + 0] = (uint8_t)(x);   // R
            buf[offset + 1] = (uint8_t)(y);   // G
            buf[offset + 2] = (uint8_t)(x ^ y); // B
        }
    }
}

/**
 * Helper: Compare decoded data
 */
static bool compare_buffers(const uint8_t *buf1, const uint8_t *buf2, size_t len)
{
    return memcmp(buf1, buf2, len) == 0;
}

TEST_CASE("JPEG encoder and decoder consistency test", "[jpeg][video]")
{
    int ret;
    esp_err_t err;
    size_t frame_buf_sz = TEST_JPEG_WIDTH * TEST_JPEG_HEIGHT * 3;
    static const uint32_t s_test_decode_formats[] = {
        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_BGR24,
        V4L2_PIX_FMT_BGR565,
        V4L2_PIX_FMT_YUV420,
        V4L2_PIX_FMT_YUV444,
        V4L2_PIX_FMT_UYVY,
    };
    int test_decode_formats_num = ARRAY_SIZE(s_test_decode_formats);

    setUp();

    // Step 1. Initialize ESP-IDF JPEG decoder
    jpeg_decoder_handle_t jpeg_dec_handle = NULL;
    jpeg_decode_engine_cfg_t dev_cfg = {
        .intr_priority = 1,
        .timeout_ms = 100,
    };
    err = jpeg_new_decoder_engine(&dev_cfg, &jpeg_dec_handle);
    TEST_ASSERT_EQUAL_HEX8(ESP_OK, err);
    TEST_ASSERT_NOT_NULL(jpeg_dec_handle);

    // Step 2. Initialize ESP-VIDEO decoder
    esp_video_init_jpeg_dec_config_t jpeg_dec_config = {
        .dec_handle = jpeg_dec_handle,
    };
    esp_video_init_jpeg_enc_config_t jpeg_enc_config = {
        .enc_handle = NULL,
    };
    esp_video_init_config_t config = {
        .jpeg_dec = &jpeg_dec_config,
        .jpeg_enc = &jpeg_enc_config,
    };
    err = esp_video_init_with_flags(&config, ESP_VIDEO_INIT_FLAGS_JPEG_DEC | ESP_VIDEO_INIT_FLAGS_JPEG_ENC);
    TEST_ASSERT_EQUAL_HEX8(ESP_OK, err);

    // Step 3. Create V4L2 Encoder
    int fd_enc = open(ESP_VIDEO_JPEG_ENC_DEVICE_NAME, O_RDWR);  // Assume device path
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd_enc);

    // Step 4. Generate a raw RGB24 test image

    uint8_t *src_img = heap_caps_malloc(frame_buf_sz, MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_SPIRAM);
    TEST_ASSERT_NOT_NULL(src_img);

    fill_rgb_image(src_img, TEST_JPEG_WIDTH, TEST_JPEG_HEIGHT);

    // Step 5. Encode loop & comparison
    for (int fmt = 0; fmt < test_decode_formats_num; ++fmt) {
        uint32_t pixfmt = s_test_decode_formats[fmt];

        printf("==== Testing decode output pixel format: " V4L2_FMT_STR " ====\n", V4L2_FMT_STR_ARG(pixfmt));

        for (int qual = 10, i = 0; i < 10; ++i, qual += 5) {
            // Set encoder format
            struct v4l2_format enc_fmt = {0};
            enc_fmt.type           = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            enc_fmt.fmt.pix.width  = TEST_JPEG_WIDTH;
            enc_fmt.fmt.pix.height = TEST_JPEG_HEIGHT;
            enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

            switch (pixfmt) {
            case V4L2_PIX_FMT_YUV420:
                enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
                break;
            case V4L2_PIX_FMT_YUV444:
                enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV444;
                break;
            case V4L2_PIX_FMT_UYVY:
                enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
                break;
            default:
                break;
            }

            ret = ioctl(fd_enc, VIDIOC_S_FMT, &enc_fmt);
            TEST_ASSERT(ret == 0);

            memset(&enc_fmt, 0, sizeof(enc_fmt));
            enc_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
            enc_fmt.fmt.pix.width = TEST_JPEG_WIDTH;
            enc_fmt.fmt.pix.height = TEST_JPEG_HEIGHT;
            ret = ioctl(fd_enc, VIDIOC_S_FMT, &enc_fmt);
            TEST_ASSERT(ret == 0);

            // Set encoder quality
            struct v4l2_ext_control ext_ctrl = {0};
            struct v4l2_ext_controls ext_ctrls = {0};
            ext_ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
            ext_ctrl.value = qual;
            ext_ctrls.count = 1;
            ext_ctrls.controls = &ext_ctrl;
            ret = ioctl(fd_enc, VIDIOC_S_EXT_CTRLS, &ext_ctrls);
            TEST_ASSERT(ret == 0);

            // Request (allocate and queue) buffers for encoder input (output) and output (capture)
            struct v4l2_requestbuffers req;

            // Request 1 buffer for OUTPUT (input to encoder)
            memset(&req, 0, sizeof(req));
            req.count = 1;
            req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            req.memory = V4L2_MEMORY_USERPTR;
            ret = ioctl(fd_enc, VIDIOC_REQBUFS, &req);
            TEST_ASSERT(ret == 0);

            // Request 1 buffer for CAPTURE (output from encoder)
            memset(&req, 0, sizeof(req));
            req.count = 1;
            req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            req.memory = V4L2_MEMORY_USERPTR;
            ret = ioctl(fd_enc, VIDIOC_REQBUFS, &req);
            TEST_ASSERT(ret == 0);

            // Queue output buffer
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = 0;
            buf.length = frame_buf_sz;
            buf.m.userptr = (unsigned long)src_img;
            ret = ioctl(fd_enc, VIDIOC_QBUF, &buf);
            TEST_ASSERT(ret == 0);

            // Get encoded JPEG
            uint8_t *jpeg_buf = heap_caps_malloc(frame_buf_sz, MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_SPIRAM);
            TEST_ASSERT_NOT_NULL(jpeg_buf);

            // Queue capture buffer
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = 0;
            buf.length = frame_buf_sz;
            buf.m.userptr = (unsigned long)jpeg_buf;
            ret = ioctl(fd_enc, VIDIOC_QBUF, &buf);
            TEST_ASSERT(ret == 0);

            // Start stream
            int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            ret = ioctl(fd_enc, VIDIOC_STREAMON, &type);
            TEST_ASSERT(ret == 0);

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ret = ioctl(fd_enc, VIDIOC_STREAMON, &type);
            TEST_ASSERT(ret == 0);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = 0;
            ret = ioctl(fd_enc, VIDIOC_DQBUF, &buf);
            TEST_ASSERT(ret == 0);
            TEST_ASSERT_GREATER_THAN(0, buf.bytesused);
            TEST_ASSERT_EQUAL_HEX8(0xff, jpeg_buf[0]);
            TEST_ASSERT_EQUAL_HEX8(0xd8, jpeg_buf[1]);
            size_t jpeg_size = buf.bytesused;

            // Step 6. Decode with both decoders
            // (a) Decode directly with esp-idf decoder
            // Select decode parameters
            jpeg_decode_cfg_t dec_cfg = {
                .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
                .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB,
                .conv_std = COLOR_CONV_STD_RGB_YUV_BT601,
            };

            uint8_t *esp_dec_out = heap_caps_malloc(frame_buf_sz, MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_SPIRAM);
            TEST_ASSERT_NOT_NULL(esp_dec_out);
            uint32_t esp_dec_out_size = 0;

            // Set output_format
            switch (pixfmt) {
            case V4L2_PIX_FMT_RGB565:
                dec_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
                break;
            case V4L2_PIX_FMT_RGB24:
                dec_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB888;
                break;
            case V4L2_PIX_FMT_BGR24:
                dec_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB888;
                dec_cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
                break;
            case V4L2_PIX_FMT_BGR565:
                dec_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
                dec_cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
                break;
            case V4L2_PIX_FMT_YUV420:
                dec_cfg.output_format = JPEG_DECODE_OUT_FORMAT_YUV420;
                break;
            case V4L2_PIX_FMT_YUV444:
                dec_cfg.output_format = JPEG_DECODE_OUT_FORMAT_YUV444;
                break;
            case V4L2_PIX_FMT_UYVY:
                dec_cfg.output_format = JPEG_DECODE_OUT_FORMAT_YUV422;
                break;
            default:
                TEST_FAIL_MESSAGE("Unexpected pixel format");
            }

            err = jpeg_decoder_process(jpeg_dec_handle, &dec_cfg, jpeg_buf, jpeg_size, esp_dec_out, frame_buf_sz, &esp_dec_out_size);
            TEST_ASSERT_EQUAL_HEX8(ESP_OK, err);
            TEST_ASSERT_GREATER_THAN(0, esp_dec_out_size);

            // (b) Decode using V4L2 decoder interface
            int fd_dec = open(ESP_VIDEO_JPEG_DEC_DEVICE_NAME, O_RDWR); // Assume
            TEST_ASSERT_GREATER_OR_EQUAL(0, fd_dec);

            struct v4l2_format v4l2_fmt;

            memset(&v4l2_fmt, 0, sizeof(v4l2_fmt));
            v4l2_fmt.type           = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            v4l2_fmt.fmt.pix.width  = TEST_JPEG_WIDTH;
            v4l2_fmt.fmt.pix.height = TEST_JPEG_HEIGHT;
            v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
            ret = ioctl(fd_dec, VIDIOC_S_FMT, &v4l2_fmt);
            TEST_ASSERT(ret == 0);

            memset(&v4l2_fmt, 0, sizeof(v4l2_fmt));
            v4l2_fmt.type           = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2_fmt.fmt.pix.width  = TEST_JPEG_WIDTH;
            v4l2_fmt.fmt.pix.height = TEST_JPEG_HEIGHT;
            v4l2_fmt.fmt.pix.pixelformat = pixfmt;
            ret = ioctl(fd_dec, VIDIOC_S_FMT, &v4l2_fmt);
            TEST_ASSERT(ret == 0);

            // Request 1 buffer for OUTPUT (input to decoder)
            memset(&req, 0, sizeof(req));
            req.count = 1;
            req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            req.memory = V4L2_MEMORY_USERPTR;
            ret = ioctl(fd_dec, VIDIOC_REQBUFS, &req);
            TEST_ASSERT(ret == 0);

            // Request 1 buffer for CAPTURE (output from decoder)
            memset(&req, 0, sizeof(req));
            req.count = 1;
            req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            req.memory = V4L2_MEMORY_USERPTR;
            ret = ioctl(fd_dec, VIDIOC_REQBUFS, &req);
            TEST_ASSERT(ret == 0);

            // Queue output buffer
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = 0;
            buf.length = frame_buf_sz;
            buf.m.userptr = (unsigned long)jpeg_buf;
            ret = ioctl(fd_dec, VIDIOC_QBUF, &buf);
            TEST_ASSERT(ret == 0);

            uint8_t *v4l2_dec_out = heap_caps_malloc(frame_buf_sz, MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_SPIRAM);
            TEST_ASSERT_NOT_NULL(v4l2_dec_out);

            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = 0;
            buf.length = frame_buf_sz;
            buf.m.userptr = (unsigned long)v4l2_dec_out;
            ret = ioctl(fd_dec, VIDIOC_QBUF, &buf);
            TEST_ASSERT(ret == 0);

            int out_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            ret = ioctl(fd_dec, VIDIOC_STREAMON, &out_type);
            TEST_ASSERT(ret == 0);

            out_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ret = ioctl(fd_dec, VIDIOC_STREAMON, &out_type);
            TEST_ASSERT(ret == 0);

            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = 0;
            ret = ioctl(fd_dec, VIDIOC_DQBUF, &buf);
            TEST_ASSERT(ret == 0);
            TEST_ASSERT_GREATER_THAN(0, buf.bytesused);

            out_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            ret = ioctl(fd_dec, VIDIOC_STREAMOFF, &out_type);
            TEST_ASSERT(ret == 0);

            out_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ret = ioctl(fd_dec, VIDIOC_STREAMOFF, &out_type);
            TEST_ASSERT(ret == 0);

            // Step 7. Compare the output of both decoders
            // Note: RGB565 2 bytes/pix, RGB24 3 bytes/pix, GREY 1 byte/pix
            size_t cmp_len = 0;
            switch (pixfmt) {
            case V4L2_PIX_FMT_RGB565:
            case V4L2_PIX_FMT_BGR565:
                cmp_len = TEST_JPEG_WIDTH * TEST_JPEG_HEIGHT * 2;
                break;
            case V4L2_PIX_FMT_RGB24:
            case V4L2_PIX_FMT_BGR24:
                cmp_len = TEST_JPEG_WIDTH * TEST_JPEG_HEIGHT * 3;
                break;
            case V4L2_PIX_FMT_YUV420:
                cmp_len = TEST_JPEG_WIDTH * TEST_JPEG_HEIGHT * 3 / 2;
                break;
            case V4L2_PIX_FMT_YUV444:
                cmp_len = TEST_JPEG_WIDTH * TEST_JPEG_HEIGHT * 3;
                break;
            case V4L2_PIX_FMT_UYVY:
                cmp_len = TEST_JPEG_WIDTH * TEST_JPEG_HEIGHT * 2;
                break;
            }

            bool eq = compare_buffers(esp_dec_out, v4l2_dec_out, cmp_len);
            if (!eq) {
                printf("Decoder comparison failed: format=0x%" PRIx32 ", quality=%d\n", pixfmt, qual);
            }
            TEST_ASSERT_MESSAGE(eq, "esp-idf decoder output and v4l2 decoder output mismatch");

            // Stop encoder streams
            type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            ret = ioctl(fd_enc, VIDIOC_STREAMOFF, &type);
            TEST_ASSERT(ret == 0);

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ret = ioctl(fd_enc, VIDIOC_STREAMOFF, &type);
            TEST_ASSERT(ret == 0);

            // Close V4L2 decoder and free memory
            close(fd_dec);
            heap_caps_free(v4l2_dec_out);
            heap_caps_free(esp_dec_out);
            heap_caps_free(jpeg_buf);
        }
    }

    heap_caps_free(src_img);
    close(fd_enc);

    // Step 8. Clean up
    esp_video_deinit_with_flags(ESP_VIDEO_INIT_FLAGS_JPEG_DEC | ESP_VIDEO_INIT_FLAGS_JPEG_ENC);
    jpeg_del_decoder_engine(jpeg_dec_handle);
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_DEC_VIDEO_DEVICE && CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE */

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
TEST_CASE("V4L2 set/get selection", "[video]")
{
    int fd;
    struct v4l2_selection in_selection;
    struct v4l2_selection out_selection;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&in_selection, 0, sizeof(in_selection));
    in_selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    TEST_ASSERT_EQUAL_INT(-1, ioctl(fd, VIDIOC_G_SELECTION, &in_selection));

    memset(&in_selection, 0, sizeof(in_selection));
    in_selection.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    TEST_ASSERT_EQUAL_INT(-1, ioctl(fd, VIDIOC_S_SELECTION, &in_selection));

    memset(&in_selection, 0, sizeof(in_selection));
    in_selection.type = V4L2_BUF_TYPE_META_CAPTURE;
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_SELECTION, &in_selection));

    memset(&in_selection, 0, sizeof(in_selection));
    in_selection.type = V4L2_BUF_TYPE_META_CAPTURE;
    in_selection.r.left = 0;
    in_selection.r.width = 1080;
    in_selection.r.top = 0;
    in_selection.r.height = 720;
    TEST_ESP_OK(ioctl(fd, VIDIOC_S_SELECTION, &in_selection));

    memset(&out_selection, 0, sizeof(out_selection));
    out_selection.type = V4L2_BUF_TYPE_META_CAPTURE;
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_SELECTION, &out_selection));

    TEST_ASSERT_EQUAL_INT(0, memcmp(&out_selection, &in_selection, sizeof(out_selection)));

    TEST_ESP_OK(close(fd));

    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("V4L2 set/get param", "[video]")
{
    int fd;
    int fps;
    int ret;
    struct v4l2_buffer buf;
    struct v4l2_streamparm sparm;
    struct v4l2_captureparm *cparam = &sparm.parm.capture;
    struct v4l2_fract *timeperframe = &cparam->timeperframe;
    struct v4l2_requestbuffers req;
    int buf_count = 3;
    int capture_seconds = 3;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&sparm, 0, sizeof(sparm));
    sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_G_PARM, &sparm);
    TEST_ESP_OK(ret);

    TEST_ASSERT_EQUAL_INT(cparam->capability, V4L2_CAP_TIMEPERFRAME);
    TEST_ASSERT_GREATER_OR_EQUAL(1, timeperframe->numerator);
    TEST_ASSERT_GREATER_OR_EQUAL(1, timeperframe->denominator);
    printf("fps=%0.4f\n", (float)timeperframe->denominator / timeperframe->numerator);

    fps = timeperframe->denominator / timeperframe->numerator;
    for (int i = 2; i <= fps; i++) {
        if (fps % i != 0) {
            continue;
        }
        int div_fps = fps / i;

        printf("test fps=%d\n", div_fps);

        memset(&sparm, 0, sizeof(sparm));
        sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cparam->capability = V4L2_CAP_TIMEPERFRAME;
        timeperframe->numerator = 1;
        timeperframe->denominator = div_fps;
        ret = ioctl(fd, VIDIOC_S_PARM, &sparm);
        TEST_ESP_OK(ret);

        memset(&req, 0, sizeof(req));
        req.count  = buf_count;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_REQBUFS, &req);
        TEST_ESP_OK(ret);

        for (int j = 0; j < buf_count; j++) {
            memset(&buf, 0, sizeof(buf));
            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = j;
            ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
            TEST_ESP_OK(ret);

            ret = ioctl(fd, VIDIOC_QBUF, &buf);
            TEST_ESP_OK(ret);
        }

        int val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(fd, VIDIOC_STREAMON, &val);
        TEST_ESP_OK(ret);

        int frame_count = 0;
        int64_t start_time_us = esp_timer_get_time();
        while (esp_timer_get_time() - start_time_us < (capture_seconds * 1000 * 1000)) {
            memset(&buf, 0, sizeof(buf));
            buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            ret = ioctl(fd, VIDIOC_DQBUF, &buf);
            TEST_ESP_OK(ret);

            frame_count++;

            ret = ioctl(fd, VIDIOC_QBUF, &buf);
            TEST_ESP_OK(ret);
        }

        val = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(fd, VIDIOC_STREAMOFF, &val);
        TEST_ESP_OK(ret);

        TEST_ASSERT_EQUAL_INT(div_fps, frame_count / capture_seconds);

        memset(&sparm, 0, sizeof(sparm));
        sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(fd, VIDIOC_G_PARM, &sparm);
        TEST_ESP_OK(ret);

        TEST_ASSERT_EQUAL_INT(cparam->capability, V4L2_CAP_TIMEPERFRAME);
        TEST_ASSERT_EQUAL_INT(timeperframe->numerator, 1);
        TEST_ASSERT_EQUAL_INT(timeperframe->denominator, div_fps);
    }

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}
#endif /* CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE */

TEST_CASE("V4L2 set/get timeout", "[video]")
{
    int fd;
    struct timeval timeout;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req;
    int buf_count = 3;
    uint32_t os_ticks = 121;
    uint32_t timeout_ms = (os_ticks * 1000) / configTICK_RATE_HZ;
    int ret;

    setUp();

    TEST_ESP_OK(example_video_init());

    /* Test set/get DQBUF timeout */

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    ret = ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout);
    TEST_ESP_OK(ret);

    memset(&timeout, 0, sizeof(timeout));
    ret = ioctl(fd, VIDIOC_G_DQBUF_TIMEOUT, &timeout);
    TEST_ESP_OK(ret);

    TEST_ASSERT_EQUAL_INT(timeout_ms / 1000, timeout.tv_sec);
    TEST_ASSERT_EQUAL_INT((timeout_ms % 1000) * 1000, timeout.tv_usec);

    close(fd);

    /* Test DQBUF timeout */

    fd = open(TEST_APP_VIDEO_DEVICE, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    ret = ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout);
    TEST_ESP_OK(ret);

    memset(&req, 0, sizeof(req));
    req.count  = buf_count;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < buf_count; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    for (int i = 0; i < 10; i++) {
        TickType_t start_time = xTaskGetTickCount();

        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_DQBUF, &buf);
        TEST_ASSERT_EQUAL_INT(-1, ret);

        TickType_t end_time = xTaskGetTickCount();
        int ticks = end_time - start_time;
        TEST_ASSERT_GREATER_OR_EQUAL_INT(os_ticks - 1, ticks);
        TEST_ASSERT_LESS_OR_EQUAL_INT(os_ticks + 1, ticks);

        printf("DQBUF(%d) time: %d ticks\n", i, ticks);
    }

    close(fd);

    TEST_ESP_OK(example_video_deinit());
}
