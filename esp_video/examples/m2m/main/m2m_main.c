/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "example_v4l2.h"

#define TEST_DURATION_SEC CONFIG_EXAMPLE_TEST_DURATION_SEC

static const char *TAG = "m2m_example";

typedef struct {
    uint32_t frame_count;
    uint32_t skip_count;
    uint64_t total_bytes;
    int64_t start_us;
    int64_t end_us;
    int64_t last_frame_us;
    int64_t min_interval_us;
    int64_t max_interval_us;
    int64_t total_interval_us;
    int64_t total_stage_us;
    int64_t min_stage_us;
    int64_t max_stage_us;
    int64_t total_codec_us;
    int64_t min_codec_us;
    int64_t max_codec_us;
    uint32_t codec_frame_count;
    uint64_t codec_total_bytes;
} test_stats_t;

#if CONFIG_EXAMPLE_M2M_MODE_PIPELINE
typedef struct {
    test_stats_t capture;
    test_stats_t decode;
    test_stats_t encode;
} pipeline_stats_t;
#endif

static void stats_reset(test_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->min_interval_us = INT64_MAX;
    stats->min_stage_us = INT64_MAX;
    stats->min_codec_us = INT64_MAX;
}

static void stats_begin(test_stats_t *stats)
{
    stats_reset(stats);
    stats->start_us = esp_timer_get_time();
}

static void stats_end(test_stats_t *stats)
{
    stats->end_us = esp_timer_get_time();
}

static void stats_on_capture(test_stats_t *stats, uint32_t bytes)
{
    int64_t now_us = esp_timer_get_time();

    if (stats->last_frame_us > 0) {
        int64_t interval_us = now_us - stats->last_frame_us;

        if (interval_us < stats->min_interval_us) {
            stats->min_interval_us = interval_us;
        }
        if (interval_us > stats->max_interval_us) {
            stats->max_interval_us = interval_us;
        }
        stats->total_interval_us += interval_us;
    }

    stats->last_frame_us = now_us;
    stats->frame_count++;
    stats->total_bytes += bytes;
}

#if CONFIG_EXAMPLE_M2M_MODE_PIPELINE
static void stats_on_stage(test_stats_t *stats, int64_t stage_us)
{
    stats->frame_count++;
    stats->total_stage_us += stage_us;

    if (stage_us < stats->min_stage_us) {
        stats->min_stage_us = stage_us;
    }
    if (stage_us > stats->max_stage_us) {
        stats->max_stage_us = stage_us;
    }
}
#endif

#if CONFIG_EXAMPLE_M2M_MODE_ENCODE || CONFIG_EXAMPLE_M2M_MODE_DECODE
static void stats_on_codec(test_stats_t *stats, uint32_t bytes, int64_t codec_us)
{
    stats->codec_frame_count++;
    stats->codec_total_bytes += bytes;
    stats->total_codec_us += codec_us;

    if (codec_us < stats->min_codec_us) {
        stats->min_codec_us = codec_us;
    }
    if (codec_us > stats->max_codec_us) {
        stats->max_codec_us = codec_us;
    }
}
#endif

static void print_capture_stats(const test_stats_t *stats, const char *label)
{
    int64_t duration_us = stats->end_us - stats->start_us;
    double fps = (stats->total_interval_us > 0) ? (double)stats->frame_count * 1000000.0 / (double)stats->total_interval_us : 0.0;

    ESP_LOGI(TAG, "%s statistics:", label);
    ESP_LOGI(TAG, "  Duration:       %.2f s", (double)duration_us / 1000000.0);
    ESP_LOGI(TAG, "  Frames:         %" PRIu32, stats->frame_count);
    ESP_LOGI(TAG, "  Skipped:        %" PRIu32, stats->skip_count);
    ESP_LOGI(TAG, "  FPS:            %.2f", fps);
    if (stats->frame_count > 0) {
        ESP_LOGI(TAG, "  Avg frame size: %" PRIu32 " bytes",
                 (uint32_t)(stats->total_bytes / stats->frame_count));
    }
    if (stats->frame_count > 1) {
        ESP_LOGI(TAG, "  Frame interval: min %.2f ms, max %.2f ms, avg %.2f ms",
                 (double)stats->min_interval_us / 1000.0,
                 (double)stats->max_interval_us / 1000.0,
                 (double)stats->total_interval_us / (double)(stats->frame_count - 1) / 1000.0);
    }
}

