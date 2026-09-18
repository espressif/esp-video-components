/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

#include "esp_ipa.h"
#include "esp_ipa_detect.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-256)

#define IPA_TARGET_NAME     "test_apps_dummy"
#define IPA_TARGET_NAME_2   "test_apps_dummy_2"
#define IPA_TARGET_NAME_3   "test_apps_dummy_3"

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
    .max_ae_target_level = 1000,
    .min_ae_target_level = 0,
    .step_ae_target_level = 10,
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

TEST_CASE("enumerate IPA configs of the same sensor", "[IPA]")
{
    const esp_ipa_config_t *first = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    const esp_ipa_config_t *enum0 = esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME, 0);
    const esp_ipa_config_t *enum1 = esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME, 1);
    const esp_ipa_config_t *enum2 = esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME, 2);
    const esp_ipa_config_t *enum3 = esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME, 3);
    const esp_ipa_config_t *dummy2 = esp_ipa_pipeline_get_config(IPA_TARGET_NAME_2);

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_PTR(first, enum0);
    TEST_ASSERT_NOT_NULL(first->ext);
    TEST_ASSERT_EQUAL(1, first->ext->hue);
    TEST_ASSERT_EQUAL_STRING("0", first->description);

    TEST_ASSERT_NOT_NULL(enum1);
    TEST_ASSERT_NOT_EQUAL(first, enum1);
    TEST_ASSERT_NOT_NULL(enum1->ext);
    TEST_ASSERT_EQUAL(10, enum1->ext->hue);
    TEST_ASSERT_EQUAL(20, enum1->ext->brightness);
    TEST_ASSERT_EQUAL_STRING("alt", enum1->description);

    TEST_ASSERT_NOT_NULL(enum2);
    TEST_ASSERT_NOT_EQUAL(enum1, enum2);
    TEST_ASSERT_NOT_NULL(enum2->ext);
    TEST_ASSERT_EQUAL(40, enum2->ext->hue);
    TEST_ASSERT_EQUAL(80, enum2->ext->brightness);
    TEST_ASSERT_EQUAL(12, enum2->ext->stats_region.left);
    TEST_ASSERT_EQUAL_STRING("10", enum2->description);

    TEST_ASSERT_NOT_NULL(enum3);
    TEST_ASSERT_NOT_EQUAL(enum2, enum3);
    TEST_ASSERT_NOT_NULL(enum3->ext);
    TEST_ASSERT_EQUAL(30, enum3->ext->hue);
    TEST_ASSERT_EQUAL(5, enum3->ext->brightness);
    TEST_ASSERT_EQUAL(8, enum3->ext->stats_region.left);
    TEST_ASSERT_EQUAL_STRING("2", enum3->description);

    TEST_ASSERT_NULL(esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME, 4));
    TEST_ASSERT_NULL(esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME, -1));
    TEST_ASSERT_NULL(esp_ipa_pipeline_enum_configs("not_exist_sensor", 0));

    TEST_ASSERT_NOT_NULL(dummy2);
    TEST_ASSERT_EQUAL_PTR(dummy2, esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME_2, 0));
    TEST_ASSERT_NULL(esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME_2, 1));
}

