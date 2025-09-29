/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

#include "esp_ipa.h"
#include "esp_ipa_detect.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-256)

#define IPA_TARGET_NAME     "test_apps_dummy"
#define IPA_TARGET_NAME_2   "test_apps_dummy_2"

static size_t before_free_8bit;
static size_t before_free_32bit;
static const char *TAG = "dummy";

static esp_ipa_sensor_focus_t s_esp_ipa_sensor_info = {
    .max_pos = 1000,
    .min_pos = 0,
    .cur_pos = 0,
    .step_pos = 1,
    .start_time = 0,
    .period_in_us = 1000,
    .codes_per_step = 1,
};

static const esp_ipa_sensor_t s_esp_ipa_sensor = {
    .width = 1080,
    .height = 720,
    .cur_exposure = 28e3,
    .max_exposure = 97e3,
    .min_exposure = 10e3,
    .cur_gain = 1.0,
    .max_gain = 16.0,
    .min_gain = 1.0,
    .focus_info = &s_esp_ipa_sensor_info,
};

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

TEST_CASE("detect IPAs", "[IPA]")
{
    const int counted = 1000;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME_2);

    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_stats_t stats = {
        .flags = IPA_STATS_FLAGS_AWB | IPA_STATS_FLAGS_AE | IPA_STATS_FLAGS_SHARPEN,
        .awb_stats = {
            {
                .counted = counted,
                .sum_b = counted * 140,
                .sum_g = counted * 200,
                .sum_r = counted * 110,
            }
        },
        .ae_stats = {
            { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
            { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
            { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
            { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
            { 50 }, { 50 }, { 50 }, { 50 }, { 50 }
        },
        .sharpen_stats = {
            .value = 75
        }
    };
    esp_ipa_metadata_t metadata = {0};

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    esp_ipa_pipeline_print(handle);
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Auto color correction test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    esp_ipa_set_int32(handle->ipa_array[0], "ct", 0);

    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_ST);
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_CCM);

    esp_ipa_set_float(handle->ipa_array[0], "dummy_gamma_luma", 15.4);
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_CCM, metadata.flags & IPA_METADATA_FLAGS_CCM);
    TEST_ASSERT_EQUAL_HEX32(0, memcmp(&metadata.ccm, &ipa_config->acc->ccm->luma_low_ccm, sizeof(esp_ipa_ccm_t)));

    esp_ipa_set_float(handle->ipa_array[0], "dummy_gamma_luma", 15.51);
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_CCM, metadata.flags & IPA_METADATA_FLAGS_CCM);

    static const struct {
        int ct;
        float m0;
        uint32_t saturation;
    } test_data[] = {
        {
            .ct = 1510,
            .m0 = 1.2,
            .saturation = 2,
        },
        {
            .ct = 1990,
            .m0 = 1.0,
            .saturation = 0,
        },
        {
            .ct = 10,
            .m0 = 1.1,
            .saturation = 1,
        },
        {
            .ct = 900,
            .m0 = 1.0,
            .saturation = 0,
        },
        {
            .ct = 2100,
            .m0 = 1.2,
            .saturation = 2,
        },
        {
            .ct = 2490,
            .m0 = 1.0,
            .saturation = 0,
        },
        {
            .ct = 1510,
            .m0 = 1.0,
            .saturation = 0,
        },
        {
            .ct = 999,
            .m0 = 1.1,
            .saturation = 1,
        },
        {
            .ct = 2400,
            .m0 = 1.2,
            .saturation = 2,
        },
        {
            .ct = 2600,
            .m0 = 1.3,
            .saturation = 3,
        },
        {
            .ct = 2499,
            .m0 = 1.2,
            .saturation = 2,
        },
        {
            .ct = 4001,
            .m0 = 1.4,
            .saturation = 4,
        },
        {
            .ct = 3410,
            .m0 = 1.3,
            .saturation = 3,
        },
        {
            .ct = 4510,
            .m0 = 1.5,
            .saturation = 5,
        },
        {
            .ct = 4490,
            .m0 = 1.4,
            .saturation = 4,
        },
        {
            .ct = 5000,
            .m0 = 1.5,
            .saturation = 5,
        },
        {
            .ct = 4490,
            .m0 = 1.4,
            .saturation = 4,
        },
        {
            .ct = 5001,
            .m0 = 1.5,
            .saturation = 5,
        },
        {
            .ct = 4490,
            .m0 = 1.4,
            .saturation = 4,
        },
        {
            .ct = 5999,
            .m0 = 1.5,
            .saturation = 5,
        },
        {
            .ct = 4490,
            .m0 = 1.4,
            .saturation = 4,
        },
        {
            .ct = 6000,
            .m0 = 1.5,
            .saturation = 5,
        },
        {
            .ct = 4490,
            .m0 = 1.4,
            .saturation = 4,
        },
        {
            .ct = 10001,
            .m0 = 1.5,
            .saturation = 5,
        },
    };

    for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
        metadata.flags = 0;
        esp_ipa_set_int32(handle->ipa_array[0], "ct", test_data[i].ct);
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        if (test_data[i].m0 != 1.0) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_CCM, metadata.flags & IPA_METADATA_FLAGS_CCM);
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_ST, metadata.flags & IPA_METADATA_FLAGS_ST);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].saturation, metadata.saturation);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].m0, metadata.ccm.matrix[0][0]);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_CCM);
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_ST);
        }
    }

    static const struct {
        int ct;
        uint32_t gain_r;
        uint32_t gain_gr;
        uint32_t gain_gb;
        uint32_t gain_b;
    } test_lsc_data[] = {
        {
            .ct = 1000,
            .gain_r  = 256,
            .gain_gr = 384,
            .gain_gb = 512,
            .gain_b  = 640
        },
        {
            .ct = 2000,
            .gain_r  = 0
        },
        {
            .ct = 3000,
            .gain_r  = 0
        },
        {
            .ct = 4001,
            .gain_r  = 26,
            .gain_gr = 77,
            .gain_gb = 128,
            .gain_b  = 205
        },
        {
            .ct = 5000,
            .gain_r  = 0
        },
        {
            .ct = 6000,
            .gain_r  = 0
        },
        {
            .ct = 3999,
            .gain_r  = 256,
            .gain_gr = 384,
            .gain_gb = 512,
            .gain_b  = 640
        },
    };

    metadata.flags = 0;
    esp_ipa_set_int32(handle->ipa_array[0], "ct", 10000);
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));

    for (int i = 0; i < ARRAY_SIZE(test_lsc_data); i++) {
        metadata.flags = 0;
        esp_ipa_set_int32(handle->ipa_array[0], "ct", test_lsc_data[i].ct);
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        if (test_lsc_data[i].gain_r != 0) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_LSC, metadata.flags & IPA_METADATA_FLAGS_LSC);
            TEST_ASSERT_EQUAL_HEX32(test_lsc_data[i].gain_r,  metadata.lsc.gain_r[0].val);
            TEST_ASSERT_EQUAL_HEX32(test_lsc_data[i].gain_gr, metadata.lsc.gain_gr[0].val);
            TEST_ASSERT_EQUAL_HEX32(test_lsc_data[i].gain_gb, metadata.lsc.gain_gb[0].val);
            TEST_ASSERT_EQUAL_HEX32(test_lsc_data[i].gain_b,  metadata.lsc.gain_b[0].val);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_LSC);
        }
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME_2);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    static const struct {
        int ct;
        float m0;
    } test_data_2[] = {
        {
            .ct = 2000,
            .m0 = 1.2,
        },
        {
            .ct = 100,
            .m0 = 1.1,
        },
        {
            .ct = 500,
            .m0 = 0,
        },
        {
            .ct = 1000,
            .m0 = 0,
        },
        {
            .ct = 1100,
            .m0 = 0,
        },
        {
            .ct = 1499,
            .m0 = 0,
        },
        {
            .ct = 1500,
            .m0 = 1.15,
        },
        {
            .ct = 1999,
            .m0 = 0,
        },
        {
            .ct = 2000,
            .m0 = 1.2,
        },
        {
            .ct = 4000,
            .m0 = 1.4,
        },
        {
            .ct = 4500,
            .m0 = 1.45,
        },
        {
            .ct = 4999,
            .m0 = 0,
        },
        {
            .ct = 5000,
            .m0 = 1.5,
        },
        {
            .ct = 6000,
            .m0 = 0,
        }
    };

    for (int i = 0; i < ARRAY_SIZE(test_data_2); i++) {
        metadata.flags = 0;
        esp_ipa_set_int32(handle->ipa_array[0], "ct", test_data_2[i].ct);
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        if (test_data_2[i].m0 != 0) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_CCM, metadata.flags & IPA_METADATA_FLAGS_CCM);
            TEST_ASSERT_EQUAL_HEX32(test_data_2[i].m0, metadata.ccm.matrix[0][0]);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_CCM);
        }
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Auto denoising test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats;
    esp_ipa_sensor_t sensor = s_esp_ipa_sensor;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    metadata.flags = 0;
    sensor.cur_gain = 0.1;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_CN);
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_SH);

    static const struct {
        float gain;
        uint8_t level;
        float m0;
        float gradient_ratio;
    } test_data[] = {
        {
            .gain = 1.51,
            .level = 2,
            .m0 = 2,
            .gradient_ratio = 2.0
        },
        {
            .gain = 1.99,
            .level = 0,
            .m0 = 0,
            .gradient_ratio = 0.0
        },
        {
            .gain = 0.01,
            .level = 1,
            .m0 = 1,
            .gradient_ratio = 1.0
        },
        {
            .gain = 0.9,
            .level = 0,
            .m0 = 0,
            .gradient_ratio = 0.0
        },
        {
            .gain = 2.1,
            .level = 2,
            .m0 = 2,
            .gradient_ratio = 2.0
        },
        {
            .gain = 2.49,
            .level = 0,
            .m0 = 0,
            .gradient_ratio = 0.0
        },
        {
            .gain = 1.51,
            .level = 0,
            .m0 = 0,
            .gradient_ratio = 0.0
        },
        {
            .gain = 0.9999,
            .level = 1,
            .m0 = 1,
            .gradient_ratio = 1.0
        },
        {
            .gain = 2.4,
            .level = 2,
            .m0 = 2,
            .gradient_ratio = 2.0
        },
        {
            .gain = 2.6,
            .level = 3,
            .m0 = 3,
            .gradient_ratio = 3.0
        },
        {
            .gain = 2.4999,
            .level = 2,
            .m0 = 2,
            .gradient_ratio = 2.0
        },
        {
            .gain = 4.0001,
            .level = 4,
            .m0 = 4,
            .gradient_ratio = 4.0
        },
        {
            .gain = 3.41,
            .level = 3,
            .m0 = 3,
            .gradient_ratio = 3.0
        },
        {
            .gain = 4.51,
            .level = 5,
            .m0 = 5,
            .gradient_ratio = 5.0
        },
        {
            .gain = 4.49,
            .level = 4,
            .m0 = 4,
            .gradient_ratio = 4.0
        },
        {
            .gain = 5.0000,
            .level = 5,
            .m0 = 5,
            .gradient_ratio = 5.0
        },
        {
            .gain = 4.49,
            .level = 4,
            .m0 = 4,
            .gradient_ratio = 4.0
        },
        {
            .gain = 5.0001,
            .level = 5,
            .m0 = 5,
            .gradient_ratio = 5.0
        },
        {
            .gain = 4.49,
            .level = 4,
            .m0 = 4,
            .gradient_ratio = 4.0
        },
        {
            .gain = 5.9999,
            .level = 5,
            .m0 = 5,
            .gradient_ratio = 5.0
        },
        {
            .gain = 4.49,
            .level = 4,
            .m0 = 4,
            .gradient_ratio = 4.0
        },
        {
            .gain = 6.0000,
            .level = 5,
            .m0 = 5,
            .gradient_ratio = 5.0
        },
        {
            .gain = 4.49,
            .level = 4,
            .m0 = 4,
            .gradient_ratio = 4.0
        },
        {
            .gain = 10.0001,
            .level = 5,
            .m0 = 5,
            .gradient_ratio = 5.0
        },
    };

    for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
        metadata.flags = 0;
        stats.flags = 0;
        sensor.cur_gain = test_data[i].gain;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));
        if (test_data[i].level) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_DM, metadata.flags & IPA_METADATA_FLAGS_DM);
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_BF, metadata.flags & IPA_METADATA_FLAGS_BF);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].gradient_ratio, metadata.demosaic.gradient_ratio);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].level, metadata.bf.level);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].m0, metadata.bf.matrix[0][0]);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_DM);
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_BF);
        }
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Auto enhancement test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats;
    esp_ipa_sensor_t sensor = s_esp_ipa_sensor;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    metadata.flags = 0;
    sensor.cur_gain = 0.1;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_CN);
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_SH);

    static const struct {
        float gain;
        uint8_t h_thresh;
        uint8_t m0;
        uint32_t contrast;
    } test_data[] = {
        {
            .gain = 1.51,
            .h_thresh = 2,
            .m0 = 2,
            .contrast = 2
        },
        {
            .gain = 1.99,
            .h_thresh = 0,
            .m0 = 0,
            .contrast = 0
        },
        {
            .gain = 0.01,
            .h_thresh = 1,
            .m0 = 1,
            .contrast = 1
        },
        {
            .gain = 0.9,
            .h_thresh = 0,
            .m0 = 0,
            .contrast = 0
        },
        {
            .gain = 2.1,
            .h_thresh = 2,
            .m0 = 2,
            .contrast = 2
        },
        {
            .gain = 2.49,
            .h_thresh = 0,
            .m0 = 0,
            .contrast = 0
        },
        {
            .gain = 1.51,
            .h_thresh = 0,
            .m0 = 0,
            .contrast = 0
        },
        {
            .gain = 0.9999,
            .h_thresh = 1,
            .m0 = 1,
            .contrast = 1
        },
        {
            .gain = 2.4,
            .h_thresh = 2,
            .m0 = 2,
            .contrast = 2
        },
        {
            .gain = 2.6,
            .h_thresh = 3,
            .m0 = 3,
            .contrast = 3
        },
        {
            .gain = 2.4999,
            .h_thresh = 2,
            .m0 = 2,
            .contrast = 2
        },
        {
            .gain = 4.0001,
            .h_thresh = 4,
            .m0 = 4,
            .contrast = 4
        },
        {
            .gain = 3.41,
            .h_thresh = 3,
            .m0 = 3,
            .contrast = 3
        },
        {
            .gain = 4.51,
            .h_thresh = 5,
            .m0 = 5,
            .contrast = 5
        },
        {
            .gain = 4.49,
            .h_thresh = 4,
            .m0 = 4,
            .contrast = 4
        },
        {
            .gain = 5.0000,
            .h_thresh = 5,
            .m0 = 5,
            .contrast = 5
        },
        {
            .gain = 4.49,
            .h_thresh = 4,
            .m0 = 4,
            .contrast = 4
        },
        {
            .gain = 5.0001,
            .h_thresh = 5,
            .m0 = 5,
            .contrast = 5
        },
        {
            .gain = 4.49,
            .h_thresh = 4,
            .m0 = 4,
            .contrast = 4
        },
        {
            .gain = 5.9999,
            .h_thresh = 5,
            .m0 = 5,
            .contrast = 5
        },
        {
            .gain = 4.49,
            .h_thresh = 4,
            .m0 = 4,
            .contrast = 4
        },
        {
            .gain = 6.0000,
            .h_thresh = 5,
            .m0 = 5,
            .contrast = 5
        },
        {
            .gain = 4.49,
            .h_thresh = 4,
            .m0 = 4,
            .contrast = 4
        },
        {
            .gain = 10.0001,
            .h_thresh = 5,
            .m0 = 5,
            .contrast = 5
        },
    };

    for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
        metadata.flags = 0;
        stats.flags = 0;
        sensor.cur_gain = test_data[i].gain;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));
        if (test_data[i].h_thresh) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_CN, metadata.flags & IPA_METADATA_FLAGS_CN);
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_SH, metadata.flags & IPA_METADATA_FLAGS_SH);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].contrast, metadata.contrast);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].h_thresh, metadata.sharpen.h_thresh);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].m0, metadata.sharpen.matrix[0][0]);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_CN);
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_SH);
        }
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    static const struct {
        float luma;
        uint8_t gamma_y0;
        uint8_t gamma_y1;
        uint8_t gamma_y13;
        uint8_t gamma_y15;
    } test_gamma_data[] = {
        {
            1.2,
            0,
            0,
            0,
            0
        },
        {
            10.1,
            0,
            0,
            0,
            0
        },
        {
            15.1,
            8,
            24,
            216,
            255
        },
        {
            15.3,
            0,
            0,
            0,
            0
        },
        {
            20.1,
            16,
            32,
            224,
            255
        },
        {
            20.3,
            0,
            0,
            0,
            0
        },
        {
            25.1,
            24,
            40,
            232,
            255
        },
        {
            30.1,
            32,
            48,
            240,
            255
        },
        {
            30.3,
            0,
            0,
            0,
            0
        },
        {
            40.1,
            0,
            0,
            0,
            0
        },
    };

    for (int i = 0; i < ARRAY_SIZE(test_gamma_data); i++) {
        esp_ipa_set_float(handle->ipa_array[0], "dummy_gamma_luma", test_gamma_data[i].luma);
        metadata.flags = 0;
        stats.flags = 0;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));

        ESP_LOGV(TAG, "%d: %f %d:%d:%d:%d\n", i, test_gamma_data[i].luma, test_gamma_data[i].gamma_y0, test_gamma_data[i].gamma_y1,
                 test_gamma_data[i].gamma_y13, test_gamma_data[i].gamma_y15);

        if (test_gamma_data[i].gamma_y15) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_GAMMA, metadata.flags & IPA_METADATA_FLAGS_GAMMA);
            TEST_ASSERT_EQUAL_UINT8(test_gamma_data[i].gamma_y0, metadata.gamma.y[0]);
            TEST_ASSERT_EQUAL_UINT8(test_gamma_data[i].gamma_y1, metadata.gamma.y[1]);
            TEST_ASSERT_EQUAL_UINT8(test_gamma_data[i].gamma_y13, metadata.gamma.y[13]);
            TEST_ASSERT_EQUAL_UINT8(test_gamma_data[i].gamma_y15, metadata.gamma.y[15]);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GAMMA);
        }
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Auto white balance test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    for (int i = 0; i < ipa_config->awb->min_counted; i++) {
        metadata.flags = 0;
        stats.flags = 0;
        stats.awb_stats[0].counted = ipa_config->awb->min_counted + 1;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_RG);
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_BG);
    }

    for (int i = 0; i < ipa_config->awb->min_counted; i++) {
        metadata.flags = 0;
        stats.flags = IPA_STATS_FLAGS_AWB;
        stats.awb_stats[0].counted = i;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_RG);
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_BG);
    }

    for (int i = 0; i < 100; i++) {
        metadata.flags = 0;
        stats.awb_stats[0].counted = i + ipa_config->awb->min_counted;
        stats.awb_stats[0].sum_g = 10000000;
        stats.awb_stats[0].sum_r = 10000000 / (1 + ipa_config->awb->min_red_gain_step * ((float)i / 100));
        stats.awb_stats[0].sum_b = 10000000 / (1 + ipa_config->awb->min_blue_gain_step * ((float)i / 100));
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));

        if (i > 0) {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_RG);
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_BG);
        }
    }

    for (int i = 0; i < 100; i++) {
        metadata.flags = 0;
        stats.awb_stats[0].counted = i + ipa_config->awb->min_counted;
        stats.awb_stats[0].sum_g = 10000000;
        stats.awb_stats[0].sum_r = 10000000 / ((0.1 + ipa_config->awb->min_red_gain_step) * i);
        stats.awb_stats[0].sum_b = 10000000 / ((0.1 + ipa_config->awb->min_blue_gain_step) * i);
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));

        if (i > 0) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_RG, metadata.flags & IPA_METADATA_FLAGS_RG);
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_BG, metadata.flags & IPA_METADATA_FLAGS_BG);
        }
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Auto gain control test", "[IPA]")
{
    int seq = 0;
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    const esp_ipa_agc_config_t *agc_config = ipa_config->agc;

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    for (int i = 0; i < 100; i++) {
        metadata.flags = 0;
        stats.flags = 0;
        stats.seq = seq++;
        stats.ae_stats[0].luminance = ((i % 3) + 1 ) * 45;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GN);
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_ET);
    }

    for (int i = 0; i < 100; i++) {
        metadata.flags = 0;
        stats.flags = IPA_STATS_FLAGS_AE;
        stats.seq = seq++;
        for (int j = 0; j < ISP_AE_REGIONS; j++) {
            stats.ae_stats[j].luminance = ((i % 3) + 1 ) * 45;
        }

        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        if ((i % (agc_config->exposure_frame_delay + 1)) == 0) {
            TEST_ASSERT_NOT_EQUAL_HEX32(0, metadata.flags & (IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET));
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & (IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET));
        }
    }

    for (int i = 0; i < 100; i++) {
        uint8_t test_luma;
        int test_count;

        metadata.flags = 0;
        stats.flags = IPA_STATS_FLAGS_AE;
        stats.seq = seq++;
        if (i % 2) {
            test_luma = agc_config->luma_high_threshold + 1;
            test_count = agc_config->luma_high_regions - 1;
        } else {
            test_luma = agc_config->luma_low_threshold - 1;
            test_count = agc_config->luma_low_regions - 1;
        }
        for (int j = 0; j < ISP_AE_REGIONS; j++) {
            if (j < test_count) {
                stats.ae_stats[j].luminance = test_luma;
            } else {
                if (i % 2) {
                    stats.ae_stats[j].luminance = 0;
                } else {
                    stats.ae_stats[j].luminance = 135;
                }
            }
        }
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & (IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET));
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    static esp_ipa_config_t new_ipa_config;
    memcpy(&new_ipa_config, ipa_config, sizeof(new_ipa_config));
    static esp_ipa_agc_config_t new_agc_config;
    memcpy(&new_agc_config, ipa_config->agc, sizeof(new_agc_config));

    new_agc_config.exposure_adjust_delay = 0;
    new_agc_config.exposure_frame_delay = 0;
    new_agc_config.gain_frame_delay = 0;
    new_agc_config.luma_high = 101;
    new_agc_config.luma_low = 99;
    new_agc_config.luma_target = 100;
    new_agc_config.meter_mode = ESP_IPA_AGC_METER_AVERAGE;
    new_ipa_config.agc = &new_agc_config;

    static const struct {
        uint8_t ae_luma;
        float gain;
        uint32_t exposure;
    } test_data_avg[] = {
        {
            .ae_luma = 36,
            .gain = 1.1111,
            .exposure = 70000,
        },
        {
            .ae_luma = 50,
            .gain = 1.12,
            .exposure = 50000,
        },
        {
            .ae_luma = 60,
            .gain = 1.1667,
            .exposure = 40000,
        },
        {
            .ae_luma = 69,
            .gain = 1.0144,
            .exposure = 40000,
        },
        {
            .ae_luma = 80,
            .gain = 1.1667,
            .exposure = 30000,
        },
        {
            .ae_luma = 90,
            .gain = 1.0370,
            .exposure = 30000,
        },
        {
            .ae_luma = 130,
            .gain = 1.0769,
            .exposure = 20000,
        },
        {
            .ae_luma = 145,
            .gain = 1.9310,
            .exposure = 10000,
        },
        {
            .ae_luma = 190,
            .gain = 1.4737,
            .exposure = 10000,
        }
    };

    for (int i = 0; i < ARRAY_SIZE(test_data_avg); i++) {
        TEST_ESP_OK(esp_ipa_pipeline_create(&new_ipa_config, &handle));
        TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

        metadata.flags = 0;
        stats.flags = IPA_STATS_FLAGS_AE;
        stats.seq = seq++;
        for (int j = 0; j < ISP_AE_REGIONS; j++) {
            stats.ae_stats[j].luminance = test_data_avg[i].ae_luma;
        }

        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET, metadata.flags & (IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET));
        TEST_ASSERT_EQUAL_INT32(test_data_avg[i].exposure, metadata.exposure);
        TEST_ASSERT_FLOAT_WITHIN(0.001, test_data_avg[i].gain, metadata.gain);
        TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    }

    new_agc_config.meter_mode = ESP_IPA_AGC_METER_HIGHLIGHT_PRIOR;
    static const struct {
        uint8_t ae_luma;
        float gain;
        uint32_t exposure;
    } test_data_high_light[] = {
        {
            .ae_luma = 36,
            .gain = 1.1111,
            .exposure = 70000,
        },
        {
            .ae_luma = 50,
            .gain = 1.12,
            .exposure = 50000,
        },
        {
            .ae_luma = 60,
            .gain = 1.1667,
            .exposure = 40000,
        },
        {
            .ae_luma = 63,
            .gain = 1.1111,
            .exposure = 40000,
        },
        {
            .ae_luma = 80,
            .gain = 1.1667,
            .exposure = 30000,
        },
        {
            .ae_luma = 90,
            .gain = 1.0370,
            .exposure = 30000,
        },
        {
            .ae_luma = 130,
            .gain = 1.1308,
            .exposure = 20000,
        },
        {
            .ae_luma = 145,
            .gain = 1.0138,
            .exposure = 20000,
        },
        {
            .ae_luma = 190,
            .gain = 1.4737,
            .exposure = 10000,
        }
    };

    for (int i = 0; i < ARRAY_SIZE(test_data_high_light); i++) {
        TEST_ESP_OK(esp_ipa_pipeline_create(&new_ipa_config, &handle));
        TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

        metadata.flags = 0;
        stats.flags = IPA_STATS_FLAGS_AE;
        stats.seq = seq++;
        for (int j = 0; j < ISP_AE_REGIONS; j++) {
            stats.ae_stats[j].luminance = test_data_high_light[i].ae_luma;
        }

        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET, metadata.flags & (IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET));
        TEST_ASSERT_EQUAL_INT32(test_data_high_light[i].exposure, metadata.exposure);
        TEST_ASSERT_FLOAT_WITHIN(0.001, test_data_high_light[i].gain, metadata.gain);
        TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    }

    new_agc_config.meter_mode = ESP_IPA_AGC_METER_LOWLIGHT_PRIOR;
    static const struct {
        uint8_t ae_luma;
        float gain;
        uint32_t exposure;
    } test_data_low_light[] = {
        {
            .ae_luma = 36,
            .gain = 1.1111,
            .exposure = 70000,
        },
        {
            .ae_luma = 50,
            .gain = 1.0267,
            .exposure = 60000,
        },
        {
            .ae_luma = 60,
            .gain = 1.0267,
            .exposure = 50000,
        },
        {
            .ae_luma = 63,
            .gain = 1.2222,
            .exposure = 40000,
        },
        {
            .ae_luma = 80,
            .gain = 1.1667,
            .exposure = 30000,
        },
        {
            .ae_luma = 90,
            .gain = 1.0370,
            .exposure = 30000,
        },
        {
            .ae_luma = 130,
            .gain = 1.0769,
            .exposure = 20000,
        },
        {
            .ae_luma = 145,
            .gain = 1.9310,
            .exposure = 10000,
        },
        {
            .ae_luma = 190,
            .gain = 1.4737,
            .exposure = 10000,
        }
    };

    for (int i = 0; i < ARRAY_SIZE(test_data_low_light); i++) {
        TEST_ESP_OK(esp_ipa_pipeline_create(&new_ipa_config, &handle));
        TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

        metadata.flags = 0;
        stats.flags = IPA_STATS_FLAGS_AE;
        stats.seq = seq++;
        for (int j = 0; j < ISP_AE_REGIONS; j++) {
            stats.ae_stats[j].luminance = test_data_low_light[i].ae_luma;
        }

        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET, metadata.flags & (IPA_METADATA_FLAGS_GN | IPA_METADATA_FLAGS_ET));
        TEST_ASSERT_EQUAL_INT32(test_data_low_light[i].exposure, metadata.exposure);
        TEST_ASSERT_FLOAT_WITHIN(0.001, test_data_low_light[i].gain, metadata.gain);
        TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    }


}

