/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "esp_heap_caps.h"

#include "memory_checks.h"
#include "unity_test_utils_memory.h"
#include "unity.h"

#include "linux/videodev2.h"
#include "esp_video.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-100)

#ifdef CONFIG_SIMULATED_INTF
#define VIDEO_COUNT             2
#define VIDEO_DEVICE_NAME       CONFIG_CAMERA_SIM_NAME
#define VIDEO_BUFFER_NUM        CONFIG_SIMULATED_INTF_DEVICE_BUFFER_COUNT
#define VIDEO_BUFFER_SIZE       sim_picture_jpeg_len
#define VIDEO_BUFFER_DATA       sim_picture_jpeg
#define VIDEO_DESC_BUFFER_SIZE  128
#define VIDEO_LINUX_CAPS        (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE)

#define TASK_STACK_SIZE         8192
#define TASK_PRIORITY           2

static bool s_inited;
static SemaphoreHandle_t s_done_sem[VIDEO_COUNT];

extern const unsigned int sim_picture_jpeg_len;
extern const unsigned char sim_picture_jpeg[];

static void init(void)
{
    if (!s_inited) {
        esp_camera_sim_config_t sim_camera_config[VIDEO_COUNT] = {
            {
                .id = 0
            }
#if VIDEO_COUNT > 1
            ,
            {
                .id = 1
            }
#endif
        };
        esp_camera_config_t config = {
            .sccb_num = 0,
            .dvp_num  = 0,
            .sim_num  = VIDEO_COUNT,
            .sim      = sim_camera_config
        };

        TEST_ESP_OK(esp_camera_init(&config));

        for (int i = 0; i < VIDEO_COUNT; i++) {
            s_done_sem[i] = xSemaphoreCreateBinary();
            TEST_ASSERT_NOT_NULL(s_done_sem[i]);
        }

        unity_utils_record_free_mem();
        test_utils_record_free_mem();
        s_inited = true;
    }
}

static void test_video_basic_operation_task(void *p)
{
    struct esp_video *video = (struct esp_video *)p;
    struct esp_video_capability capability;
    struct esp_video_format fmt;
    int count;
    TickType_t tick;
    uint32_t recv_size;
    uint32_t offset;
    char buffer[VIDEO_DESC_BUFFER_SIZE];

    printf("task=%s starts\n", pcTaskGetName(NULL));

    TEST_ESP_OK(esp_video_get_capability(video, &capability));
    TEST_ASSERT_TRUE(capability.fmt_jpeg);

    TEST_ESP_OK(esp_video_get_description(video, buffer, VIDEO_DESC_BUFFER_SIZE));

    /* Setup by given parameters */

    TEST_ESP_OK(esp_video_setup_buffer(video, VIDEO_BUFFER_NUM));

    fmt.fps = 20;
    TEST_ESP_OK(esp_video_set_format(video, &fmt));
    memset(&fmt, 0, sizeof(fmt));
    TEST_ESP_OK(esp_video_get_format(video, &fmt));
    TEST_ASSERT_EQUAL_INT(20, fmt.fps);

    /* Test receiving picture from video device in default FPS(20)  */

    TEST_ESP_OK(esp_video_start_capture(video));

    count = 0;
    tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - tick < (1000 / portTICK_PERIOD_MS)) {
        uint8_t *buffer = esp_video_recv_buffer(video, &recv_size, &offset, 100);
        if (buffer) {
            TEST_ASSERT_EQUAL_MEMORY(VIDEO_BUFFER_DATA, buffer, VIDEO_BUFFER_SIZE);
            count++;
            esp_video_free_buffer(video, buffer);
            TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_SIZE, recv_size);
        } else {
            break;
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL(fmt.fps - 1, count);

    TEST_ESP_OK(esp_video_stop_capture(video));

    /* Test receiving picture from video device in FPS = 30 */

    fmt.fps = 30;
    TEST_ESP_OK(esp_video_set_format(video, &fmt));
    memset(&fmt, 0, sizeof(fmt));
    TEST_ESP_OK(esp_video_get_format(video, &fmt));
    TEST_ASSERT_EQUAL_INT(30, fmt.fps);

    TEST_ESP_OK(esp_video_start_capture(video));

    count = 0;
    tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - tick < (1000 / portTICK_PERIOD_MS)) {
        uint8_t *buffer = esp_video_recv_buffer(video, &recv_size, &offset, 100);
        if (buffer) {
            TEST_ASSERT_EQUAL_MEMORY(VIDEO_BUFFER_DATA, buffer, VIDEO_BUFFER_SIZE);
            count++;
            esp_video_free_buffer(video, buffer);
            TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_SIZE, recv_size);
        } else {
            break;
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL(fmt.fps - 1, count);

    TEST_ESP_OK(esp_video_stop_capture(video));

    TEST_ESP_OK(esp_video_close(video));

    printf("task=%s tests done\n", pcTaskGetName(NULL));

    xSemaphoreGive(s_done_sem[video->id]);

    vTaskDelete(NULL);
}

