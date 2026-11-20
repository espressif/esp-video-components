/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
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
#include <sys/time.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_bit_defs.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "soc/soc_caps.h"
#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"

#if !CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#error "Enable ISP Pipeline Controller in menuconfig"
#endif

#define BUFFER_COUNT             3
#define CAM_DEV_PATH             EXAMPLE_CAM_DEV_PATH

#define SUPPORT_GPIO_MARKER      1

#if SUPPORT_GPIO_MARKER
#define GPIO_MARKER_STREAM_ON    GPIO_NUM_4
#define GPIO_MARKER_CAPTURE      GPIO_NUM_5
#define GPIO_MARKER_STREAM_OFF   GPIO_NUM_6
#endif

static const char *TAG = "software_aov";

#if SUPPORT_GPIO_MARKER
static esp_err_t init_marker_gpios(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_MARKER_CAPTURE) | (1ULL << GPIO_MARKER_STREAM_OFF) | (1ULL << GPIO_MARKER_STREAM_ON),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Failed to config marker GPIOs");
    gpio_set_level(GPIO_MARKER_CAPTURE, 0);
    gpio_set_level(GPIO_MARKER_STREAM_OFF, 0);
    gpio_set_level(GPIO_MARKER_STREAM_ON, 0);
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
    }

    *out_fd = fd;
    return ESP_OK;
}

/*
 * STREAMOFF clears the driver's queued and done lists. Map once in
 * camera_open(), and queue again before every STREAMON.
 */
static esp_err_t queue_capture_buffers(int fd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type   = type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t camera_stream(int fd, bool on)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (on) {
        esp_err_t ret = queue_capture_buffers(fd);
        if (ret != ESP_OK) {
            return ret;
        }
    }

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
 * Hold TOP and CNNT on during light sleep. STREAMOFF deletes the ISP
 * processor and the CSI controller. The next STREAMON creates both again.
 * esp_isp_new_processor() resets the ISP module on ESP-IDF v5.5.6, then
 * isp_start_pipeline() writes the new processor from the driver state that
 * stays in RAM, including CCM. Keeping these domains powered does not leave
 * the previous hardware configuration in place.
 */
static esp_err_t keep_camera_power_domains(void)
{
#if SOC_PM_SUPPORT_TOP_PD
    ESP_RETURN_ON_ERROR(esp_sleep_pd_config(ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_ON),
                        TAG, "failed to keep TOP domain on");
#endif
#if SOC_PM_SUPPORT_CNNT_PD
    ESP_RETURN_ON_ERROR(esp_sleep_pd_config(ESP_PD_DOMAIN_CNNT, ESP_PD_OPTION_ON),
                        TAG, "failed to keep CNNT domain on");
#endif
    return ESP_OK;
}

static esp_err_t capture_one_frame(int fd)
{
    struct v4l2_buffer buf;
    esp_err_t ret = ESP_OK;

    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to receive video frame");
        return ESP_FAIL;
    }

    if (buf.flags & V4L2_BUF_FLAG_DONE) {
        ESP_LOGI(TAG, "Captured one frame, size=%" PRIu32, buf.bytesused);
    } else {
        ESP_LOGW(TAG, "Frame captured with error flags=0x%" PRIx32, buf.flags);
        ret = ESP_FAIL;
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
        ESP_LOGE(TAG, "failed to queue video frame");
        ret = ESP_FAIL;
    }
    return ret;
}

static esp_err_t enter_light_sleep(void)
{
    int64_t start_us;
    esp_err_t ret;

    ESP_LOGI(TAG, "Enter light sleep for %d ms", CONFIG_EXAMPLE_SLEEP_TIME_MS);
    fflush(stdout);

    ret = esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_EXAMPLE_SLEEP_TIME_MS * 1000ULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable timer wakeup: %s", esp_err_to_name(ret));
        return ret;
    }

    start_us = esp_timer_get_time();
    ret = esp_light_sleep_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "light sleep failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint32_t cause = esp_sleep_get_wakeup_causes();
    ESP_LOGI(TAG, "Woke up from light sleep, cause=0x%" PRIx32 "%s, slept %" PRId64 " ms",
             cause, (cause & BIT(ESP_SLEEP_WAKEUP_TIMER)) ? " timer" : "",
             (esp_timer_get_time() - start_us) / 1000);
    return ESP_OK;
}

void app_main(void)
{
    int fd = -1;
    bool streaming = false;
    esp_err_t ret;

    ret = example_video_init();
    ESP_GOTO_ON_ERROR(ret, clean1, TAG, "Camera init failed");

    ret = keep_camera_power_domains();
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "failed to keep camera power domains");

    ret = camera_open(&fd);
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "Camera open failed");

    ESP_LOGI(TAG, "wait until color temperature is in range");
    ret = camera_stream(fd, true);
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "failed to start stream");
    streaming = true;
    ret = wait_color_temperature(fd);
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "color temperature wait failed");

#if SUPPORT_GPIO_MARKER
    ret = init_marker_gpios();
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "GPIO init failed");
#endif

    while (1) {
#if SUPPORT_GPIO_MARKER
        gpio_set_level(GPIO_MARKER_CAPTURE, 1);
#endif
        ret = capture_one_frame(fd);
        ESP_GOTO_ON_ERROR(ret, clean0, TAG, "capture failed");

#if SUPPORT_GPIO_MARKER
        gpio_set_level(GPIO_MARKER_CAPTURE, 0);

        gpio_set_level(GPIO_MARKER_STREAM_OFF, 1);
#endif
        ret = camera_stream(fd, false);
        ESP_GOTO_ON_ERROR(ret, clean0, TAG, "failed to stop stream");
        streaming = false;

#if SUPPORT_GPIO_MARKER
        gpio_set_level(GPIO_MARKER_STREAM_OFF, 0);
#endif

        ret = enter_light_sleep();
        ESP_GOTO_ON_ERROR(ret, clean0, TAG, "light sleep failed");

#if SUPPORT_GPIO_MARKER
        gpio_set_level(GPIO_MARKER_STREAM_ON, 1);
#endif

        ret = camera_stream(fd, true);
        ESP_GOTO_ON_ERROR(ret, clean0, TAG, "failed to start stream");
        streaming = true;

#if SUPPORT_GPIO_MARKER
        gpio_set_level(GPIO_MARKER_STREAM_ON, 0);
#endif
    }

clean0:
    /*
     * IPA keeps its own camera fd. The last esp_video_close() frees capture
     * buffers and does not stop the sensor, CSI, or ISP. Stop the stream
     * first. If that fails, leave the device open.
     */
    if (streaming) {
        ESP_LOGI(TAG, "Stop capture stream before closing the device");
        if (camera_stream(fd, false) != ESP_OK) {
            ESP_LOGE(TAG, "failed to stop stream, keep the device open");
            return;
        }
    }
    if (fd >= 0) {
        close(fd);
    }
    ESP_ERROR_CHECK(example_video_deinit());
clean1:
    return;
}