#if CONFIG_EXAMPLE_M2M_MODE_PIPELINE
static void print_stage_stats(const test_stats_t *stats, const char *label)
{
    int64_t duration_us = stats->end_us - stats->start_us;
    double fps = (stats->total_stage_us > 0) ? (double)stats->frame_count * 1000000.0 / (double)stats->total_stage_us : 0.0;

    ESP_LOGI(TAG, "%s statistics:", label);
    ESP_LOGI(TAG, "  Duration:       %.2f s", (double)duration_us / 1000000.0);
    ESP_LOGI(TAG, "  Frames:         %" PRIu32, stats->frame_count);
    ESP_LOGI(TAG, "  Skipped:        %" PRIu32, stats->skip_count);
    ESP_LOGI(TAG, "  FPS:            %.2f", fps);
    ESP_LOGI(TAG, "  Total time:     %.2f ms", (double)stats->total_stage_us / 1000.0);
    if (stats->frame_count > 0) {
        ESP_LOGI(TAG, "  Time/frame:     min %.2f ms, max %.2f ms, avg %.2f ms",
                 (double)stats->min_stage_us / 1000.0,
                 (double)stats->max_stage_us / 1000.0,
                 (double)stats->total_stage_us / (double)stats->frame_count / 1000.0);
    }
}
#endif

#if CONFIG_EXAMPLE_M2M_MODE_ENCODE || CONFIG_EXAMPLE_M2M_MODE_DECODE
static void print_codec_stats(const test_stats_t *stats, const char *label)
{
    int64_t duration_us = stats->end_us - stats->start_us;
    double pipeline_fps = (duration_us > 0) ? (double)stats->codec_frame_count * 1000000.0 / (double)duration_us : 0.0;
    double codec_fps = (stats->total_codec_us > 0) ?
                       (double)stats->codec_frame_count * 1000000.0 / (double)stats->total_codec_us : 0.0;

    ESP_LOGI(TAG, "%s statistics:", label);
    ESP_LOGI(TAG, "  Duration:       %.2f s", (double)duration_us / 1000000.0);
    ESP_LOGI(TAG, "  Frames:         %" PRIu32, stats->codec_frame_count);
    ESP_LOGI(TAG, "  Skipped:        %" PRIu32, stats->skip_count);
    ESP_LOGI(TAG, "  Pipeline FPS:   %.2f", pipeline_fps);
    ESP_LOGI(TAG, "  Codec FPS:      %.2f", codec_fps);
    if (stats->codec_frame_count > 0) {
        ESP_LOGI(TAG, "  Avg output size: %" PRIu32 " bytes",
                 (uint32_t)(stats->codec_total_bytes / stats->codec_frame_count));
        ESP_LOGI(TAG, "  Codec time/frame: min %.2f ms, max %.2f ms, avg %.2f ms",
                 (double)stats->min_codec_us / 1000.0,
                 (double)stats->max_codec_us / 1000.0,
                 (double)stats->total_codec_us / (double)stats->codec_frame_count / 1000.0);
    }
}
#endif

static esp_err_t run_capture_test(example_camera_handle_t camera)
{
    test_stats_t stats;
    example_image_t image = {0};
    int64_t end_us;

    (void)camera;

    ESP_LOGI(TAG, "Capture test: %d seconds", TEST_DURATION_SEC);
    stats_begin(&stats);
    end_us = stats.start_us + (int64_t)TEST_DURATION_SEC * 1000000LL;

    while (esp_timer_get_time() < end_us) {
        if (camera_capture_image(&image) == ESP_OK) {
            stats_on_capture(&stats, image.size);
            buffer_free(&image);
        } else {
            stats.skip_count++;
        }
    }

    stats_end(&stats);
    if (stats.frame_count == 0) {
        return ESP_FAIL;
    }

    print_capture_stats(&stats, "Capture");
    return ESP_OK;
}