TEST_CASE("set IPA pipeline config dynamically", "[IPA]")
{
    const esp_ipa_config_t *base = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    const esp_ipa_config_t *night = esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME, 3);
    const esp_ipa_config_t *day = esp_ipa_pipeline_enum_configs(IPA_TARGET_NAME, 2);
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    static esp_ipa_config_t cfg;

    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_NOT_NULL(night);
    TEST_ASSERT_NOT_NULL(day);

    TEST_ESP_OK(esp_ipa_pipeline_create(base, &handle));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ipa_pipeline_set_config(NULL, base, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ipa_pipeline_set_config(handle, NULL, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ipa_pipeline_set_config(handle, night, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ipa_pipeline_set_config(handle, base, NULL, &metadata));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ipa_pipeline_set_config(handle, base, &s_esp_ipa_sensor, NULL));

    {
        static const char *bad_names[16];
        memcpy(&cfg, base, sizeof(cfg));
        TEST_ASSERT_TRUE(base->nums <= 16);
        for (int i = 0; i < base->nums; i++) {
            bad_names[i] = base->names[i];
        }
        bad_names[0] = strcmp(handle->ipa_array[0]->name, "esp_ipa_ext") ? "esp_ipa_ext" : "esp_ipa_ian";
        cfg.names = bad_names;
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ipa_pipeline_set_config(handle, &cfg, &s_esp_ipa_sensor, &metadata));

        memcpy(&cfg, base, sizeof(cfg));
        cfg.ext = NULL;
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ipa_pipeline_set_config(handle, &cfg, &s_esp_ipa_sensor, &metadata));
    }

    /* Switch to night ext: handle pointer stays, inherits new IPA/map after init. */
    {
        esp_ipa_pipeline_handle_t same = handle;

        memcpy(&cfg, base, sizeof(cfg));
        cfg.ext = night->ext;
        memset(&metadata, 0, sizeof(metadata));
        TEST_ESP_OK(esp_ipa_pipeline_set_config(handle, &cfg, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_PTR(same, handle);
        TEST_ASSERT_EQUAL_PTR(&cfg, handle->config);
        TEST_ASSERT_EQUAL(30, metadata.hue);
        TEST_ASSERT_EQUAL(5, metadata.brightness);
    }
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    handle = NULL;

    /* Switch to day ext after a live pipeline is already initialized. */
    TEST_ESP_OK(esp_ipa_pipeline_create(base, &handle));
    memset(&metadata, 0, sizeof(metadata));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL(1, metadata.hue);

    memcpy(&cfg, base, sizeof(cfg));
    cfg.ext = day->ext;
    memset(&metadata, 0, sizeof(metadata));
    TEST_ESP_OK(esp_ipa_pipeline_set_config(handle, &cfg, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL(40, metadata.hue);
    TEST_ASSERT_EQUAL(80, metadata.brightness);
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    handle = NULL;

    /* no-env → env: swapped-in modules are already initialized. */
    {
        static esp_ipa_ian_config_t ian_no_env;
        static esp_ipa_ian_luma_config_t luma_no_env;
        esp_ipa_stats_t stats = {
            .flags = IPA_STATS_FLAGS_AE,
            .ae_stats = {
                { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
                { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
                { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
                { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
                { 50 }, { 50 }, { 50 }, { 50 }, { 50 },
            },
        };

        TEST_ASSERT_NOT_NULL(base->ian);
        TEST_ASSERT_NOT_NULL(base->ian->luma);
        TEST_ASSERT_NOT_NULL(base->ian->luma->env);

        memcpy(&luma_no_env, base->ian->luma, sizeof(luma_no_env));
        luma_no_env.env = NULL;
        memcpy(&ian_no_env, base->ian, sizeof(ian_no_env));
        ian_no_env.luma = &luma_no_env;
        memcpy(&cfg, base, sizeof(cfg));
        cfg.ian = &ian_no_env;

        TEST_ESP_OK(esp_ipa_pipeline_create(&cfg, &handle));
        TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
        memset(&metadata, 0, sizeof(metadata));
        TEST_ESP_OK(esp_ipa_pipeline_set_config(handle, base, &s_esp_ipa_sensor, &metadata));
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
        handle = NULL;
    }

    /*
     * Rebuild failure (missing submodule) must not change the live handle contents.
     */
    {
        const esp_ipa_config_t *old_cfg;
        esp_ipa_t *old_ext;

        TEST_ESP_OK(esp_ipa_pipeline_create(base, &handle));
        TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
        old_cfg = handle->config;
        old_ext = handle->ipa_array[0];

        memcpy(&cfg, base, sizeof(cfg));
        cfg.awb = NULL;
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                          esp_ipa_pipeline_set_config(handle, &cfg, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_PTR(old_cfg, handle->config);
        TEST_ASSERT_EQUAL_PTR(old_ext, handle->ipa_array[0]);
        TEST_ASSERT_EQUAL(1, metadata.hue);

        TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
        handle = NULL;
    }
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
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_BLC);

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

    ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    static const struct {
        float gain;
        uint32_t blc_top_left;
        uint32_t blc_top_right;
        uint32_t blc_bottom_left;
        uint32_t blc_bottom_right;
    } test_blc_data [] = {
        {
            .gain = 1.0,
            .blc_top_left = 0,
            .blc_top_right = 0,
            .blc_bottom_left = 0,
            .blc_bottom_right = 0,
        },
        {
            .gain = 17.0,
            .blc_top_left = 32,
            .blc_top_right = 32,
            .blc_bottom_left = 32,
            .blc_bottom_right = 32,
        },
        {
            .gain = 33.0,
            .blc_top_left = 48,
            .blc_top_right = 48,
            .blc_bottom_left = 48,
            .blc_bottom_right = 48,
        },
        {
            .gain = 49.0,
            .blc_top_left = 64,
            .blc_top_right = 64,
            .blc_bottom_left = 64,
            .blc_bottom_right = 64,
        },
        {
            .gain = 50.0,
            .blc_top_left = 0,
            .blc_top_right = 0,
            .blc_bottom_left = 0,
            .blc_bottom_right = 0,
        },
        {
            .gain = 1.0,
            .blc_top_left = 16,
            .blc_top_right = 16,
            .blc_bottom_left = 16,
            .blc_bottom_right = 16,
        },
        {
            .gain = 9.0,
            .blc_top_left = 0,
            .blc_top_right = 0,
            .blc_bottom_left = 0,
            .blc_bottom_right = 0,
        },
        {
            .gain = 17.0,
            .blc_top_left = 32,
            .blc_top_right = 32,
            .blc_bottom_left = 32,
            .blc_bottom_right = 32,
        },
    };

    esp_ipa_sensor_t esp_ipa_sensor_blc = s_esp_ipa_sensor;

    for (int i = 0; i < ARRAY_SIZE(test_blc_data); i++) {
        memset(&metadata, 0, sizeof(esp_ipa_metadata_t));
        esp_ipa_sensor_blc.cur_gain = test_blc_data[i].gain;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &esp_ipa_sensor_blc, &metadata));

        if (test_blc_data[i].blc_top_left != 0) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_BLC, metadata.flags & IPA_METADATA_FLAGS_BLC);
            TEST_ASSERT_TRUE(metadata.blc.stretch);
            TEST_ASSERT_EQUAL_HEX32(test_blc_data[i].blc_top_left, metadata.blc.top_left_chan_offset);
            TEST_ASSERT_EQUAL_HEX32(test_blc_data[i].blc_top_right, metadata.blc.top_right_chan_offset);
            TEST_ASSERT_EQUAL_HEX32(test_blc_data[i].blc_bottom_left, metadata.blc.bottom_left_chan_offset);
            TEST_ASSERT_EQUAL_HEX32(test_blc_data[i].blc_bottom_right, metadata.blc.bottom_right_chan_offset);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_BLC);
        }
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("ACC CCM gain LUT blend test", "[IPA][ACC]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};
    esp_ipa_sensor_t sensor = s_esp_ipa_sensor;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    const float base = 1.2f;

    TEST_ASSERT_NOT_NULL(ipa_config->acc->ccm);
    TEST_ASSERT_TRUE(ipa_config->acc->ccm->gain_lut_enable);
    TEST_ASSERT_NOT_NULL(ipa_config->acc->ccm->gain_lut);
    TEST_ASSERT_EQUAL(3, ipa_config->acc->ccm->gain_lut_size);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &sensor, &metadata));

    esp_ipa_set_float(handle->ipa_array[0], "dummy_gamma_luma", 20.0);
    esp_ipa_set_int32(handle->ipa_array[0], "ct", 2000);

    static const struct {
        float gain;
        float m0;
        float m01;
    } test_data[] = {
        { 1.0f,  1.2f,  1.2f },
        { 8.0f,  1.1f,  0.6f },
        { 16.0f, 1.0f,  0.0f },
        { 12.0f, 1.05f, 0.3f },
        { 4.0f,  1.157142857f, 0.942857143f },
    };

    for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
        metadata.flags = 0;
        sensor.cur_gain = test_data[i].gain;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_CCM, metadata.flags & IPA_METADATA_FLAGS_CCM);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, test_data[i].m0, metadata.ccm.matrix[0][0]);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, test_data[i].m01, metadata.ccm.matrix[0][1]);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, test_data[i].m0, metadata.ccm.matrix[1][1]);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, test_data[i].m0, metadata.ccm.matrix[2][2]);
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME_2);
    TEST_ASSERT_FALSE(ipa_config->acc->ccm->gain_lut_enable);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &sensor, &metadata));

    esp_ipa_set_float(handle->ipa_array[0], "dummy_gamma_luma", 20.0);
    esp_ipa_set_int32(handle->ipa_array[0], "ct", 2000);
    sensor.cur_gain = 16.0f;
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_CCM, metadata.flags & IPA_METADATA_FLAGS_CCM);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, base, metadata.ccm.matrix[0][0]);

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("ACC LSC gain and CT lookup test", "[IPA][ACC]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};
    esp_ipa_sensor_t sensor = s_esp_ipa_sensor;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME_3);
    const esp_ipa_acc_lsc_t *lsc = &ipa_config->acc->lsc_table[0];

    TEST_ASSERT_NOT_NULL(lsc->gain_table);
    TEST_ASSERT_EQUAL(3, lsc->gain_table_size);
    TEST_ASSERT_NULL(lsc->lsc_gain_table);
    TEST_ASSERT_EQUAL(0, lsc->lsc_gain_table_size);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.0f, ipa_config->acc->lsc_disable_gain);

    static const struct {
        float gain;
        int ct;
        uint32_t gain_r;
        uint32_t gain_gr;
        uint32_t gain_gb;
        uint32_t gain_b;
        bool expect_lsc;
    } test_data[] = {
        { 1.0f,  3000, 256, 384, 512, 640, true },
        { 1.0f,  5000,  26,  77, 128, 205, true },
        { 8.0f,  3000, 512, 640, 768, 896, true },
        { 12.0f, 3000, 512, 640, 768, 896, true },
        { 4.0f,  3000, 256, 384, 512, 640, true },
        { 8.0f,  4500,  51, 102, 154, 230, true },
        { 16.0f, 3000, 384, 448, 512, 576, true },
        { 16.0f, 5000, 154, 179, 205, 243, true },
        { 24.0f, 3000,   0,   0,   0,   0, false },
    };

    for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
        TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
        if (test_data[i].expect_lsc) {
            sensor.cur_gain = test_data[i].gain;
            TEST_ESP_OK(esp_ipa_pipeline_init(handle, &sensor, &metadata));
        } else {
            sensor.cur_gain = 1.0f;
            TEST_ESP_OK(esp_ipa_pipeline_init(handle, &sensor, &metadata));
            sensor.cur_gain = test_data[i].gain;
        }
        esp_ipa_set_int32(handle->ipa_array[0], "ct", test_data[i].ct);
        metadata.flags = 0;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));

        if (test_data[i].expect_lsc) {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_LSC, metadata.flags & IPA_METADATA_FLAGS_LSC);
            TEST_ASSERT_TRUE(metadata.lsc.enable);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].gain_r,  metadata.lsc.gain_r[0].val);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].gain_gr, metadata.lsc.gain_gr[0].val);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].gain_gb, metadata.lsc.gain_gb[0].val);
            TEST_ASSERT_EQUAL_HEX32(test_data[i].gain_b,  metadata.lsc.gain_b[0].val);
        } else {
            TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_LSC, metadata.flags & IPA_METADATA_FLAGS_LSC);
            TEST_ASSERT_FALSE(metadata.lsc.enable);
        }

        TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    }
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
            TEST_ASSERT_EQUAL_UINT8(test_gamma_data[i].gamma_y0, metadata.gamma.red.y[0]);
            TEST_ASSERT_EQUAL_UINT8(test_gamma_data[i].gamma_y1, metadata.gamma.red.y[1]);
            TEST_ASSERT_EQUAL_UINT8(test_gamma_data[i].gamma_y13, metadata.gamma.red.y[13]);
            TEST_ASSERT_EQUAL_UINT8(test_gamma_data[i].gamma_y15, metadata.gamma.red.y[15]);
        } else {
            TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GAMMA);
        }
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("Gamma 3-channel test", "[IPA]")
{
    /* Use test_apps_dummy_2 config; runtime selects the last gamma table entry (luma=30.1, shared x/y for R/G/B) */
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats;
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME_2);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    esp_ipa_set_float(handle->ipa_array[0], "dummy_gamma_luma", 26.0f);
    metadata.flags = 0;
    stats.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));

    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_GAMMA, metadata.flags & IPA_METADATA_FLAGS_GAMMA);

    /* Verify Red channel: values from luma=30.1 table entry (shared x/y) */
    TEST_ASSERT_EQUAL_UINT8(0, metadata.gamma.red.x[0]);
    TEST_ASSERT_EQUAL_UINT8(3, metadata.gamma.red.x[1]);
    TEST_ASSERT_EQUAL_UINT8(0, metadata.gamma.red.y[0]);
    TEST_ASSERT_EQUAL_UINT8(20, metadata.gamma.red.y[1]);
    TEST_ASSERT_EQUAL_UINT8(240, metadata.gamma.red.y[13]);
    TEST_ASSERT_EQUAL_UINT8(255, metadata.gamma.red.y[15]);

    /* Verify Green channel: different x/y values from red */
    TEST_ASSERT_EQUAL_UINT8(0, metadata.gamma.green.x[0]);
    TEST_ASSERT_EQUAL_UINT8(4, metadata.gamma.green.x[1]);
    TEST_ASSERT_EQUAL_UINT8(10, metadata.gamma.green.y[0]);
    TEST_ASSERT_EQUAL_UINT8(30, metadata.gamma.green.y[1]);
    TEST_ASSERT_EQUAL_UINT8(244, metadata.gamma.green.y[13]);
    TEST_ASSERT_EQUAL_UINT8(255, metadata.gamma.green.y[15]);

    /* Verify Blue channel: different x/y values from red/green */
    TEST_ASSERT_EQUAL_UINT8(0, metadata.gamma.blue.x[0]);
    TEST_ASSERT_EQUAL_UINT8(5, metadata.gamma.blue.x[1]);
    TEST_ASSERT_EQUAL_UINT8(5, metadata.gamma.blue.y[0]);
    TEST_ASSERT_EQUAL_UINT8(25, metadata.gamma.blue.y[1]);
    TEST_ASSERT_EQUAL_UINT8(242, metadata.gamma.blue.y[13]);
    TEST_ASSERT_EQUAL_UINT8(255, metadata.gamma.blue.y[15]);

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

