/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_attr.h"
#include "unity.h"

#include "example_video_common.h"
#include "esp_video_caps.h"
#include "esp_video_ioctl.h"
#include "esp_cam_sensor_types.h"

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE && ESP_VIDEO_CSI_DRIVER_HAS_EVENT

#include "hal/mipi_csi_host_ll.h"

#define VIDEO_BUFFER_NUM                3
#define CAPTURE_FRAMES_BEFORE_ERROR     10
#define CAPTURE_FRAMES_AFTER_RESTART    10
#define EVENT_WAIT_TIMEOUT_MS           5000
#define DQBUF_TIMEOUT_MS                500
#define CAPTURE_TASK_STACK_SIZE         4096
#define EVENT_TASK_STACK_SIZE           4096
#define CAPTURE_TASK_PRIORITY           5
#define EVENT_TASK_PRIORITY             6

typedef struct {
    int fd;
    volatile bool capture_running;
    volatile int frames_received;
    volatile bool callback_triggered;
    volatile uint32_t callback_err_mask;
    volatile bool interrupt_disable_seen;
    volatile bool dqbuf_timed_out;
    SemaphoreHandle_t frames_ready_sem;
    SemaphoreHandle_t capture_exit_sem;
    SemaphoreHandle_t event_done_sem;
    SemaphoreHandle_t event_exit_sem;
} test_event_ctx_t;

void setUp(void);

static bool IRAM_ATTR test_csi_event_callback(void *user_data, uint32_t err_mask)
{
    test_event_ctx_t *ctx = (test_event_ctx_t *)user_data;

    ctx->callback_triggered = true;
    ctx->callback_err_mask = err_mask;

    /* Return true to disable CSI host interrupt and post INTERRUPT_DISABLE. */
    return true;
}

static void test_event_listener_task(void *arg)
{
    test_event_ctx_t *ctx = (test_event_ctx_t *)arg;

    while (1) {
        struct v4l2_event event;

        if (ioctl(ctx->fd, VIDIOC_DQEVENT, &event) < 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        switch (event.type) {
        case V4L2_EVENT_ESP_MIPI_CSI_INTERRUPT_DISABLE:
            ctx->interrupt_disable_seen = true;
            xSemaphoreGive(ctx->event_done_sem);
            break;
        case V4L2_EVENT_ESP_VIDEO_EVENT_UNSUBSCRIBED:
            xSemaphoreGive(ctx->event_exit_sem);
            vTaskDelete(NULL);
            break;
        default:
            break;
        }
    }
}

static void test_capture_task(void *arg)
{
    test_event_ctx_t *ctx = (test_event_ctx_t *)arg;

    while (ctx->capture_running) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->fd, VIDIOC_DQBUF, &buf) < 0) {
            ctx->dqbuf_timed_out = true;
            ctx->capture_running = false;
            break;
        }

        ctx->frames_received++;
        if (ctx->frames_received == CAPTURE_FRAMES_BEFORE_ERROR) {
            xSemaphoreGive(ctx->frames_ready_sem);
        }

        if (ioctl(ctx->fd, VIDIOC_QBUF, &buf) < 0) {
            continue;
        }
    }

    xSemaphoreGive(ctx->capture_exit_sem);
    vTaskDelete(NULL);
}

static void inject_csi_lane_mismatch(int fd)
{
    esp_cam_sensor_format_t sensor_fmt;
    uint32_t lane_num;
    uint32_t mismatch_lanes;

    memset(&sensor_fmt, 0, sizeof(sensor_fmt));
    TEST_ESP_OK(ioctl(fd, VIDIOC_G_SENSOR_FMT, &sensor_fmt));

    lane_num = sensor_fmt.mipi_info.lane_num;
    TEST_ASSERT_GREATER_THAN_UINT32(0, lane_num);

    /* Force host/sensor lane mismatch to trigger CSI host error IRQ. */
    mismatch_lanes = (lane_num > 1) ? 1 : 2;
    mipi_csi_host_ll_set_active_lanes_num(&MIPI_CSI_HOST, mismatch_lanes);
}

static void capture_frames(int fd, int frame_count)
{
    for (int i = 0; i < frame_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        TEST_ESP_OK(ioctl(fd, VIDIOC_DQBUF, &buf));
        TEST_ESP_OK(ioctl(fd, VIDIOC_QBUF, &buf));
    }
}

TEST_CASE("V4L2 event init/deinit with listener task", "[video][event]")
{
    int fd;
    int count = 10;

    setUp();

    TEST_ESP_OK(example_video_init());

    for (int i = 0; i < count; i++) {
        fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
        TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

        /*
         * example_video_event_init() creates a listener task that blocks on
         * VIDIOC_DQEVENT after subscribing V4L2_EVENT_ESP_MIPI_CSI_ERROR.
         */
        TEST_ESP_OK(example_video_event_init(EXAMPLE_VIDEO_EVENT_TARGET_MIPI_CSI, fd));

        /* Double init should fail while listener task is alive */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                          example_video_event_init(EXAMPLE_VIDEO_EVENT_TARGET_MIPI_CSI, fd));

        /*
         * example_video_event_deinit() issues VIDIOC_UNSUBSCRIBE_EVENT, which
         * posts V4L2_EVENT_ESP_VIDEO_EVENT_UNSUBSCRIBED so the listener task
         * can exit asynchronously, then waits for that task to finish.
         */
        TEST_ESP_OK(example_video_event_deinit(EXAMPLE_VIDEO_EVENT_TARGET_MIPI_CSI));

        /* Double deinit should fail after listener task is gone */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                          example_video_event_deinit(EXAMPLE_VIDEO_EVENT_TARGET_MIPI_CSI));

        close(fd);
    }

    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("V4L2 event API invalid arguments", "[video][event]")
{
    int fd;

    setUp();

    TEST_ESP_OK(example_video_init());

    fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, example_video_event_init(EXAMPLE_VIDEO_EVENT_TARGET_MIPI_CSI, -1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, example_video_event_init(EXAMPLE_VIDEO_EVENT_TARGET_MAX, fd));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, example_video_event_deinit(EXAMPLE_VIDEO_EVENT_TARGET_MAX));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, example_video_event_deinit(EXAMPLE_VIDEO_EVENT_TARGET_MIPI_CSI));

    close(fd);
    TEST_ESP_OK(example_video_deinit());
}