static void test_linux_posix_with_v4l2_operation_task(void *p)
{
    int index = (int)p;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int fps;
    int ret;
    int fd;
    char *name;
    int count;
    TickType_t tick;
    struct v4l2_requestbuffers req;
    struct v4l2_capability cap;
    struct v4l2_streamparm param;
    uint8_t *video_buffer_ptr[VIDEO_BUFFER_NUM];

    printf("task=%s starts\n", pcTaskGetName(NULL));

    ret = asprintf(&name, "/dev/video%d", index);
    TEST_ASSERT_GREATER_THAN(0, ret);

    fd = open(name, O_RDONLY);
    free(name);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&cap, 0, sizeof(cap));
    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    TEST_ESP_OK(ret);

    ret = asprintf(&name, "%s%d", VIDEO_DEVICE_NAME, index);
    TEST_ASSERT_GREATER_THAN(0, ret);

    TEST_ASSERT_EQUAL_UINT32(VIDEO_LINUX_CAPS, cap.capabilities);
    TEST_ASSERT_EQUAL_STRING(name, cap.driver);
    free(name);

    memset(&req, 0, sizeof(req));
    req.count  = VIDEO_BUFFER_NUM;
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < VIDEO_BUFFER_NUM; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type        = type;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_SIZE, buf.length);

        video_buffer_ptr[i] = mmap(NULL,
                                   buf.length,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED,
                                   fd,
                                   buf.m.offset);
        TEST_ASSERT_NOT_NULL(video_buffer_ptr[i]);
    }

    fps = 20;
    memset(&(param), 0, sizeof(param));
    param.type                                  = type;
    param.parm.capture.timeperframe.numerator   = 1;
    param.parm.capture.timeperframe.denominator = fps;
    ret = ioctl(fd, VIDIOC_S_PARM, &param);
    TEST_ESP_OK(ret);

    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    TEST_ESP_OK(ret);

    count = 0;
    tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - tick < (1000 / portTICK_PERIOD_MS)) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type   = type;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_DQBUF, &buf);
        TEST_ESP_OK(ret);

        TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_SIZE, buf.bytesused);
        TEST_ASSERT_EQUAL_MEMORY(VIDEO_BUFFER_DATA, video_buffer_ptr[buf.index], buf.bytesused);
        memset(video_buffer_ptr[buf.index], 0, buf.bytesused);

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);

        count++;
    }

    TEST_ASSERT_GREATER_OR_EQUAL(fps - 1, count);

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    TEST_ESP_OK(ret);

    ret = close(fd);
    TEST_ESP_OK(ret);

    printf("task=%s tests done\n", pcTaskGetName(NULL));

    xSemaphoreGive(s_done_sem[index]);

    vTaskDelete(NULL);
}

TEST_CASE("video basic operation", "[video]")
{
    int ret;
    char *name;
    TaskHandle_t th;
    struct esp_video *video;

    /* Initialize esp-video system */

    init();

    for (int i = 0; i < VIDEO_COUNT; i++) {
        ret = asprintf(&name, "%s%d", VIDEO_DEVICE_NAME, i);
        TEST_ASSERT_GREATER_THAN(0, ret);

        video = esp_video_open(name);
        TEST_ASSERT_NOT_NULL(video);

        ret = xTaskCreate(test_video_basic_operation_task,
                          name,
                          TASK_STACK_SIZE,
                          video,
                          TASK_PRIORITY,
                          &th);
        TEST_ASSERT_EQUAL_INT(pdPASS, ret);
        printf("create task=%s\n", name);
        free(name);
    }

    for (int i = 0; i < VIDEO_COUNT; i++) {
        ret = xSemaphoreTake(s_done_sem[i], portMAX_DELAY);
        TEST_ASSERT_EQUAL_INT(pdPASS, ret);
    }
}

TEST_CASE("Linux POSIX with V4L2 operation", "[video]")
{
    int ret;
    char *name;
    TaskHandle_t th;

    /* Initialize esp-video system */

    init();

    for (int i = 0; i < VIDEO_COUNT; i++) {
        ret = asprintf(&name, "test_l4v2_video%d", i);
        TEST_ASSERT_GREATER_THAN(0, ret);

        ret = xTaskCreate(test_linux_posix_with_v4l2_operation_task,
                          name,
                          TASK_STACK_SIZE,
                          (void *)i,
                          TASK_PRIORITY,
                          &th);
        TEST_ASSERT_EQUAL_INT(pdPASS, ret);
        printf("create task=%s\n", name);
        free(name);
    }

    for (int i = 0; i < VIDEO_COUNT; i++) {
        ret = xSemaphoreTake(s_done_sem[i], portMAX_DELAY);
        TEST_ASSERT_EQUAL_INT(pdPASS, ret);
    }
}

