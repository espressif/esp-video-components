/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

// #undef LOG_LOCAL_LEVEL
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"
#include "fast_startup_common.h"

#define MOUNT_POINT             CONFIG_EXAMPLE_SDMMC_MOUNT_POINT

#if !CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#error "Enable ISP Pipeline Controller in menuconfig"
#endif

/**
 * @brief Values kept across deep sleep.
 *
 * image_index is outside esp_ipa_state_t so a later get does not clear it.
 * esp_ipa_state_t is large because every module array is stored at full size.
 */
typedef struct {
    uint32_t image_index;
    esp_ipa_state_t ipa;
} example_rtc_keep_t;

static const char *TAG = "fast_startup";
static RTC_DATA_ATTR example_rtc_keep_t s_rtc;

typedef struct {
    fast_startup_capture_t cap;
    example_storage_handle_t fatfs;
    bool sd_mounted;
} app_t;

static esp_err_t wait_color_temperature(app_t *app)
{
    int64_t start_us = esp_timer_get_time();
    int32_t ct = 0;

    ESP_LOGD(TAG, "wait ct in [%d, %d], timeout=%d ms, recv=%d ms",
             CONFIG_EXAMPLE_CT_MIN, CONFIG_EXAMPLE_CT_MAX, CONFIG_EXAMPLE_CT_WAIT_MS,
             CONFIG_EXAMPLE_RECV_TIMEOUT_MS);

    while (1) {
        int64_t elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
        int64_t remaining_ms = (int64_t)CONFIG_EXAMPLE_CT_WAIT_MS - elapsed_ms;
        esp_err_t ret;

        if (remaining_ms <= 0) {
            break;
        }

        /* A single DQBUF wait must not overrun CONFIG_EXAMPLE_CT_WAIT_MS */
        ESP_RETURN_ON_ERROR(fast_startup_capture_set_timeout(&app->cap, (uint32_t)MIN(remaining_ms, (int64_t)CONFIG_EXAMPLE_RECV_TIMEOUT_MS)),
                            TAG, "set receive timeout failed");

        ret = fast_startup_capture_skip_frame(&app->cap);
        if (ret != ESP_OK) {
            return ret;
        }

        elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
        ret = esp_video_isp_pipeline_get_env_int32("ct", &ct);
        if (ret != ESP_OK) {
            ESP_LOGD(TAG, "ct not ready (%s) elapsed=%" PRId64 " ms", esp_err_to_name(ret), elapsed_ms);
            continue;
        }

        ESP_LOGD(TAG, "ct=%" PRId32 " elapsed=%" PRId64 " ms", ct, elapsed_ms);
        if (ct >= CONFIG_EXAMPLE_CT_MIN && ct <= CONFIG_EXAMPLE_CT_MAX) {
            ESP_LOGI(TAG, "color temperature %" PRId32 " K is in range", ct);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "color temperature %" PRId32 " K stayed outside [%d, %d]",
             ct, CONFIG_EXAMPLE_CT_MIN, CONFIG_EXAMPLE_CT_MAX);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t save_first_frame(app_t *app)
{
    char path[128];
    int n;

    n = snprintf(path, sizeof(path), "%s/%s_no.%04" PRIu32 ".jpg",
                 MOUNT_POINT, CONFIG_EXAMPLE_IMAGE_FILE_NAME, s_rtc.image_index);
    ESP_RETURN_ON_FALSE(n > 0 && n < (int)sizeof(path), ESP_ERR_INVALID_SIZE, TAG, "image path is too long");
    return fast_startup_capture_save_jpeg(&app->cap, path);
}

static esp_err_t run_cold(app_t *app)
{
    esp_err_t ret;

    ESP_LOGD(TAG, "cold boot, stream until color temperature is in range");
    ret = fast_startup_capture_start(&app->cap, false);
    if (ret == ESP_OK) {
        ret = wait_color_temperature(app);
    }
    fast_startup_capture_stop(&app->cap);
    return ret;
}

static esp_err_t run_restore(app_t *app)
{
    esp_err_t ret;

    ESP_LOGD(TAG, "wakeup, set saved IPA state before stream on, image index=%" PRIu32, s_rtc.image_index);
    ret = esp_video_isp_pipeline_set_ipa_state(&s_rtc.ipa);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "saved IPA state rejected (%s)", esp_err_to_name(ret));
        return run_cold(app);
    }

    ret = example_mount_fatfs_to_mmc(&app->fatfs);
    ESP_RETURN_ON_ERROR(ret, TAG, "failed to mount SD card");
    app->sd_mounted = true;

    ret = fast_startup_capture_start(&app->cap, true);
    if (ret == ESP_OK) {
        ret = save_first_frame(app);
    }
    fast_startup_capture_stop(&app->cap);

    if (ret == ESP_OK) {
        s_rtc.image_index++;
        ESP_LOGD(TAG, "next image index=%" PRIu32, s_rtc.image_index);
    }
    return ret;
}

static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "enter deep sleep for %d s, image index=%" PRIu32,
             CONFIG_EXAMPLE_SLEEP_TIME_SEC, s_rtc.image_index);
    ESP_LOGD(TAG, "RTC IPA state version=%" PRIu32 " modules=0x%" PRIx32
             " crc=0x%08" PRIx32 " %" PRIu32 "x%" PRIu32,
             s_rtc.ipa.version, s_rtc.ipa.modules, s_rtc.ipa.crc32,
             s_rtc.ipa.width, s_rtc.ipa.height);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_EXAMPLE_SLEEP_TIME_SEC * 1000000ULL);
    esp_deep_sleep_start();
}

void app_main(void)
{
    app_t *app;
    esp_err_t ret;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_causes();
    bool restore = (cause & BIT(ESP_SLEEP_WAKEUP_TIMER)) != 0;

#if LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("ISP", ESP_LOG_DEBUG);
    esp_log_level_set("fast_startup_common", ESP_LOG_DEBUG);
#endif

    ESP_LOGI(TAG, "boot wakeup_cause=%d restore=%d image_index=%" PRIu32,
             (int)cause, restore, s_rtc.image_index);
    ESP_LOGD(TAG, "RTC IPA sizeof=%zu image_index=%" PRIu32, sizeof(s_rtc.ipa),
             s_rtc.image_index);

    app = calloc(1, sizeof(*app));
    ESP_ERROR_CHECK(app ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(example_video_init());
    ESP_ERROR_CHECK(fast_startup_capture_init(&app->cap, EXAMPLE_CAM_DEV_PATH, CONFIG_EXAMPLE_RECV_TIMEOUT_MS,
                    CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY));

    if (restore) {
        ret = run_restore(app);
    } else {
        ret = run_cold(app);
    }

    if (app->sd_mounted) {
        ESP_LOGD(TAG, "unmount SD card");
        if (example_unmount_fatfs_in_mmc(app->fatfs) != ESP_OK) {
            ESP_LOGE(TAG, "failed to unmount SD card");
            ret = ESP_FAIL;
        }
        app->sd_mounted = false;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "stop before sleep: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGD(TAG, "read IPA state after stream off");
    ESP_ERROR_CHECK(esp_video_isp_pipeline_get_ipa_state(&s_rtc.ipa));
    enter_deep_sleep();
}
