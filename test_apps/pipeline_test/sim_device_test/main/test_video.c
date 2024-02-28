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
#ifdef CONFIG_HEAP_TRACING
#include "esp_heap_trace.h"
#endif

#include "memory_checks.h"
#include "unity_test_utils_memory.h"
#include "unity.h"

#include "linux/videodev2.h"
#include "esp_video.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-100)

void setUp(void);

#ifdef CONFIG_SIMULATED_INTF
#define VIDEO_DEVICE_NAME       "SIM"
#define VIDEO_BUFFER_NUM        CONFIG_SIMULATED_INTF_DEVICE_BUFFER_COUNT
#define VIDEO_BUFFER_SIZE       sim_picture_jpeg_len
#define VIDEO_BUFFER_DATA       sim_picture_jpeg
#define VIDEO_DESC_BUFFER_SIZE  128
#define VIDEO_LINUX_CAPS        (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_DEVICE_CAPS)
#define VIDEO_CAM_WIDTH         200
#define VIDEO_CAM_HEIGHT        200
#define VIDEO_CAM_FORMAT        V4L2_PIX_FMT_JPEG
#define VIDEO_CAM_PIXEL_SIZE    1

#define TASK_STACK_SIZE         8192
#define TASK_PRIORITY           2

#ifdef CONFIG_HEAP_TRACING
#define HEAP_RES_NUM            8
static heap_trace_record_t recs[HEAP_RES_NUM];
#endif

static bool s_inited;
static SemaphoreHandle_t s_done_sem;

extern const unsigned int sim_picture_jpeg_len;
extern const unsigned char sim_picture_jpeg[];

static void init(void)
{
    if (!s_inited) {
        esp_camera_sim_config_t sim_camera_config = {
            .id = 0
        };
        esp_camera_config_t config = {
            .sccb_num = 0,
            .sim_num  = 1,
            .sim      = &sim_camera_config
        };

        TEST_ESP_OK(esp_camera_init(&config));

        s_done_sem = xSemaphoreCreateBinary();
        TEST_ASSERT_NOT_NULL(s_done_sem);

        setUp();
        s_inited = true;
    }
}

static void test_linux_posix_with_v4l2_operation_task(void *p)
{
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
    struct v4l2_format format;
    uint8_t *video_buffer_ptr[VIDEO_BUFFER_NUM];

    printf("task=%s starts\n", pcTaskGetName(NULL));

    ret = asprintf(&name, "/dev/video0");
    TEST_ASSERT_GREATER_THAN(0, ret);

    fd = open(name, O_RDONLY);
    free(name);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    memset(&cap, 0, sizeof(cap));
    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    TEST_ESP_OK(ret);

    /* Set format before setting up buffer */

    format.type = type;
    format.fmt.pix.width = VIDEO_CAM_WIDTH;
    format.fmt.pix.height = VIDEO_CAM_HEIGHT;
    format.fmt.pix.pixelformat = VIDEO_CAM_FORMAT;
    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    TEST_ESP_OK(ret);

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

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
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

    xSemaphoreGive(s_done_sem);

    vTaskDelete(NULL);
}

TEST_CASE("Linux POSIX with V4L2 operation", "[video]")
{
    int ret;
    char *name;
    TaskHandle_t th;

    /* Initialize esp-video system */

    init();

#ifdef CONFIG_HEAP_TRACING
    heap_trace_init_standalone(recs, HEAP_RES_NUM);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif
    ret = asprintf(&name, "test_l4v2_video");
    TEST_ASSERT_GREATER_THAN(0, ret);
    printf("create task=%s\n", name);
    ret = xTaskCreate(test_linux_posix_with_v4l2_operation_task,
                      name,
                      TASK_STACK_SIZE,
                      NULL,
                      TASK_PRIORITY,
                      &th);
    TEST_ASSERT_EQUAL_INT(pdPASS, ret);
    free(name);

    ret = xSemaphoreTake(s_done_sem, portMAX_DELAY);
    TEST_ASSERT_EQUAL_INT(pdPASS, ret);

#ifdef CONFIG_HEAP_TRACING
    heap_trace_stop();
    heap_trace_dump();
#endif
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
    /* some FreeRTOS stuff is cleaned up by idle task */
    vTaskDelay(5);

    /* clean up some of the newlib's lazy allocations */
    esp_reent_cleanup();

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