TEST_CASE("V4L2 external controller class set/get", "[video]")
{
    int fd;
    int ret;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    struct v4l2_query_ext_ctrl qctrl;
    const int _3a_lock_value = V4L2_LOCK_EXPOSURE | V4L2_LOCK_WHITE_BALANCE | V4L2_LOCK_FOCUS;
    const uint8_t sim_flash_led_dims[] = {
        V4L2_FLASH_LED_MODE_NONE,
        V4L2_FLASH_LED_MODE_FLASH,
        V4L2_FLASH_LED_MODE_TORCH
    };
    const int fps = (uint16_t)random() % 90 + 10;

    /* Initialize esp-video system */

    init();

    fd = open("/dev/video0", O_RDONLY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    qctrl.id = V4L2_CID_3A_LOCK;
    ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_CTRL_TYPE_BITMASK, qctrl.type);
    TEST_ASSERT_EQUAL_INT(0, qctrl.minimum);
    TEST_ASSERT_EQUAL_INT(0x7, qctrl.maximum);
    TEST_ASSERT_EQUAL_INT(1, qctrl.step);
    TEST_ASSERT_EQUAL_INT(0, qctrl.default_value);

    control[0].id       = V4L2_CID_3A_LOCK;
    control[0].value    = _3a_lock_value;
    controls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    controls.count      = 1;
    controls.controls   = control;
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls);
    TEST_ESP_OK(ret);

    control[0].id       = V4L2_CID_3A_LOCK;
    control[0].value    = 0;
    controls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    controls.count      = 1;
    controls.controls   = control;
    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(_3a_lock_value, control[0].value);

    qctrl.id = V4L2_CID_FLASH_LED_MODE;
    ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_CTRL_TYPE_INTEGER_MENU, qctrl.type);
    TEST_ASSERT_EQUAL_INT(1, qctrl.elem_size);
    TEST_ASSERT_EQUAL_INT(3, qctrl.elems);
    TEST_ASSERT_EQUAL_INT(3, qctrl.nr_of_dims);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_UINT8(sim_flash_led_dims[i], qctrl.dims[i]);
    }
    TEST_ASSERT_EQUAL_INT(V4L2_FLASH_LED_MODE_NONE, qctrl.default_value);

    control[0].id       = V4L2_CID_FLASH_LED_MODE;
    control[0].value    = V4L2_FLASH_LED_MODE_FLASH;
    controls.ctrl_class = V4L2_CTRL_CLASS_FLASH;
    controls.count      = 1;
    controls.controls   = control;
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls);
    TEST_ESP_OK(ret);

    control[0].id       = V4L2_CID_FLASH_LED_MODE;
    control[0].value    = 0;
    controls.ctrl_class = V4L2_CTRL_CLASS_FLASH;
    controls.count      = 1;
    controls.controls   = control;
    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_FLASH_LED_MODE_FLASH, control[0].value);

    qctrl.id = CAM_SENSOR_FPS;
    ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(V4L2_CTRL_TYPE_INTEGER, qctrl.type);
    TEST_ASSERT_EQUAL_INT(1, qctrl.minimum);
    TEST_ASSERT_EQUAL_INT(100, qctrl.maximum);
    TEST_ASSERT_EQUAL_INT(1, qctrl.step);
    TEST_ASSERT_EQUAL_INT(1, qctrl.default_value);

    control[0].id       = CAM_SENSOR_FPS;
    control[0].value    = fps;
    controls.ctrl_class = V4L2_CTRL_CLASS_PRIV;
    controls.count      = 1;
    controls.controls   = control;
    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls);
    TEST_ESP_OK(ret);

    control[0].id       = CAM_SENSOR_FPS;
    control[0].value    = 0;
    controls.ctrl_class = V4L2_CTRL_CLASS_PRIV;
    controls.count      = 1;
    controls.controls   = control;
    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL_INT(fps, control[0].value);

    ret = close(fd);
    TEST_ESP_OK(ret);
}

#endif /* CONFIG_SIMULATED_INTF */

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    /**
     * \ \     /_ _| __ \  ____|  _ \
     *  \ \   /   |  |   | __|   |   |
     *   \ \ /    |  |   | |     |   |
     *    \_/   ___|____/ _____|\___/
    */

    printf("\r\n");
    printf("\\ \\     /_ _| __ \\  ____|  _ \\  \r\n");
    printf(" \\ \\   /   |  |   | __|   |   |\r\n");
    printf("  \\ \\ /    |  |   | |     |   | \r\n");
    printf("   \\_/   ___|____/ _____|\\___/  \r\n");

    unity_run_menu();
}
