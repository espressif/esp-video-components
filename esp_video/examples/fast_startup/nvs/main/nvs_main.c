/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

// #undef LOG_LOCAL_LEVEL
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"
#include "fast_startup_common.h"

#define MOUNT_POINT             CONFIG_EXAMPLE_SDMMC_MOUNT_POINT

#define CACHE_PARTITION         "ipa_nvs"
#define CACHE_NAMESPACE         "fast_startup"
#define CACHE_KEY               "ipa"

#if !CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#error "Enable ISP Pipeline Controller in menuconfig"
#endif

static const char *TAG = "nvs_fast_startup";

typedef struct {
    fast_startup_capture_t cap;
    example_storage_handle_t fatfs;
    nvs_handle_t nvs;
    esp_ipa_state_t cache;          /*!< State loaded from NVS at boot */
    bool cache_valid;
    bool settled;                   /*!< IPA settled before stream off */
    uint32_t image_index;           /*!< Index used by this boot's pictures */
    esp_ipa_state_t state;          /*!< State read after stream off */
} app_t;

static esp_err_t cache_open(app_t *app)
{
    esp_err_t ret = nvs_flash_init_partition(CACHE_PARTITION);

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "erase NVS partition %s (%s)", CACHE_PARTITION, esp_err_to_name(ret));
        ESP_RETURN_ON_ERROR(nvs_flash_erase_partition(CACHE_PARTITION), TAG, "erase NVS failed");
        ret = nvs_flash_init_partition(CACHE_PARTITION);
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "init NVS partition %s failed", CACHE_PARTITION);
    ESP_RETURN_ON_ERROR(nvs_open_from_partition(CACHE_PARTITION, CACHE_NAMESPACE, NVS_READWRITE, &app->nvs),
                        TAG, "open NVS namespace failed");
    return ESP_OK;
}

static void log_state(const char *what, const esp_ipa_state_t *state)
{
    ESP_LOGI(TAG, "%s: exposure=%" PRIu32 " gain=%.3f total_gain=%.3f ct=%" PRId32 " K%s",
             what, state->agc.applied_exposure, state->agc.prev_gain, state->agc.prev_total_gain,
             state->awb.ct.ct, state->awb.ct.valid ? "" : " (invalid)");
}

/**
 * The blob is esp_ipa_state_t as is. set_ipa_state() checks its version, CRC,
 * resolution, configuration version, modules and tuning tables.
 */
static void cache_load(app_t *app)
{
    size_t size = sizeof(app->cache);
    esp_err_t ret = nvs_get_blob(app->nvs, CACHE_KEY, &app->cache, &size);

    app->cache_valid = ret == ESP_OK && size == sizeof(app->cache);
    if (app->cache_valid) {
        log_state("cached state", &app->cache);
    } else {
        ESP_LOGI(TAG, "no IPA cache in NVS (%s, size=%zu)", esp_err_to_name(ret), size);
    }
}

/**
 * Decide whether the new state differs enough from the cached one to be worth a
 * flash write. Brightness is the AGC total gain, which already includes exposure;
 * color is the published color temperature. Small drifts keep the cache, so
 * most boots write nothing.
 */
static bool cache_needs_update(const app_t *app)
{
    const esp_ipa_state_t *old = &app->cache;
    const esp_ipa_state_t *cur = &app->state;
    bool compared = false;

    if (!app->cache_valid) {
        ESP_LOGI(TAG, "no valid cache, write");
        return true;
    }
    if (cur->modules & ESP_IPA_STATE_MOD_AGC) {
        /* prev_total_gain = exposure / min_exposure * sensor gain */
        float old_b = old->agc.prev_total_gain;
        float cur_b = cur->agc.prev_total_gain;
        float diff_pct = old_b > 0.0f ? fabsf(cur_b - old_b) * 100.0f / old_b : (cur_b > 0.0f ? 100.0f : 0.0f);

        ESP_LOGI(TAG, "total gain %.3f -> %.3f (%.1f%%, threshold %d%%)",
                 old_b, cur_b, diff_pct, CONFIG_EXAMPLE_CACHE_BRIGHTNESS_DIFF_PCT);
        if (diff_pct >= CONFIG_EXAMPLE_CACHE_BRIGHTNESS_DIFF_PCT) {
            return true;
        }
        compared = true;
    }

    if ((cur->modules & ESP_IPA_STATE_MOD_AWB) && old->awb.ct.valid && cur->awb.ct.valid) {
        int32_t diff_k = cur->awb.ct.ct - old->awb.ct.ct;

        ESP_LOGI(TAG, "color temperature %" PRId32 " -> %" PRId32 " K (threshold %d K)",
                 old->awb.ct.ct, cur->awb.ct.ct, CONFIG_EXAMPLE_CACHE_CT_DIFF_K);
        if (abs(diff_k) >= CONFIG_EXAMPLE_CACHE_CT_DIFF_K) {
            return true;
        }
        compared = true;
    }

    if (!compared) {
        ESP_LOGI(TAG, "no AGC or AWB state to compare");
    }
    return false;
}