/*
 * AEN backlight GAMMA: detect from hist low/high ratios + env luma, debounce with
 * detect_count_threshold, then select dedicated backlight gamma table by degree (low+high).
 * Config: test_apps_dummy.json aen.gamma.backlight (detect_count_threshold=2, model=0).
 */
TEST_CASE("AEN gamma backlight enhancement", "[IPA][AEN]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    const esp_ipa_aen_gamma_backlight_config_t *bl;

    TEST_ASSERT_NOT_NULL(ipa_config);
    TEST_ASSERT_NOT_NULL(ipa_config->aen);
    TEST_ASSERT_NOT_NULL(ipa_config->aen->gamma);
    TEST_ASSERT_NOT_NULL(ipa_config->aen->gamma->backlight);
    bl = ipa_config->aen->gamma->backlight;
    TEST_ASSERT_EQUAL_UINT16(2, bl->detect_count_threshold);
    TEST_ASSERT_EQUAL_UINT16(2, bl->detect_count_margin); /* omitted in JSON → equals detect_count_threshold */
    TEST_ASSERT_EQUAL(ESP_IPA_AEN_GAMMA_MODEL_0, bl->model);
    TEST_ASSERT_EQUAL_UINT32(2, bl->gamma_table_size);
    TEST_ASSERT_NOT_NULL(bl->gamma_table);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    /* Backlight hist: low≈0.32 (>0.2), high≈0.21 (>0.15), degree≈0.53 → table[0] */
    memset(&stats, 0, sizeof(stats));
    stats.flags = IPA_STATS_FLAGS_HIST;
    for (int i = 0; i <= 4; i++) {
        stats.hist_stats[i].value = 60;
    }
    for (int i = 5; i <= 13; i++) {
        stats.hist_stats[i].value = 50;
    }
    for (int i = 14; i <= 15; i++) {
        stats.hist_stats[i].value = 100;
    }

    /* Env luma below threshold: never enter backlight mode */
    esp_ipa_set_float(handle->ipa_array[0], "dummy_backlight_env_luma", 1.0f);
    for (int i = 0; i < bl->detect_count_threshold + 2; i++) {
        metadata.flags = 0;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GAMMA);
    }

    /* Env luma above threshold, but count still <= detect_count_threshold for first frames */
    esp_ipa_set_float(handle->ipa_array[0], "dummy_backlight_env_luma", 10.0f);
    for (int i = 0; i < bl->detect_count_threshold; i++) {
        metadata.flags = 0;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GAMMA);
    }

    /* Frame detect_count_threshold+1: count > detect_count_threshold → backlight on, apply table[0] */
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_GAMMA, metadata.flags & IPA_METADATA_FLAGS_GAMMA);
    TEST_ASSERT_EQUAL_UINT8(50, metadata.gamma.red.y[0]);
    TEST_ASSERT_EQUAL_UINT8(60, metadata.gamma.red.y[1]);
    TEST_ASSERT_EQUAL_UINT8(180, metadata.gamma.red.y[13]);
    TEST_ASSERT_EQUAL_UINT8(255, metadata.gamma.red.y[15]);

    /* Stronger backlight degree≈0.80 → table[1] */
    for (int i = 0; i <= 4; i++) {
        stats.hist_stats[i].value = 80;
    }
    for (int i = 5; i <= 13; i++) {
        stats.hist_stats[i].value = 20;
    }
    for (int i = 14; i <= 15; i++) {
        stats.hist_stats[i].value = 150;
    }
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_GAMMA, metadata.flags & IPA_METADATA_FLAGS_GAMMA);
    TEST_ASSERT_EQUAL_UINT8(70, metadata.gamma.red.y[0]);
    TEST_ASSERT_EQUAL_UINT8(80, metadata.gamma.red.y[1]);
    TEST_ASSERT_EQUAL_UINT8(200, metadata.gamma.red.y[13]);
    TEST_ASSERT_EQUAL_UINT8(255, metadata.gamma.red.y[15]);

    /* Leave backlight: middle-only hist fails low/high thresholds.
     * After activate+upgrade frames count==4; need 2 misses to reach count<=detect_count_threshold. */
    memset(stats.hist_stats, 0, sizeof(stats.hist_stats));
    for (int i = 5; i <= 13; i++) {
        stats.hist_stats[i].value = 100;
    }
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    /* Mode off: gamma state reset; dummy_gamma_luma unset → no normal gamma update */
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GAMMA);

    /* After exit, normal gamma path still works via luma_env */
    esp_ipa_set_float(handle->ipa_array[0], "dummy_gamma_luma", 20.1f);
    metadata.flags = 0;
    stats.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_GAMMA, metadata.flags & IPA_METADATA_FLAGS_GAMMA);
    TEST_ASSERT_EQUAL_UINT8(16, metadata.gamma.red.y[0]);
    TEST_ASSERT_EQUAL_UINT8(32, metadata.gamma.red.y[1]);
    TEST_ASSERT_EQUAL_UINT8(224, metadata.gamma.red.y[13]);
    TEST_ASSERT_EQUAL_UINT8(255, metadata.gamma.red.y[15]);

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