#if CONFIG_EXAMPLE_M2M_MODE_PIPELINE
static esp_err_t run_pipeline_test(example_camera_handle_t camera)
{
    pipeline_stats_t stats;
    example_jpeg_decoder_handle_t jpeg_decoder;
    example_h264_encoder_handle_t h264_encoder;
    example_jpeg_config_t jpeg_cfg = {0};
    example_h264_config_t h264_cfg = {
        .bitrate = CONFIG_EXAMPLE_H264_BITRATE,
        .i_period = CONFIG_EXAMPLE_H264_I_PERIOD,
        .min_qp = CONFIG_EXAMPLE_H264_MIN_QP,
        .max_qp = CONFIG_EXAMPLE_H264_MAX_QP,
    };
    example_image_t buffer0 = {0};
    example_image_t buffer1 = {0};
    example_image_t buffer2 = {0};
    int64_t end_us;
    int64_t stage_start_us;

    ESP_RETURN_ON_ERROR(open_jpeg_decoder(&jpeg_cfg, &jpeg_decoder), TAG, "open JPEG decoder failed");
    ESP_RETURN_ON_ERROR(open_h264_encoder(&h264_cfg, &h264_encoder), TAG, "open H.264 encoder failed");
    ESP_RETURN_ON_ERROR(camera_connect(camera, jpeg_decoder), TAG, "camera connect failed");
    ESP_RETURN_ON_ERROR(jpeg_decode_connect(jpeg_decoder, h264_encoder), TAG, "jpeg decode connect failed");

    ESP_LOGI(TAG, "Pipeline test (capture -> JPEG decode -> H.264 encode): %d seconds", TEST_DURATION_SEC);
    stats_begin(&stats.capture);
    stats_begin(&stats.decode);
    stats_begin(&stats.encode);
    end_us = stats.capture.start_us + (int64_t)TEST_DURATION_SEC * 1000000LL;

    while (esp_timer_get_time() < end_us) {
        stage_start_us = esp_timer_get_time();
        if (camera_capture_image(&buffer0) != ESP_OK) {
            stats.capture.skip_count++;
            stats.decode.skip_count++;
            stats.encode.skip_count++;
            continue;
        }
        stats_on_stage(&stats.capture, esp_timer_get_time() - stage_start_us);

        stage_start_us = esp_timer_get_time();
        if (jpeg_decode(&buffer0, &buffer1) != ESP_OK) {
            stats.decode.skip_count++;
            stats.encode.skip_count++;
            buffer_free(&buffer0);
            continue;
        }
        stats_on_stage(&stats.decode, esp_timer_get_time() - stage_start_us);

        stage_start_us = esp_timer_get_time();
        if (h264_encode(&buffer1, &buffer2) != ESP_OK) {
            stats.encode.skip_count++;
            buffer_free(&buffer0);
            buffer_free(&buffer1);
            continue;
        }
        stats_on_stage(&stats.encode, esp_timer_get_time() - stage_start_us);

        buffer_free(&buffer0);
        buffer_free(&buffer1);
        buffer_free(&buffer2);
    }

    stats_end(&stats.capture);
    stats_end(&stats.decode);
    stats_end(&stats.encode);

    close_h264_encoder(h264_encoder);
    close_jpeg_decoder(jpeg_decoder);

    if (stats.capture.frame_count == 0) {
        return ESP_FAIL;
    }

    print_stage_stats(&stats.decode, "JPEG decode");
    print_stage_stats(&stats.encode, "H.264 encode");
    return ESP_OK;
}
#elif CONFIG_EXAMPLE_M2M_MODE_ENCODE
#if CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE
static esp_err_t run_jpeg_encode_test(example_camera_handle_t camera)
{
    test_stats_t stats;
    example_jpeg_encoder_handle_t jpeg_encoder;
    example_jpeg_config_t jpeg_cfg = {
        .quality = CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY,
    };
    example_image_t raw = {0};
    example_image_t encoded = {0};
    int64_t end_us;

    ESP_RETURN_ON_ERROR(open_jpeg_encoder(&jpeg_cfg, &jpeg_encoder), TAG, "open JPEG encoder failed");
    ESP_RETURN_ON_ERROR(jpeg_encode_connect(camera, jpeg_encoder), TAG, "jpeg encode connect failed");

    ESP_LOGI(TAG, "JPEG encode test: %d seconds", TEST_DURATION_SEC);
    stats_begin(&stats);
    end_us = stats.start_us + (int64_t)TEST_DURATION_SEC * 1000000LL;

    while (esp_timer_get_time() < end_us) {
        if (camera_capture_image(&raw) != ESP_OK) {
            stats.skip_count++;
            continue;
        }

        int64_t codec_start = esp_timer_get_time();
        if (jpeg_encode(&raw, &encoded) == ESP_OK) {
            stats_on_codec(&stats, encoded.size, esp_timer_get_time() - codec_start);
        } else {
            stats.skip_count++;
        }

        buffer_free(&raw);
        buffer_free(&encoded);
    }

    stats_end(&stats);
    close_jpeg_encoder(jpeg_encoder);

    if (stats.codec_frame_count == 0) {
        return ESP_FAIL;
    }

    print_codec_stats(&stats, "JPEG Encode");
    return ESP_OK;
}
#endif