static esp_err_t cache_store(app_t *app)
{
    int64_t start_us;

    if (!cache_needs_update(app)) {
        ESP_LOGI(TAG, "change below threshold, not written");
        return ESP_OK;
    }

    start_us = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(nvs_set_blob(app->nvs, CACHE_KEY, &app->state, sizeof(app->state)), TAG, "write IPA cache failed");
    ESP_RETURN_ON_ERROR(nvs_commit(app->nvs), TAG, "commit IPA cache failed");
    ESP_LOGI(TAG, "IPA state written (%zu bytes, %" PRId64 " ms)",
             sizeof(app->state), (esp_timer_get_time() - start_us) / 1000);
    return ESP_OK;
}

/**
 * @brief AE and AWB results that the stability check follows.
 */
typedef struct {
    uint32_t exposure;              /*!< AGC applied exposure */
    float gain;                     /*!< AGC sensor gain */
    int32_t ct;                     /*!< AWB color temperature */
    bool ct_valid;
} ipa_sample_t;

static void read_sample(const esp_ipa_state_t *state, ipa_sample_t *sample)
{
    memset(sample, 0, sizeof(*sample));
    if (state->modules & ESP_IPA_STATE_MOD_AGC) {
        sample->exposure = state->agc.applied_exposure;
        sample->gain = state->agc.prev_gain;
    }
    if (state->modules & ESP_IPA_STATE_MOD_AWB) {
        sample->ct = state->awb.ct.ct;
        sample->ct_valid = state->awb.ct.valid;
    }
}

static bool within_pct(float old_val, float new_val, int pct)
{
    if (old_val == new_val) {
        return true;
    }
    return old_val > 0.0f && fabsf(new_val - old_val) * 100.0f <= old_val * pct;
}

static bool sample_close(const ipa_sample_t *prev, const ipa_sample_t *cur, uint32_t modules)
{
    if ((modules & ESP_IPA_STATE_MOD_AGC) &&
            (!within_pct((float)prev->exposure, (float)cur->exposure, CONFIG_EXAMPLE_STABLE_AE_TOL_PCT) ||
             !within_pct(prev->gain, cur->gain, CONFIG_EXAMPLE_STABLE_AE_TOL_PCT))) {
        return false;
    }
    /*
     * Only AWB model 2 with export_ct publishes a color temperature. Without one on
     * both frames only exposure and gain are checked; a frame where it just appears
     * is not stable.
     */
    if ((modules & ESP_IPA_STATE_MOD_AWB) && (prev->ct_valid || cur->ct_valid) &&
            (!prev->ct_valid || !cur->ct_valid || abs(cur->ct - prev->ct) > CONFIG_EXAMPLE_STABLE_CT_TOL_K)) {
        return false;
    }
    return true;
}

/**
 * Stream until exposure, gain and color temperature have each changed by no
 * more than their tolerance for CONFIG_EXAMPLE_STABLE_FRAMES frames in a row.
 * After every AE update the AGC holds for frame_delay frames, so the frame
 * count must be larger than that delay.
 */