TEST_CASE("AWB sub-window config and subwin stats path", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    const esp_ipa_awb_config_t *awb = ipa_config->awb;

    TEST_ASSERT_NOT_NULL(awb);
    TEST_ASSERT_TRUE(awb->enable_sub_win);
    TEST_ASSERT_EQUAL_UINT32(100, awb->min_subwin_wp_counted);
    TEST_ASSERT_EQUAL_UINT32(3, awb->min_subwin_participated);
    TEST_ASSERT_EQUAL_UINT16(40, awb->subwin_green_dark);
    TEST_ASSERT_EQUAL_UINT16(100, awb->subwin_green_mid);
    TEST_ASSERT_EQUAL_UINT16(200, awb->subwin_green_bright);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, awb->subwin_weight[2][2]);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    const uint32_t g_cnt = awb->min_counted;
    stats.flags = IPA_STATS_FLAGS_AWB | IPA_STATS_FLAGS_AWB_SUBWIN;
    stats.awb_stats[0].counted = g_cnt;
    stats.awb_stats[0].sum_r = g_cnt * 100;
    stats.awb_stats[0].sum_g = g_cnt * 100;
    stats.awb_stats[0].sum_b = g_cnt * 100;

    const uint32_t cell_cnt = awb->min_subwin_wp_counted + 800;
    const uint32_t cell_sum = cell_cnt * 100;
    for (int xi = 0; xi < ISP_AWB_SUBWIN_X_NUM; xi++) {
        for (int yj = 0; yj < ISP_AWB_SUBWIN_Y_NUM; yj++) {
            stats.awb_subwin[xi][yj].counted = cell_cnt;
            stats.awb_subwin[xi][yj].sum_r = cell_sum;
            stats.awb_subwin[xi][yj].sum_g = cell_sum;
            stats.awb_subwin[xi][yj].sum_b = cell_sum;
        }
    }

    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_NOT_EQUAL_HEX32(0, metadata.flags & (IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG));

    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("AWB model_2 zone", "[IPA]")
{
    const esp_ipa_config_t *base = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    static esp_ipa_config_t cfg;
    static esp_ipa_awb_config_t awb;
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};

    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_NOT_NULL(base->awb);
    memcpy(&cfg, base, sizeof(cfg));
    memcpy(&awb, base->awb, sizeof(awb));
    awb.model = ESP_IPA_AWB_MODEL_2;
    awb.enable_sub_win = false;
    cfg.awb = &awb;

    /* test_apps_dummy.json awb lines 216–239 (copied into awb) */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, awb.new_w);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, awb.prev_w);
    TEST_ASSERT_FALSE(awb.export_ct);
    TEST_ASSERT_EQUAL_UINT32(1, awb.zone_switch_count);
    TEST_ASSERT_EQUAL_UINT32(500, awb.type_counter_max);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, awb.outlier_rg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, awb.outlier_bg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, awb.zone_hysteresis_ratio);
    TEST_ASSERT_EQUAL_UINT32(1, awb.zones_count);
    TEST_ASSERT_EQUAL_UINT32(1, awb.ref_points_count);
    TEST_ASSERT_NOT_NULL(awb.zones);
    TEST_ASSERT_NOT_NULL(awb.ref_points);
    TEST_ASSERT_EQUAL(ESP_IPA_AWB_ZONE_MCT, awb.zones[0].type);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.45f, awb.zones[0].rg_min);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.70f, awb.zones[0].rg_max);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.45f, awb.zones[0].bg_min);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.70f, awb.zones[0].bg_max);
    TEST_ASSERT_TRUE(awb.zones[0].enabled);
    TEST_ASSERT_EQUAL_UINT32(5200, awb.ref_points[0].ct);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.55f, awb.ref_points[0].rg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.55f, awb.ref_points[0].bg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.35f, awb.ref_points[0].radius);

    memset(&stats, 0, sizeof(stats));
    stats.flags = IPA_STATS_FLAGS_AWB;
    {
        const uint32_t cnt = awb.min_counted;
        stats.awb_stats[0].counted = cnt;
        stats.awb_stats[0].sum_g = cnt * 200U;
        stats.awb_stats[0].sum_r = (uint32_t)((double)stats.awb_stats[0].sum_g * 0.55);
        stats.awb_stats[0].sum_b = (uint32_t)((double)stats.awb_stats[0].sum_g * 0.55);
    }

    TEST_ESP_OK(esp_ipa_pipeline_create(&cfg, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_NOT_EQUAL_HEX32(0, metadata.flags & (IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG));
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

TEST_CASE("AWB model_3 fixed CT", "[IPA]")
{
    const esp_ipa_config_t *base = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    static esp_ipa_config_t cfg;
    static esp_ipa_awb_config_t awb;
    static const esp_ipa_awb_fixed_t s_awb_fixed_presets[] = {
        { .ct = 5000, .rg = 0.5f, .bg = 0.4f },
        { .ct = 6500, .rg = 0.55f, .bg = 0.38f },
    };
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};

    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_NOT_NULL(base->awb);
    memcpy(&cfg, base, sizeof(cfg));
    memcpy(&awb, base->awb, sizeof(awb));
    awb.model = ESP_IPA_AWB_MODEL_3;
    awb.fixed_ct.default_ct = 5000;
    awb.fixed_ct.presets = s_awb_fixed_presets;
    awb.fixed_ct.presets_count = 2;
    cfg.awb = &awb;

    TEST_ESP_OK(esp_ipa_pipeline_create(&cfg, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_RG, metadata.flags & IPA_METADATA_FLAGS_RG);
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_BG, metadata.flags & IPA_METADATA_FLAGS_BG);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, metadata.red_gain);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, metadata.blue_gain);
    TEST_ASSERT_EQUAL_INT32(5000, esp_ipa_get_int32(handle->ipa_array[0], "ct"));

    stats.flags = IPA_STATS_FLAGS_AWB;
    stats.awb_stats[0].counted = awb.min_counted + 10;
    stats.awb_stats[0].sum_r = 1000;
    stats.awb_stats[0].sum_g = 4000;
    stats.awb_stats[0].sum_b = 8000;
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & (IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG));
    TEST_ASSERT_EQUAL_INT32(5000, esp_ipa_get_int32(handle->ipa_array[0], "ct"));

    esp_ipa_awb_set_fixed_ct(handle, 6500);
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG,
                            metadata.flags & (IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG));
    TEST_ASSERT_EQUAL_FLOAT(1.0f / 0.55f, metadata.red_gain);
    TEST_ASSERT_EQUAL_FLOAT(1.0f / 0.38f, metadata.blue_gain);
    TEST_ASSERT_EQUAL_INT32(6500, esp_ipa_get_int32(handle->ipa_array[0], "ct"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ipa_awb_set_fixed_ct(handle, 6200));

    /* Rebuild with a new model-3 table: set_config init selects default_ct. */
    {
        static const esp_ipa_awb_fixed_t s_awb_fixed_presets_v2[] = {
            { .ct = 5000, .rg = 0.6f, .bg = 0.5f },
            { .ct = 6500, .rg = 0.7f, .bg = 0.45f },
        };
        static esp_ipa_awb_config_t awb2;
        static esp_ipa_config_t cfg2;

        memcpy(&awb2, &awb, sizeof(awb2));
        awb2.fixed_ct.default_ct = 6500;
        awb2.fixed_ct.presets = s_awb_fixed_presets_v2;
        awb2.fixed_ct.presets_count = 2;
        memcpy(&cfg2, &cfg, sizeof(cfg2));
        cfg2.awb = &awb2;
        memset(&metadata, 0, sizeof(metadata));
        TEST_ESP_OK(esp_ipa_pipeline_set_config(handle, &cfg2, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG,
                                metadata.flags & (IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG));
        TEST_ASSERT_EQUAL_FLOAT(1.0f / 0.7f, metadata.red_gain);
        TEST_ASSERT_EQUAL_FLOAT(1.0f / 0.45f, metadata.blue_gain);
        TEST_ASSERT_EQUAL_INT32(6500, esp_ipa_get_int32(handle->ipa_array[0], "ct"));
    }

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    handle = NULL;

    /* model 0 → model 3 via set_config applies default_ct in returned metadata. */
    {
        static esp_ipa_config_t cfg_m3;
        static esp_ipa_awb_config_t awb_m3;
        static const esp_ipa_awb_fixed_t presets_m3[] = {
            { .ct = 5000, .rg = 0.5f, .bg = 0.4f },
            { .ct = 6500, .rg = 0.55f, .bg = 0.38f },
        };

        TEST_ASSERT_EQUAL(ESP_IPA_AWB_MODEL_0, base->awb->model);
        TEST_ESP_OK(esp_ipa_pipeline_create(base, &handle));
        memset(&metadata, 0, sizeof(metadata));
        TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

        memcpy(&awb_m3, base->awb, sizeof(awb_m3));
        awb_m3.model = ESP_IPA_AWB_MODEL_3;
        awb_m3.fixed_ct.default_ct = 6500;
        awb_m3.fixed_ct.presets = presets_m3;
        awb_m3.fixed_ct.presets_count = 2;
        memcpy(&cfg_m3, base, sizeof(cfg_m3));
        cfg_m3.awb = &awb_m3;
        memset(&metadata, 0, sizeof(metadata));
        TEST_ESP_OK(esp_ipa_pipeline_set_config(handle, &cfg_m3, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG,
                                metadata.flags & (IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG));
        TEST_ASSERT_EQUAL_FLOAT(1.0f / 0.55f, metadata.red_gain);
        TEST_ASSERT_EQUAL_FLOAT(1.0f / 0.38f, metadata.blue_gain);
        TEST_ASSERT_EQUAL_INT32(6500, esp_ipa_get_int32(handle->ipa_array[0], "ct"));

        TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    }

    /*
     * Failed rebuild (missing submodule) must not replace modules or
     * rewrite the user-selected CT.
     */
    {
        static esp_ipa_config_t cfg_m3;
        static esp_ipa_config_t cfg_fail;
        static esp_ipa_awb_config_t awb_m3;
        static const esp_ipa_awb_fixed_t presets_m3[] = {
            { .ct = 5000, .rg = 0.5f, .bg = 0.4f },
            { .ct = 6500, .rg = 0.55f, .bg = 0.38f },
        };
        esp_ipa_t *old_awb_ipa = NULL;
        int awb_idx = -1;

        memcpy(&awb_m3, base->awb, sizeof(awb_m3));
        awb_m3.model = ESP_IPA_AWB_MODEL_3;
        awb_m3.min_red_gain_step = 0.0f;
        awb_m3.min_blue_gain_step = 0.0f;
        awb_m3.fixed_ct.default_ct = 5000;
        awb_m3.fixed_ct.presets = presets_m3;
        awb_m3.fixed_ct.presets_count = 2;
        memcpy(&cfg_m3, base, sizeof(cfg_m3));
        cfg_m3.awb = &awb_m3;

        TEST_ESP_OK(esp_ipa_pipeline_create(&cfg_m3, &handle));
        memset(&metadata, 0, sizeof(metadata));
        TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
        TEST_ESP_OK(esp_ipa_awb_set_fixed_ct(handle, 6500));
        TEST_ASSERT_EQUAL_INT32(6500, esp_ipa_get_int32(handle->ipa_array[0], "ct"));
        for (int i = 0; i < handle->config->nums; i++) {
            if (!strcmp(handle->ipa_array[i]->name, "esp_ipa_awb")) {
                awb_idx = i;
                old_awb_ipa = handle->ipa_array[i];
                break;
            }
        }
        TEST_ASSERT_TRUE(awb_idx >= 0);
        TEST_ASSERT_NOT_NULL(old_awb_ipa);

        memcpy(&cfg_fail, &cfg_m3, sizeof(cfg_fail));
        cfg_fail.awb = NULL;
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                          esp_ipa_pipeline_set_config(handle, &cfg_fail, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_PTR(old_awb_ipa, handle->ipa_array[awb_idx]);
        TEST_ASSERT_EQUAL_INT32(6500, esp_ipa_get_int32(handle->ipa_array[0], "ct"));

        memset(&stats, 0, sizeof(stats));
        metadata.flags = 0;
        TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
        TEST_ASSERT_EQUAL_HEX32(IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG,
                                metadata.flags & (IPA_METADATA_FLAGS_RG | IPA_METADATA_FLAGS_BG));
        TEST_ASSERT_EQUAL_FLOAT(1.0f / 0.55f, metadata.red_gain);
        TEST_ASSERT_EQUAL_FLOAT(1.0f / 0.38f, metadata.blue_gain);
        TEST_ASSERT_EQUAL_INT32(6500, esp_ipa_get_int32(handle->ipa_array[0], "ct"));

        TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
    }
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

/*
 * esp_ipa_agc_config_t::max_gain: when > 0, clamps output gain after sensor min/max (see maxgain.txt).
 * RAM copy; very dark AE stats so AGC requests gain above the lower cap.
 */
TEST_CASE("AGC max_gain caps sensor gain", "[IPA][AGC]")
{
    const esp_ipa_config_t *base = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    static esp_ipa_config_t cfg;
    static esp_ipa_agc_config_t agc;
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};

    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_NOT_NULL(base->agc);

    memcpy(&cfg, base, sizeof(cfg));
    memcpy(&agc, base->agc, sizeof(agc));
    agc.exposure_adjust_delay = 0;
    agc.exposure_frame_delay = 0;
    agc.gain_frame_delay = 0;
    agc.luma_low = 99;
    agc.luma_high = 101;
    agc.luma_target = 100;
    agc.luma_pwl_enable = false;
    agc.meter_mode = ESP_IPA_AGC_METER_HIGHLIGHT_PRIOR;
    cfg.agc = &agc;

    stats.flags = IPA_STATS_FLAGS_AE;
    for (int j = 0; j < ISP_AE_REGIONS; j++) {
        stats.ae_stats[j].luminance = 1;
    }

    agc.max_gain = 1.1f;
    TEST_ESP_OK(esp_ipa_pipeline_create(&cfg, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_NOT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GN);
    const float gain_lo_cap = metadata.gain;
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.1f, gain_lo_cap);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(s_esp_ipa_sensor.min_gain, gain_lo_cap);
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    agc.max_gain = 2.2f;
    TEST_ESP_OK(esp_ipa_pipeline_create(&cfg, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &s_esp_ipa_sensor, &metadata));
    TEST_ASSERT_NOT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GN);
    const float gain_hi_cap = metadata.gain;
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(2.2f, gain_hi_cap);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(s_esp_ipa_sensor.min_gain, gain_hi_cap);
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    TEST_ASSERT_GREATER_THAN_FLOAT(gain_lo_cap + 0.05f, gain_hi_cap);
}

/*
 * gain_only + fixed_exposure_time: init may set exposure once; process only adjusts gain.
 */
TEST_CASE("AGC gain_only mode only adjusts gain", "[IPA][AGC]")
{
    const esp_ipa_config_t *base = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);
    static esp_ipa_config_t cfg;
    static esp_ipa_agc_config_t agc;
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};
    esp_ipa_sensor_t sensor = s_esp_ipa_sensor;
    const uint32_t fixed_exposure_time = 20000;

    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_NOT_NULL(base->agc);

    memcpy(&cfg, base, sizeof(cfg));
    memcpy(&agc, base->agc, sizeof(agc));
    agc.gain_only = true;
    agc.fixed_exposure_time = fixed_exposure_time;
    agc.exposure_adjust_delay = 0;
    agc.exposure_frame_delay = 0;
    agc.gain_frame_delay = 0;
    agc.inc_gain_ratio = 1.0f;
    agc.dec_gain_ratio = 1.0f;
    agc.luma_low = 99;
    agc.luma_high = 101;
    agc.luma_target = 100;
    agc.luma_pwl_enable = false;
    agc.anti_flicker_mode = ESP_IPA_AGC_ANTI_FLICKER_NONE;
    agc.ac_freq = 0;
    agc.meter_mode = ESP_IPA_AGC_METER_HIGHLIGHT_PRIOR;
    cfg.agc = &agc;

    /* Init with mismatched exposure should apply fixed_exposure_time once */
    sensor.cur_exposure = s_esp_ipa_sensor.cur_exposure;
    sensor.cur_gain = 1.0f;
    TEST_ESP_OK(esp_ipa_pipeline_create(&cfg, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &sensor, &metadata));
    TEST_ASSERT_NOT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_ET);
    TEST_ASSERT_EQUAL_INT32(fixed_exposure_time, metadata.exposure);
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    /* Process with exposure already at fixed_exposure_time: only gain changes */
    sensor.cur_exposure = fixed_exposure_time;
    sensor.cur_gain = 1.0f;
    stats.flags = IPA_STATS_FLAGS_AE;
    for (int j = 0; j < ISP_AE_REGIONS; j++) {
        stats.ae_stats[j].luminance = 50;
    }

    memset(&metadata, 0, sizeof(metadata));
    TEST_ESP_OK(esp_ipa_pipeline_create(&cfg, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &sensor, &metadata));
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_ET);

    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));
    TEST_ASSERT_NOT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GN);
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_ET);
    /* total_gain=2.0, target_total=4.0, exposure_gain=2.0 → gain=2.0 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, metadata.gain);
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));

    /* Bright scene with elevated gain: gain decreases, exposure stays fixed */
    sensor.cur_gain = 4.0f;
    for (int j = 0; j < ISP_AE_REGIONS; j++) {
        stats.ae_stats[j].luminance = 200;
    }
    memset(&metadata, 0, sizeof(metadata));
    TEST_ESP_OK(esp_ipa_pipeline_create(&cfg, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &sensor, &metadata));
    metadata.flags = 0;
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &sensor, &metadata));
    TEST_ASSERT_NOT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_GN);
    TEST_ASSERT_EQUAL_HEX32(0, metadata.flags & IPA_METADATA_FLAGS_ET);
    /* total_gain=8.0, target_total=4.0, exposure_gain=2.0 → gain=2.0 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, metadata.gain);
    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
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

TEST_CASE("IOCTL set/get test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    /**
     * Test AGC status get and set, the initial status is 1, so the get status should be 1.
     * After set status to 0, the get status should be 0.
     */
    int status = 0;
    TEST_ESP_OK(esp_ipa_pipeline_ioctl(handle, ESP_IPA_AGC_G_STATUS, &status));
    TEST_ASSERT_EQUAL_INT32(1, status);

    status = 0;
    TEST_ESP_OK(esp_ipa_pipeline_ioctl(handle, ESP_IPA_AGC_S_STATUS, &status));
    TEST_ESP_OK(esp_ipa_pipeline_ioctl(handle, ESP_IPA_AGC_G_STATUS, &status));
    TEST_ASSERT_EQUAL_INT32(0, status);

    TEST_ESP_OK(esp_ipa_pipeline_destroy(handle));
}