TEST_CASE("Auto sensor target control test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    const esp_ipa_atc_config_t *atc_config = ipa_config->atc;

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_AETL, metadata.flags & IPA_METADATA_FLAGS_AETL);
    TEST_ASSERT_EQUAL_INT32(100, metadata.ae_target_level);

    for (int i = 0; i < atc_config->delay_frames + 1; i++) {
        esp_ipa_stats_t stats = {0};
        metadata.flags = 0;
        esp_ipa_set_float(handle->ipa_array[0], "etc_env_luma", 100.0 + i * atc_config->min_ae_value_step);

        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        if (i < atc_config->delay_frames) {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_AETL);
        } else {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_AETL, metadata.flags & IPA_METADATA_FLAGS_AETL);
            TEST_ASSERT_NOT_EQUAL_INT32(100, metadata.ae_target_level);
        }
    }

    for (int i = 0; i < 10; i++) {
        esp_ipa_stats_t stats = {0};
        metadata.flags = 0;
        esp_ipa_set_float(handle->ipa_array[0], "etc_env_luma", 200.0);

        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        if (i == 0) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_AETL, metadata.flags & IPA_METADATA_FLAGS_AETL);
            TEST_ASSERT_EQUAL_INT32(200, metadata.ae_target_level);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_AETL);
        }
    }

    for (int i = 0; i < 10; i++) {
        esp_ipa_stats_t stats = {0};
        metadata.flags = 0;
        esp_ipa_set_float(handle->ipa_array[0], "etc_env_luma", 300.0);

        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        if (i == 0) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_AETL, metadata.flags & IPA_METADATA_FLAGS_AETL);
            TEST_ASSERT_EQUAL_INT32(300, metadata.ae_target_level);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_AETL);
        }
    }

    for (int i = 0; i < 10; i++) {
        esp_ipa_stats_t stats = {0};
        metadata.flags = 0;
        esp_ipa_set_float(handle->ipa_array[0], "etc_env_luma", 400.0);

        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_AETL);
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Extended configuration test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_HUE, metadata.flags & IPA_METADATA_FLAGS_HUE);
    TEST_ASSERT_EQUAL_INT32(1, metadata.hue);

    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_BR, metadata.flags & IPA_METADATA_FLAGS_BR);
    TEST_ASSERT_EQUAL_INT32(2, metadata.brightness);

    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_SR, metadata.flags & IPA_METADATA_FLAGS_SR);
    TEST_ASSERT_EQUAL_INT32(3, metadata.stats_region.left);
    TEST_ASSERT_EQUAL_INT32(4, metadata.stats_region.top);
    TEST_ASSERT_EQUAL_INT32(5, metadata.stats_region.width);
    TEST_ASSERT_EQUAL_INT32(6, metadata.stats_region.height);

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Set/Get IPAs global variable", "[IPA]")
{
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    /* Test set/get variable in 1 IPA */

    esp_ipa_set_int32(handle->ipa_array[0], "ct", 1000);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_int32(handle->ipa_array[0], "ct"), 1000);

    esp_ipa_set_int32(handle->ipa_array[0], "ct1", 2000);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_int32(handle->ipa_array[0], "ct1"), 2000);

    esp_ipa_set_int32(handle->ipa_array[0], "ct", 3000);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_int32(handle->ipa_array[0], "ct"), 3000);

    esp_ipa_set_float(handle->ipa_array[0], "ct", 1.001);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_float(handle->ipa_array[0], "ct"), 1.001);

    esp_ipa_set_float(handle->ipa_array[0], "ct1", 1.201);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_float(handle->ipa_array[0], "ct1"), 1.201);

    esp_ipa_set_float(handle->ipa_array[0], "ct", 1.302);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_float(handle->ipa_array[0], "ct"), 1.302);

    TEST_ASSERT_TRUE(esp_ipa_has_var(handle->ipa_array[0], "ct"));
    TEST_ASSERT_TRUE(esp_ipa_has_var(handle->ipa_array[0], "ct1"));
    TEST_ASSERT_FALSE(esp_ipa_has_var(handle->ipa_array[0], "ct2"));

    /* Test set/get variable in 2 IPAs */

    esp_ipa_set_int32(handle->ipa_array[0], "ct", 1000);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_int32(handle->ipa_array[1], "ct"), 1000);

    esp_ipa_set_int32(handle->ipa_array[1], "ct1", 2000);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_int32(handle->ipa_array[0], "ct1"), 2000);

    esp_ipa_set_int32(handle->ipa_array[1], "ct", 3000);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_int32(handle->ipa_array[0], "ct"), 3000);

    esp_ipa_set_float(handle->ipa_array[1], "ct", 1.001);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_float(handle->ipa_array[0], "ct"), 1.001);

    esp_ipa_set_float(handle->ipa_array[0], "ct1", 1.201);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_float(handle->ipa_array[1], "ct1"), 1.201);

    esp_ipa_set_float(handle->ipa_array[0], "ct", 1.302);
    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_float(handle->ipa_array[1], "ct"), 1.302);

    TEST_ASSERT_TRUE(esp_ipa_has_var(handle->ipa_array[0], "ct"));
    TEST_ASSERT_TRUE(esp_ipa_has_var(handle->ipa_array[0], "ct1"));
    TEST_ASSERT_FALSE(esp_ipa_has_var(handle->ipa_array[0], "ct2"));

    TEST_ASSERT_TRUE(esp_ipa_has_var(handle->ipa_array[1], "ct"));
    TEST_ASSERT_TRUE(esp_ipa_has_var(handle->ipa_array[1], "ct1"));
    TEST_ASSERT_FALSE(esp_ipa_has_var(handle->ipa_array[1], "ct2"));

    TEST_ASSERT_TRUE(esp_ipa_has_var(handle->ipa_array[0], "ct"));
    TEST_ASSERT_TRUE(esp_ipa_has_var(handle->ipa_array[1], "ct"));

    char *test_ptr1 = "test_ptr1";
    char *test_ptr2 = "test_ptr2";
    esp_ipa_set_ptr(handle->ipa_array[0], test_ptr1, test_ptr1);
    esp_ipa_set_ptr(handle->ipa_array[1], test_ptr2, test_ptr2);
    TEST_ASSERT_EQUAL_PTR(esp_ipa_get_ptr(handle->ipa_array[1], test_ptr1), test_ptr1);
    TEST_ASSERT_EQUAL_PTR(esp_ipa_get_ptr(handle->ipa_array[0], test_ptr2), test_ptr2);

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Customized IPA Process", "[IPA]")
{
    const int counted = 1000;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_stats_t stats = {0};
    esp_ipa_metadata_t metadata = {0};
    int32_t int_val = 0;
    float float_val = 0.01;

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    TEST_ASSERT_EQUAL_INT32(esp_ipa_get_int32(handle->ipa_array[0], "esp_ipa_customized_0_val"), int_val);
    TEST_ASSERT_EQUAL_FLOAT(esp_ipa_get_float(handle->ipa_array[0], "esp_ipa_customized_1_val"), float_val);

    for (int i = 0; i < counted; i++) {
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));

        int_val += 1;
        float_val += 0.01;
        TEST_ASSERT_EQUAL_INT32(esp_ipa_get_int32(handle->ipa_array[0], "esp_ipa_customized_0_val"), int_val);
        TEST_ASSERT_EQUAL_FLOAT(esp_ipa_get_float(handle->ipa_array[0], "esp_ipa_customized_1_val"), float_val);
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}


