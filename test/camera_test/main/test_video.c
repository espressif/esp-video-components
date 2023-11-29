/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "linux/videodev2.h"
#include "unity.h"
// #include "memory_checks.h"
// #include "unity_test_utils_memory.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_video.h"

#define VIDEO_COUNT             CONFIG_SIMULATED_INTF_DEVICE_COUNT
#define VIDEO_DEVICE_NAME       CONFIG_SIMULATED_INTF_DEVICE_NAME
#define VIDEO_BUFFER_NUM        CONFIG_SIMULATED_INTF_DEVICE_BUFFER_COUNT

#define VIDEO_DESC_BUFFER_SIZE  128
#define VIDEO_LINUX_CAPS        (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE)

#define TASK_STACK_SIZE         8192
#define TASK_PRIORITY           2

static bool s_inited;
static SemaphoreHandle_t s_done_sem[VIDEO_COUNT];

#undef TEST_ESP_OK
#define TEST_ESP_OK(ret) printf("%s %d line ret=%d\r\n", __func__, __LINE__, ret)


void esp32p4_hw_init(void);

static void init(void)
{
    if (!s_inited) {
        TEST_ESP_OK(esp_video_init());
        // esp32p4_hw_init();
        for (int i = 0; i < VIDEO_COUNT; i++) {
            s_done_sem[i] = xSemaphoreCreateBinary();
            TEST_ASSERT_NOT_NULL(s_done_sem[i]);
        }
        s_inited = true;
    }
}

static bool camera_test_fps(int fd, uint16_t times)
{
    float fps = 0.0f;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    uint32_t num = 0;
    int ret;
    TickType_t tick = xTaskGetTickCount();
    for (size_t i = 0; i < times; i++) {
        struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type   = type;
		buf.memory = V4L2_MEMORY_MMAP;
        // printf("%s %d line\r\n", __func__, __LINE__);
		ret = ioctl(fd, VIDIOC_DQBUF, &buf);
		TEST_ESP_OK(ret);
		ret = ioctl(fd, VIDIOC_QBUF, &buf);
		TEST_ESP_OK(ret);
        num++;
    }
    tick = xTaskGetTickCount() - tick;
    if (num) {
        fps = num * 1000000.0f / tick ;
    }
    printf("[fps]=%5.2f\n", fps);
    return 1;
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
    printf("fd=%d\r\n", fd);

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

        // TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_SIZE, buf.length);

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
    // camera_test_fps(fd, 90);
    while (1) {
        struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.type   = type;
		buf.memory = V4L2_MEMORY_MMAP;
        // printf("%s %d line\r\n", __func__, __LINE__);
		ret = ioctl(fd, VIDIOC_DQBUF, &buf);
		TEST_ESP_OK(ret);
            
        // TEST_ASSERT_EQUAL_INT(VIDEO_BUFFER_SIZE, buf.bytesused);
        // TEST_ASSERT_EQUAL_MEMORY(VIDEO_BUFFER_DATA, video_buffer_ptr[buf.index], buf.bytesused);
        // memset(video_buffer_ptr[buf.index], 0, buf.bytesused);
        printf("buf.bytesused=%d, %d\r\n", buf.bytesused, buf.m.offset);
        printf("data:");
        for (uint32_t loop = buf.m.offset; loop < buf.m.offset + 16; loop++) {
            printf("%02x ", video_buffer_ptr[buf.index][loop]);
        }
         printf("\r\n");

        // printf("\r\ndata2:\r\n");
        // for (uint32_t loop = buf.bytesused - 16; loop < buf.bytesused; loop++) {
        //     printf("%02x ", video_buffer_ptr[buf.index][loop]);
        // }
        // printf("\r\n");
        memset(video_buffer_ptr[buf.index], 0, buf.bytesused);
		ret = ioctl(fd, VIDIOC_QBUF, &buf);
		TEST_ESP_OK(ret);

        count++;
        // sleep(1);
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

void app_main()
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
        printf("create task=%s\n", name);
        free(name);
    }
}