typedef struct {
    esp_ipa_pipeline_handle_t handle;
    const esp_ipa_stats_t *stats;
    esp_ipa_metadata_t *metadata;
    int64_t duration_us;
    int64_t start_us;
} ipa_mt_thread_ctx_t;

static void *ipa_mt_process_thread(void *arg)
{
    ipa_mt_thread_ctx_t *ctx = arg;

    while (esp_timer_get_time() - ctx->start_us < ctx->duration_us) {
        assert(esp_ipa_pipeline_process(ctx->handle, ctx->stats, &s_esp_ipa_sensor, ctx->metadata) == ESP_OK);
    }

    return NULL;
}

static void *ipa_mt_ioctl_read_thread(void *arg)
{
    ipa_mt_thread_ctx_t *ctx = arg;
    int status = 0;

    while (esp_timer_get_time() - ctx->start_us < ctx->duration_us) {
        assert(esp_ipa_pipeline_ioctl(ctx->handle, ESP_IPA_AGC_G_STATUS, &status) == ESP_OK);
    }

    return NULL;
}

static void *ipa_mt_ioctl_write_thread(void *arg)
{
    ipa_mt_thread_ctx_t *ctx = arg;
    int status = 0;

    while (esp_timer_get_time() - ctx->start_us < ctx->duration_us) {
        assert(esp_ipa_pipeline_ioctl(ctx->handle, ESP_IPA_AGC_S_STATUS, &status) == ESP_OK);
        status ^= 1;
    }

    return NULL;
}