#if CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE && !CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE
static esp_err_t run_h264_encode_test(example_camera_handle_t camera)
{
    test_stats_t stats;
    example_h264_encoder_handle_t h264_encoder;
    example_h264_config_t h264_cfg = {
        .bitrate = CONFIG_EXAMPLE_H264_BITRATE,
        .i_period = CONFIG_EXAMPLE_H264_I_PERIOD,
        .min_qp = CONFIG_EXAMPLE_H264_MIN_QP,
        .max_qp = CONFIG_EXAMPLE_H264_MAX_QP,
    };
    example_image_t raw = {0};
    example_image_t encoded = {0};
    int64_t end_us;

    ESP_RETURN_ON_ERROR(open_h264_encoder(&h264_cfg, &h264_encoder), TAG, "open H.264 encoder failed");
    ESP_RETURN_ON_ERROR(h264_encode_connect(camera, h264_encoder), TAG, "h264 encode connect failed");

    ESP_LOGI(TAG, "H.264 encode test: %d seconds", TEST_DURATION_SEC);
    stats_begin(&stats);
    end_us = stats.start_us + (int64_t)TEST_DURATION_SEC * 1000000LL;

    while (esp_timer_get_time() < end_us) {
        if (camera_capture_image(&raw) != ESP_OK) {
            stats.skip_count++;
            continue;
        }

        int64_t codec_start = esp_timer_get_time();
        if (h264_encode(&raw, &encoded) == ESP_OK) {
            stats_on_codec(&stats, encoded.size, esp_timer_get_time() - codec_start);
        } else {
            stats.skip_count++;
        }

        buffer_free(&raw);
        buffer_free(&encoded);
    }

    stats_end(&stats);
    close_h264_encoder(h264_encoder);

    if (stats.codec_frame_count == 0) {
        return ESP_FAIL;
    }

    print_codec_stats(&stats, "H.264 Encode");
    return ESP_OK;
}
#endif
#elif CONFIG_EXAMPLE_M2M_MODE_DECODE
static esp_err_t run_jpeg_decode_test(example_camera_handle_t camera)
{
    test_stats_t stats;
    example_jpeg_decoder_handle_t jpeg_decoder;
    example_jpeg_config_t jpeg_cfg = {0};
    example_image_t jpeg = {0};
    example_image_t yuv = {0};
    int64_t end_us;

    ESP_RETURN_ON_ERROR(open_jpeg_decoder(&jpeg_cfg, &jpeg_decoder), TAG, "open JPEG decoder failed");
    ESP_RETURN_ON_ERROR(camera_connect(camera, jpeg_decoder), TAG, "camera connect failed");

    ESP_LOGI(TAG, "JPEG decode test: %d seconds", TEST_DURATION_SEC);
    stats_begin(&stats);
    end_us = stats.start_us + (int64_t)TEST_DURATION_SEC * 1000000LL;

    while (esp_timer_get_time() < end_us) {
        if (camera_capture_image(&jpeg) != ESP_OK) {
            stats.skip_count++;
            continue;
        }

        int64_t codec_start = esp_timer_get_time();
        if (jpeg_decode(&jpeg, &yuv) == ESP_OK) {
            stats_on_codec(&stats, yuv.size, esp_timer_get_time() - codec_start);
        } else {
            stats.skip_count++;
        }

        buffer_free(&jpeg);
        buffer_free(&yuv);
    }

    stats_end(&stats);
    close_jpeg_decoder(jpeg_decoder);

    if (stats.codec_frame_count == 0) {
        return ESP_FAIL;
    }

    print_codec_stats(&stats, "JPEG Decode");
    return ESP_OK;
}
#endif

void app_main(void)
{
    example_camera_handle_t camera;
    example_camera_config_t cam_cfg = {0};

    esp_err_t ret = open_camera(&cam_cfg, &camera);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "skip test because capture format is not supported");
        return;
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(run_capture_test(camera));

#if CONFIG_EXAMPLE_M2M_MODE_PIPELINE
    /**
     * capture(JPEG image) -> JPEG decode(YUV420 image) -> H.264 encode(H.264 video)
     *
     * The camera sensor should output JPEG image with YUV420 down sampling format.
     */
    ESP_ERROR_CHECK(run_pipeline_test(camera));
#else
#if CONFIG_EXAMPLE_M2M_MODE_ENCODE
#if CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE
    /**
     * capture(JPEG image) -> JPEG encode(JPEG image)
     */
    ESP_ERROR_CHECK(run_jpeg_encode_test(camera));
#elif CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
    /**
     * capture(JPEG image) -> H.264 encode(H.264 video)
     */
    ESP_ERROR_CHECK(run_h264_encode_test(camera));
#endif
#elif CONFIG_EXAMPLE_M2M_MODE_DECODE
    /**
     * capture(JPEG image) -> JPEG decode(YUV420 image)
     *
     * The camera sensor should output JPEG image with YUV420 down sampling format.
     */
    ESP_ERROR_CHECK(run_jpeg_decode_test(camera));
#endif
#endif

    close_camera();
}
