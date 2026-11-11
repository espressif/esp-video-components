/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/time.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_attr.h"
#include "esp_bit_defs.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"

#if !CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#error "Enable ISP Pipeline Controller in menuconfig"
#endif

#define SUPPORT_GPIO_MARKER      1

#define BUFFER_COUNT             3
#define CAM_DEV_PATH             EXAMPLE_CAM_DEV_PATH

#if SUPPORT_GPIO_MARKER
#define GPIO_MARKER_APP_MAIN     GPIO_NUM_4
#define GPIO_MARKER_INIT_DONE    GPIO_NUM_5
#define GPIO_MARKER_CAPTURE_DONE GPIO_NUM_6
#endif

static const char *TAG = "software_aov";

/*
 * Deep sleep keeps this buffer. get_ipa_state fills it after the image is
 * usable. set_ipa_state writes that same object back before the next STREAMON.
 * The first dequeued frame is usable only after that write-back.
 */
static RTC_DATA_ATTR esp_ipa_state_t s_ipa_state;

#if SUPPORT_GPIO_MARKER
static esp_err_t init_marker_gpios(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_MARKER_INIT_DONE) | (1ULL << GPIO_MARKER_CAPTURE_DONE) | (1ULL << GPIO_MARKER_APP_MAIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Failed to config marker GPIOs");
    gpio_set_level(GPIO_MARKER_INIT_DONE, 0);
    gpio_set_level(GPIO_MARKER_CAPTURE_DONE, 0);
    gpio_set_level(GPIO_MARKER_APP_MAIN, 1);
    return ESP_OK;
}
#endif

static esp_err_t set_recv_timeout(int fd, uint32_t timeout_ms)
{
    struct timeval timeout = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };

    if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        ESP_LOGE(TAG, "failed to set receive timeout");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static uint32_t clamp_recv_timeout_ms(int64_t remaining_ms, uint32_t recv_timeout_ms)
{
    if (remaining_ms <= 0) {
        return 0;
    }
    if (remaining_ms < (int64_t)recv_timeout_ms) {
        return (uint32_t)remaining_ms;
    }
    return recv_timeout_ms;
}

static esp_err_t camera_open(int *out_fd)
{
    int fd;
    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fd = open(CAM_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open %s", CAM_DEV_PATH);
        return ESP_FAIL;
    }

    if (set_recv_timeout(fd, CONFIG_EXAMPLE_RECV_TIMEOUT_MS) != ESP_OK) {
        close(fd);
        return ESP_FAIL;
    }

    memset(&req, 0, sizeof(req));
    req.count  = BUFFER_COUNT;
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "failed to require buffer");
        close(fd);
        return ESP_FAIL;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type   = type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to query buffer");
            close(fd);
            return ESP_FAIL;
        }

        void *buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffer == MAP_FAILED) {
            ESP_LOGE(TAG, "failed to map buffer");
            close(fd);
            return ESP_FAIL;
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            close(fd);
            return ESP_FAIL;
        }
    }

    *out_fd = fd;
    return ESP_OK;
}

static esp_err_t camera_stream(int fd, bool on)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, on ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "failed to %s stream", on ? "start" : "stop");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t recycle_one_frame(int fd)
{
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to receive video frame");
        return ESP_FAIL;
    }
    if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to queue video frame");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t wait_color_temperature(int fd)
{
    int64_t start_us = esp_timer_get_time();
    int32_t ct = 0;

    while (1) {
        int64_t elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
        int64_t remaining_ms = (int64_t)CONFIG_EXAMPLE_CT_WAIT_MS - elapsed_ms;
        uint32_t recv_ms;
        esp_err_t ret;

        if (remaining_ms <= 0) {
            break;
        }

        recv_ms = clamp_recv_timeout_ms(remaining_ms, CONFIG_EXAMPLE_RECV_TIMEOUT_MS);
        ESP_RETURN_ON_ERROR(set_recv_timeout(fd, recv_ms), TAG, "set receive timeout failed");

        ret = recycle_one_frame(fd);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = esp_video_isp_pipeline_get_env_int32("ct", &ct);
        if (ret != ESP_OK) {
            continue;
        }
        if (ct >= CONFIG_EXAMPLE_CT_MIN && ct <= CONFIG_EXAMPLE_CT_MAX) {
            ESP_LOGI(TAG, "color temperature %" PRId32 " K is in range", ct);
            /* Last DQBUF may have used only the remaining wait time. Restore the
             * capture timeout; STREAMOFF/STREAMON does not clear it. */
            return set_recv_timeout(fd, CONFIG_EXAMPLE_RECV_TIMEOUT_MS);
        }
    }

    ESP_LOGE(TAG, "color temperature %" PRId32 " K stayed outside [%d, %d]",
             ct, CONFIG_EXAMPLE_CT_MIN, CONFIG_EXAMPLE_CT_MAX);
    return ESP_ERR_TIMEOUT;
}

