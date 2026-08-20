/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <linux/videodev2.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_video_ioctl.h"
#include "example_video_common.h"

#if ESP_VIDEO_CSI_DRIVER_HAS_EVENT && EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR

static const char *TAG = "video_event";

#define VIDEO_EVENT_TASK_PRIORITY 9
#define VIDEO_EVENT_TASK_STACK_SIZE 3072

typedef struct video_event_data {
    int fd;
    TaskHandle_t task_handle;
    SemaphoreHandle_t sem;
} video_event_data_t;

static video_event_data_t s_video_event_data[] = {
    [EXAMPLE_VIDEO_EVENT_TARGET_MIPI_CSI] = {
        .fd = -1,
        .task_handle = NULL,
        .sem = NULL,
    },
};

static void video_event_task(void *arg)
{
    video_event_data_t *data = (video_event_data_t *)arg;
    int fd = data->fd;

    while (1) {
        struct v4l2_event event;

        int ret = ioctl(fd, VIDIOC_DQEVENT, &event);
        if (ret < 0) {
            ESP_LOGE(TAG, "ioctl error: %d and wait 1 second", errno);
            sleep(1);
            continue;
        }

        ESP_LOGD(TAG, "event: %" PRIx32, event.type);
        ESP_LOGD(TAG, "event timestamp: %" PRId64 "s - %ld ns", event.timestamp.tv_sec, event.timestamp.tv_nsec);
        ESP_LOGD(TAG, "event sequence: %" PRIu32, event.sequence);

        switch (event.type) {
        case V4L2_EVENT_ESP_MIPI_CSI_ERROR: {
            v4l2_event_esp_mipi_csi_error_t *csi_error = (v4l2_event_esp_mipi_csi_error_t *)&event.u.data;

            ESP_LOGD(TAG, "CSI error mask: %" PRIx32, csi_error->host_err_mask);

            if (csi_error->host_err_mask & ESP_MIPI_CSI_HOST_ERR_PHY) {
                ESP_LOGI(TAG, "MIPI-CSI PHY fatal error: data lane Start-of-Transmission sync error (ErrSotSyncHS) or start-of-transmission error (ErrSotHS) or escape entry error (ErrEsc)");
            }
            if (csi_error->host_err_mask & ESP_MIPI_CSI_HOST_ERR_PACKET) {
                ESP_LOGI(TAG, "MIPI-CSI packet fatal error: header double-bit ECC error or shorter payload or header single-bit ECC error (corrected by hardware)");
            }
            if (csi_error->host_err_mask & ESP_MIPI_CSI_HOST_ERR_FRAME) {
                ESP_LOGI(TAG, "MIPI-CSI frame boundary error: unpaired Frame Start / Frame End or frame sequence error or frame error");
            }
            if (csi_error->host_err_mask & ESP_MIPI_CSI_HOST_ERR_CRC) {
                ESP_LOGI(TAG, "MIPI-CSI CRC error: a frame containing at least one CRC error or payload CRC error");
            }
            if (csi_error->host_err_mask & ESP_MIPI_CSI_HOST_ERR_DATA_ID) {
                ESP_LOGI(TAG, "MIPI-CSI data ID error: unrecognized or unimplemented data type detected");
            }

            struct v4l2_restart_config config = {
                .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                .restart_sensor = true,
            };
            if (ioctl(fd, VIDIOC_RESTART, &config) < 0) {
                ESP_LOGE(TAG, "Failed to restart video");
            } else {
                ESP_LOGI(TAG, "Restart video successfully");
            }

            break;
        }
        case V4L2_EVENT_ESP_VIDEO_EVENT_UNSUBSCRIBED: {
            ESP_LOGI(TAG, "Video event unsubscribed and delete task");
            if (xSemaphoreGive(data->sem) != pdPASS) {
                ESP_LOGE(TAG, "Failed to give semaphore");
            }
            vTaskDelete(NULL);
            break;
        }
        default:
            ESP_LOGE(TAG, "Unknown event: %" PRIu32, event.type);
            break;
        }
    }
}

/**
 * @brief Initialize the video event tracking
 *
 * @param target: the target of the video event
 * @param fd: the file descriptor of the video device
 *
 * @return ESP_OK on success, otherwise an error code
 */
esp_err_t example_video_event_init(example_video_event_target_t target, int fd)
{
    int ret;
    struct v4l2_event_subscription sub = {
        .type = V4L2_EVENT_ESP_MIPI_CSI_ERROR,
    };

    ESP_RETURN_ON_FALSE(fd >= 0, ESP_ERR_INVALID_ARG, TAG, "Invalid file descriptor");
    ESP_RETURN_ON_FALSE(target < EXAMPLE_VIDEO_EVENT_TARGET_MAX, ESP_ERR_INVALID_ARG, TAG, "Invalid target");
    ESP_RETURN_ON_FALSE(s_video_event_data[target].task_handle == NULL, ESP_ERR_INVALID_STATE, TAG, "Video event tracking already initialized");

    s_video_event_data[target].sem = xSemaphoreCreateBinary();
    if (s_video_event_data[target].sem == NULL) {
        ESP_LOGE(TAG, "Failed to create binary semaphore");
        return ESP_ERR_NO_MEM;
    }

    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub) == 0, ESP_FAIL, fail_0, TAG, "Failed to subscribe event");

    s_video_event_data[target].fd = fd;

    ret = xTaskCreate(video_event_task, "video_event_task", VIDEO_EVENT_TASK_STACK_SIZE,
                      (void *)&s_video_event_data[target], VIDEO_EVENT_TASK_PRIORITY,
                      &s_video_event_data[target].task_handle);
    ESP_GOTO_ON_FALSE(ret == pdPASS, ESP_ERR_NO_MEM, fail_1, TAG, "Failed to create video event tracking task");

    return ESP_OK;

fail_1:
    s_video_event_data[target].fd = -1;
    sub.type = V4L2_EVENT_ALL;
    if (ioctl(fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub) < 0) {
        ESP_LOGE(TAG, "Failed to unsubscribe event");
    }
fail_0:
    s_video_event_data[target].task_handle = NULL;
    vSemaphoreDelete(s_video_event_data[target].sem);
    s_video_event_data[target].sem = NULL;
    return ESP_FAIL;
}

/**
 * @brief Deinitialize the video event tracking
 *
 * @param target: the target of the video event
 *
 * @return ESP_OK on success, otherwise an error code
 */
esp_err_t example_video_event_deinit(example_video_event_target_t target)
{
    struct v4l2_event_subscription sub = {
        .type = V4L2_EVENT_ALL,
    };

    ESP_RETURN_ON_FALSE(target < EXAMPLE_VIDEO_EVENT_TARGET_MAX, ESP_ERR_INVALID_ARG, TAG, "Invalid target");
    ESP_RETURN_ON_FALSE(s_video_event_data[target].task_handle != NULL, ESP_ERR_INVALID_STATE, TAG, "Video event task not initialized");

    /* Send unsubscribed event to video event task and wait for task to be deleted.
     */
    if (ioctl(s_video_event_data[target].fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub) < 0) {
        ESP_LOGE(TAG, "Failed to unsubscribe event");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(s_video_event_data[target].sem, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to take semaphore");
        return ESP_FAIL;
    }

    s_video_event_data[target].task_handle = NULL;
    s_video_event_data[target].fd = -1;
    vSemaphoreDelete(s_video_event_data[target].sem);
    s_video_event_data[target].sem = NULL;
    return ESP_OK;
}

#endif /* ESP_VIDEO_CSI_DRIVER_HAS_EVENT && EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