TEST_CASE("Auto focus test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    const esp_ipa_config_t *ipa_config;

    ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_AF, metadata.flags & IPA_METADATA_FLAGS_AF);
    TEST_ASSERT_EQUAL_INT32(11,  metadata.af.edge_thresh);
    TEST_ASSERT_EQUAL_INT32(11,  ipa_config->af->l1_scan_points_num);
    TEST_ASSERT_EQUAL_INT32(12,  ipa_config->af->l2_scan_points_num);
    TEST_ASSERT_EQUAL_FLOAT(1.6,  ipa_config->af->definition_high_threshold_ratio);
    TEST_ASSERT_EQUAL_FLOAT(0.6,  ipa_config->af->definition_low_threshold_ratio);
    TEST_ASSERT_EQUAL_FLOAT(1.6,  ipa_config->af->luminance_high_threshold_ratio);
    TEST_ASSERT_EQUAL_FLOAT(0.6,  ipa_config->af->luminance_low_threshold_ratio);
    TEST_ASSERT_EQUAL_INT32(500,  ipa_config->af->max_change_time);
    TEST_ASSERT_EQUAL_INT32(500,  ipa_config->af->max_pos);
    TEST_ASSERT_EQUAL_INT32(50,  metadata.af.windows[0].top_left.x);
    TEST_ASSERT_EQUAL_INT32(100, metadata.af.windows[0].top_left.y);
    TEST_ASSERT_EQUAL_INT32(149, metadata.af.windows[0].btm_right.x);
    TEST_ASSERT_EQUAL_INT32(199, metadata.af.windows[0].btm_right.y);
    TEST_ASSERT_EQUAL_INT32(150, metadata.af.windows[1].top_left.x);
    TEST_ASSERT_EQUAL_INT32(200, metadata.af.windows[1].top_left.y);
    TEST_ASSERT_EQUAL_INT32(249, metadata.af.windows[1].btm_right.x);
    TEST_ASSERT_EQUAL_INT32(299, metadata.af.windows[1].btm_right.y);
    TEST_ASSERT_EQUAL_INT32(250, metadata.af.windows[2].top_left.x);
    TEST_ASSERT_EQUAL_INT32(300, metadata.af.windows[2].top_left.y);
    TEST_ASSERT_EQUAL_INT32(349, metadata.af.windows[2].btm_right.x);
    TEST_ASSERT_EQUAL_INT32(399, metadata.af.windows[2].btm_right.y);
    TEST_ASSERT_EQUAL_INT32(1, ipa_config->af->weight_table[0]);
    TEST_ASSERT_EQUAL_INT32(10, ipa_config->af->weight_table[1]);
    TEST_ASSERT_EQUAL_INT32(100, ipa_config->af->weight_table[2]);

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME_2);
    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_AF, metadata.flags & IPA_METADATA_FLAGS_AF);
    TEST_ASSERT_EQUAL_INT32(12,  metadata.af.edge_thresh);
    TEST_ASSERT_EQUAL_INT32(11,  ipa_config->af->l1_scan_points_num);
    TEST_ASSERT_EQUAL_INT32(12,  ipa_config->af->l2_scan_points_num);
    TEST_ASSERT_EQUAL_FLOAT(1.6,  ipa_config->af->definition_high_threshold_ratio);
    TEST_ASSERT_EQUAL_FLOAT(0.6,  ipa_config->af->definition_low_threshold_ratio);
    TEST_ASSERT_EQUAL_FLOAT(1.6,  ipa_config->af->luminance_high_threshold_ratio);
    TEST_ASSERT_EQUAL_FLOAT(0.6,  ipa_config->af->luminance_low_threshold_ratio);
    TEST_ASSERT_EQUAL_INT32(500,  ipa_config->af->max_change_time);
    TEST_ASSERT_EQUAL_INT32(500,  ipa_config->af->max_pos);
    TEST_ASSERT_EQUAL_INT32(50,  metadata.af.windows[0].top_left.x);
    TEST_ASSERT_EQUAL_INT32(100, metadata.af.windows[0].top_left.y);
    TEST_ASSERT_EQUAL_INT32(149, metadata.af.windows[0].btm_right.x);
    TEST_ASSERT_EQUAL_INT32(199, metadata.af.windows[0].btm_right.y);
    TEST_ASSERT_EQUAL_INT32(2, metadata.af.windows[1].top_left.x);
    TEST_ASSERT_EQUAL_INT32(2, metadata.af.windows[1].top_left.y);
    TEST_ASSERT_EQUAL_INT32(5, metadata.af.windows[1].btm_right.x);
    TEST_ASSERT_EQUAL_INT32(5, metadata.af.windows[1].btm_right.y);
    TEST_ASSERT_EQUAL_INT32(2, metadata.af.windows[2].top_left.x);
    TEST_ASSERT_EQUAL_INT32(2, metadata.af.windows[2].top_left.y);
    TEST_ASSERT_EQUAL_INT32(5, metadata.af.windows[2].btm_right.x);
    TEST_ASSERT_EQUAL_INT32(5, metadata.af.windows[2].btm_right.y);
    TEST_ASSERT_EQUAL_INT32(1, ipa_config->af->weight_table[0]);
    TEST_ASSERT_EQUAL_INT32(0, ipa_config->af->weight_table[1]);
    TEST_ASSERT_EQUAL_INT32(0, ipa_config->af->weight_table[2]);

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
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