TEST_CASE("IOCTL multiple threads test", "[IPA]")
{
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_metadata_t metadata = {0};
    esp_ipa_stats_t stats = {0};
    const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(IPA_TARGET_NAME);

    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_config, &handle));
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &s_esp_ipa_sensor, &metadata));

    /* case1: t0 process 2s, t1 ioctl read 2s, t2 ioctl write 2s */
    pthread_t t0;
    pthread_t t1;
    pthread_t t2;
    int64_t start_us = esp_timer_get_time();
    ipa_mt_thread_ctx_t ctx0 = {
        .handle = handle,
        .stats = &stats,
        .metadata = &metadata,
        .duration_us = 2 * 1000000LL,
        .start_us = start_us,
    };
    ipa_mt_thread_ctx_t ctx1 = {
        .handle = handle,
        .duration_us = 2 * 1000000LL,
        .start_us = start_us,
    };
    ipa_mt_thread_ctx_t ctx2 = {
        .handle = handle,
        .duration_us = 2 * 1000000LL,
        .start_us = start_us,
    };

    TEST_ASSERT_EQUAL(0, pthread_create(&t0, NULL, ipa_mt_process_thread, &ctx0));
    TEST_ASSERT_EQUAL(0, pthread_create(&t1, NULL, ipa_mt_ioctl_read_thread, &ctx1));
    TEST_ASSERT_EQUAL(0, pthread_create(&t2, NULL, ipa_mt_ioctl_write_thread, &ctx2));

    TEST_ASSERT_EQUAL(0, pthread_join(t0, NULL));
    TEST_ASSERT_EQUAL(0, pthread_join(t1, NULL));
    TEST_ASSERT_EQUAL(0, pthread_join(t2, NULL));

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