/*
 * Cold boot has no restored ISP parameters. Frames taken here are not usable.
 * Stream until color temperature settles, then the caller stores that state.
 */
static esp_err_t converge_ipa(int fd)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "cold boot, converge IPA before saving state");
    ESP_RETURN_ON_ERROR(camera_stream(fd, true), TAG, "failed to start stream");
    ret = wait_color_temperature(fd);
    if (camera_stream(fd, false) != ESP_OK) {
        return ESP_FAIL;
    }
    return ret;
}

/*
 * Call only after set_ipa_state() has programmed the ISP and camera.
 * GPIO6 marks this first dequeued frame. It is usable because the restored
 * exposure, gain and color parameters are already active at STREAMON.
 */
static esp_err_t capture_usable_frame(int fd)
{
    struct v4l2_buffer buf;
    esp_err_t ret;

    ESP_RETURN_ON_ERROR(camera_stream(fd, true), TAG, "failed to start stream");

    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to receive video frame");
        camera_stream(fd, false);
        return ESP_FAIL;
    }

    if (buf.flags & V4L2_BUF_FLAG_DONE) {
        ESP_LOGI(TAG, "Captured one usable frame, size=%" PRIu32, buf.bytesused);
#if SUPPORT_GPIO_MARKER
        gpio_set_level(GPIO_MARKER_CAPTURE_DONE, 1);
#endif
        ret = ESP_OK;
    } else {
        ESP_LOGW(TAG, "Frame captured with error flags=0x%" PRIx32, buf.flags);
        ret = ESP_FAIL;
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to queue video frame");
        ret = ESP_FAIL;
    }
    if (camera_stream(fd, false) != ESP_OK) {
        ret = ESP_FAIL;
    }
    return ret;
}

static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Enter deep sleep, timer wakeup in %d ms", CONFIG_EXAMPLE_SLEEP_TIME_MS);
    fflush(stdout);
    esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_EXAMPLE_SLEEP_TIME_MS * 1000ULL);
    esp_deep_sleep_start();
}

void app_main(void)
{
    int fd = -1;
    esp_err_t ret = ESP_OK;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_causes();
    bool timer_wakeup = (cause & BIT(ESP_SLEEP_WAKEUP_TIMER)) != 0;

    ESP_LOGI(TAG, "Software AOV boot, wakeup cause=0x%x%s",
             (unsigned)cause, timer_wakeup ? " timer" : "");

#if SUPPORT_GPIO_MARKER
    ret = init_marker_gpios();
    ESP_GOTO_ON_ERROR(ret, clean1, TAG, "GPIO init failed");
#endif

    ret = example_video_init();
    ESP_GOTO_ON_ERROR(ret, clean1, TAG, "Camera init failed");
#if SUPPORT_GPIO_MARKER
    gpio_set_level(GPIO_MARKER_INIT_DONE, 1);
#endif

    ret = camera_open(&fd);
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "Camera open failed");

    if (timer_wakeup) {
        /*
         * Write the saved IPA state before STREAMON. Without this call the
         * first frame is still waiting on statistics and is not usable.
         */
        ret = esp_video_isp_pipeline_set_ipa_state(&s_ipa_state);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "restored IPA state, first frame is usable");
            ret = capture_usable_frame(fd);
        } else {
            ESP_LOGE(TAG, "saved IPA state rejected (%s), converge again", esp_err_to_name(ret));
            ret = converge_ipa(fd);
        }
    } else {
        ret = converge_ipa(fd);
    }
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "capture failed");

    ret = esp_video_isp_pipeline_get_ipa_state(&s_ipa_state);
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "failed to read IPA state");
    ESP_LOGI(TAG, "saved IPA state");

    close(fd);
    fd = -1;
    enter_deep_sleep();

clean0:
    if (fd >= 0) {
        close(fd);
    }
    ESP_ERROR_CHECK(example_video_deinit());
clean1:
    return;
}