TEST_CASE("V4L2 event callback captures INTERRUPT_DISABLE and restarts", "[video][event]")
{
    int fd;
    int ret;
    int type;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req;
    struct v4l2_event_subscription sub;
    struct v4l2_event_callback event_cb;
    struct timeval dqbuf_timeout;
    test_event_ctx_t ctx;

    setUp();
    memset(&ctx, 0, sizeof(ctx));

    TEST_ESP_OK(example_video_init());

    fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);
    ctx.fd = fd;

    ctx.frames_ready_sem = xSemaphoreCreateBinary();
    ctx.capture_exit_sem = xSemaphoreCreateBinary();
    ctx.event_done_sem = xSemaphoreCreateBinary();
    ctx.event_exit_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(ctx.frames_ready_sem);
    TEST_ASSERT_NOT_NULL(ctx.capture_exit_sem);
    TEST_ASSERT_NOT_NULL(ctx.event_done_sem);
    TEST_ASSERT_NOT_NULL(ctx.event_exit_sem);

    /* Error path cannot deliver frames; DQBUF must time out instead of blocking forever. */
    memset(&dqbuf_timeout, 0, sizeof(dqbuf_timeout));
    dqbuf_timeout.tv_sec = DQBUF_TIMEOUT_MS / 1000;
    dqbuf_timeout.tv_usec = (DQBUF_TIMEOUT_MS % 1000) * 1000;
    ret = ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &dqbuf_timeout);
    TEST_ESP_OK(ret);

    memset(&req, 0, sizeof(req));
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = VIDEO_BUFFER_NUM;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    TEST_ESP_OK(ret);

    for (int i = 0; i < VIDEO_BUFFER_NUM; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        TEST_ESP_OK(ret);

        TEST_ASSERT_NOT_NULL(mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset));

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        TEST_ESP_OK(ret);
    }

    /* Subscribe first so the driver creates event_queue. */
    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_ESP_MIPI_CSI_ERROR;
    ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    TEST_ESP_OK(ret);

    /* Must be set before STREAMON. Callback must live in IRAM. */
    memset(&event_cb, 0, sizeof(event_cb));
    event_cb.callback_func = test_csi_event_callback;
    event_cb.user_data = &ctx;
    ret = ioctl(fd, VIDIOC_S_EVENT_CALLBACK, &event_cb);
    TEST_ESP_OK(ret);

    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(test_event_listener_task, "evt_listen",
                                          EVENT_TASK_STACK_SIZE, &ctx, EVENT_TASK_PRIORITY, NULL));

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    TEST_ESP_OK(ret);

    ctx.capture_running = true;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(test_capture_task, "cap_task",
                                          CAPTURE_TASK_STACK_SIZE, &ctx, CAPTURE_TASK_PRIORITY, NULL));

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.frames_ready_sem, pdMS_TO_TICKS(EVENT_WAIT_TIMEOUT_MS)));

    inject_csi_lane_mismatch(fd);

    /* Capture task should hit DQBUF timeout and exit while CSI is in error. */
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.capture_exit_sem, pdMS_TO_TICKS(EVENT_WAIT_TIMEOUT_MS)));
    TEST_ASSERT_TRUE(ctx.dqbuf_timed_out);

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.event_done_sem, pdMS_TO_TICKS(EVENT_WAIT_TIMEOUT_MS)));
    TEST_ASSERT_TRUE(ctx.callback_triggered);
    TEST_ASSERT_NOT_EQUAL(0, ctx.callback_err_mask);
    TEST_ASSERT_TRUE(ctx.interrupt_disable_seen);

    /* Restart hardware after the error event is observed. */
    struct v4l2_restart_config restart_config = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .restart_sensor = true,
    };
    ret = ioctl(fd, VIDIOC_RESTART, &restart_config);
    TEST_ESP_OK(ret);

    /* After hardware restart, streaming should deliver frames again. */
    capture_frames(fd, CAPTURE_FRAMES_AFTER_RESTART);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    TEST_ESP_OK(ret);

    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_ALL;
    ret = ioctl(fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.event_exit_sem, pdMS_TO_TICKS(EVENT_WAIT_TIMEOUT_MS)));

    close(fd);
    vSemaphoreDelete(ctx.frames_ready_sem);
    vSemaphoreDelete(ctx.capture_exit_sem);
    vSemaphoreDelete(ctx.event_done_sem);
    vSemaphoreDelete(ctx.event_exit_sem);

    TEST_ESP_OK(example_video_deinit());
}

#endif /* CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE && ESP_VIDEO_CSI_DRIVER_HAS_EVENT */