static esp_err_t wait_ipa_stable(app_t *app)
{
    int64_t start_us = esp_timer_get_time();
    ipa_sample_t prev = {0};
    ipa_sample_t cur = {0};
    uint32_t stable = 0;
    uint32_t frames = 0;

    ESP_LOGD(TAG, "wait %d stable frames, AE tol=%d%%, ct tol=%d K, timeout=%d ms",
             CONFIG_EXAMPLE_STABLE_FRAMES, CONFIG_EXAMPLE_STABLE_AE_TOL_PCT,
             CONFIG_EXAMPLE_STABLE_CT_TOL_K, CONFIG_EXAMPLE_STABLE_WAIT_MS);

    /*
     * Frames keep the receive timeout set at init. Cutting the last DQBUF to the
     * deadline would turn "not stable" into a frame error.
     */
    while ((esp_timer_get_time() - start_us) / 1000 < CONFIG_EXAMPLE_STABLE_WAIT_MS) {
        ESP_RETURN_ON_ERROR(fast_startup_capture_skip_frame(&app->cap), TAG, "recycle frame failed");
        ESP_RETURN_ON_ERROR(esp_video_isp_pipeline_get_ipa_state(&app->state), TAG, "get IPA state failed");

        read_sample(&app->state, &cur);
        stable = (frames > 0 && sample_close(&prev, &cur, app->state.modules)) ? stable + 1 : 0;
        frames++;
        prev = cur;

        ESP_LOGD(TAG, "frame %" PRIu32 " exposure=%" PRIu32 " gain=%.3f ct=%" PRId32 " stable=%" PRIu32,
                 frames, cur.exposure, cur.gain, cur.ct, stable);
        if (stable >= CONFIG_EXAMPLE_STABLE_FRAMES) {
            ESP_LOGI(TAG, "IPA stable after %" PRIu32 " frames: exposure=%" PRIu32 " gain=%.3f ct=%" PRId32 " K",
                     frames, cur.exposure, cur.gain, cur.ct);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "IPA not stable within %d ms, %" PRIu32 " frames (exposure=%" PRIu32 " gain=%.3f ct=%" PRId32 "%s)",
             CONFIG_EXAMPLE_STABLE_WAIT_MS, frames, cur.exposure, cur.gain, cur.ct, cur.ct_valid ? "" : " invalid");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t image_path(char *path, size_t size, uint32_t index, const char *suffix)
{
    int n = snprintf(path, size, "%s/%s_no.%04" PRIu32 "%s.jpg",
                     MOUNT_POINT, CONFIG_EXAMPLE_IMAGE_FILE_NAME, index, suffix);

    ESP_RETURN_ON_FALSE(n > 0 && n < (int)size, ESP_ERR_INVALID_SIZE, TAG, "image path is too long");
    return ESP_OK;
}

static esp_err_t image_exists(uint32_t index, const char *suffix, bool *exists)
{
    char path[128];
    struct stat st;

    ESP_RETURN_ON_ERROR(image_path(path, sizeof(path), index, suffix), TAG, "image path failed");
    if (stat(path, &st) == 0) {
        *exists = true;
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(errno == ENOENT, ESP_FAIL, TAG, "failed to stat %s errno=%d", path, errno);
    *exists = false;
    return ESP_OK;
}

/**
 * Pick the first index that has neither <name>_no.<index>.jpg nor
 * <name>_no.<index>_cold_first.jpg. The index is not kept in NVS, so a
 * picture does not cost a flash write.
 */
static esp_err_t pick_image_index(app_t *app)
{
    bool exists;
    bool cold_exists;

    for (app->image_index = 0; ; app->image_index++) {
        ESP_RETURN_ON_ERROR(image_exists(app->image_index, "", &exists), TAG, "check image failed");
        ESP_RETURN_ON_ERROR(image_exists(app->image_index, "_cold_first", &cold_exists), TAG, "check image failed");
        if (!exists && !cold_exists) {
            return ESP_OK;
        }
    }
}

static esp_err_t capture_to_sd(app_t *app, const char *suffix)
{
    char path[128];

    ESP_RETURN_ON_ERROR(image_path(path, sizeof(path), app->image_index, suffix), TAG, "image path failed");
    return fast_startup_capture_save_jpeg(&app->cap, path);
}

/**
 * Cold boot: no usable cache. Stream until exposure, gain and color
 * temperature are stable, then save that frame. With
 * CONFIG_EXAMPLE_SAVE_COLD_FIRST_FRAME the first frame is saved as well, as
 * <name>_no.<index>_cold_first.jpg, to compare with a warm-boot first frame.
 */
static esp_err_t run_cold(app_t *app)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "cold boot: stream until IPA is stable");
    ret = fast_startup_capture_start(&app->cap, true);
#if CONFIG_EXAMPLE_SAVE_COLD_FIRST_FRAME
    if (ret == ESP_OK) {
        ret = capture_to_sd(app, "_cold_first");
    }
#endif
    if (ret == ESP_OK) {
        ret = wait_ipa_stable(app);
    }
    if (ret == ESP_OK) {
        app->settled = true;
        ret = capture_to_sd(app, "");
    }
    fast_startup_capture_stop(&app->cap);
    return ret;
}

/**
 * Warm boot: write the cached state back before stream on and save the first
 * frame. Keep streaming afterwards so the IPA follows the current scene; the
 * state read after that decides whether NVS is rewritten.
 */
static esp_err_t run_restore(app_t *app)
{
    esp_err_t ret;

    ret = esp_video_isp_pipeline_set_ipa_state(&app->cache);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "cached IPA state rejected (%s), fall back to cold boot", esp_err_to_name(ret));
        app->cache_valid = false;
        return run_cold(app);
    }
    ESP_LOGI(TAG, "warm boot: NVS cache applied before stream on, first frame uses it");

    ret = fast_startup_capture_start(&app->cap, true);
    if (ret == ESP_OK) {
        ret = capture_to_sd(app, "");
    }
    /* The picture is already saved; a scene that does not settle only skips the cache update. */
    if (ret == ESP_OK && wait_ipa_stable(app) == ESP_OK) {
        app->settled = true;
    }
    fast_startup_capture_stop(&app->cap);
    return ret;
}

void app_main(void)
{
    app_t *app;
    esp_err_t ret;
    int64_t boot_us = esp_timer_get_time();

#if LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("ISP", ESP_LOG_DEBUG);
    esp_log_level_set("fast_startup_common", ESP_LOG_DEBUG);
#endif

    app = calloc(1, sizeof(*app));
    ESP_ERROR_CHECK(app ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGD(TAG, "IPA cache sizeof=%zu", sizeof(app->cache));

    ESP_ERROR_CHECK(cache_open(app));
    cache_load(app);
    ESP_LOGI(TAG, "boot reset_reason=%d", (int)esp_reset_reason());

    ESP_ERROR_CHECK(example_video_init());
    ESP_ERROR_CHECK(fast_startup_capture_init(&app->cap, EXAMPLE_CAM_DEV_PATH, CONFIG_EXAMPLE_RECV_TIMEOUT_MS,
                    CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY));
    ESP_ERROR_CHECK(example_mount_fatfs_to_mmc(&app->fatfs));
    ESP_ERROR_CHECK(pick_image_index(app));

    if (app->cache_valid) {
        ret = run_restore(app);
    } else {
        ret = run_cold(app);
    }
    ESP_LOGI(TAG, "capture done %" PRId64 " ms after app start", (esp_timer_get_time() - boot_us) / 1000);

    if (ret == ESP_OK && app->settled) {
        ret = esp_video_isp_pipeline_get_ipa_state(&app->state);
        if (ret == ESP_OK) {
            log_state("IPA state after stream off", &app->state);
            ret = cache_store(app);
        }
    } else if (ret == ESP_OK) {
        ESP_LOGW(TAG, "IPA not stable, not written");
    }

    if (example_unmount_fatfs_in_mmc(app->fatfs) != ESP_OK) {
        ESP_LOGE(TAG, "failed to unmount SD card");
    }
    nvs_close(app->nvs);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "stop: %s", esp_err_to_name(ret));
        return;
    }

#if CONFIG_EXAMPLE_RESTART_AFTER_CAPTURE
    ESP_LOGI(TAG, "restart in %d ms", CONFIG_EXAMPLE_RESTART_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_EXAMPLE_RESTART_DELAY_MS));
    esp_restart();
#else
    ESP_LOGI(TAG, "done, power off or reset to capture again");
#endif
}
