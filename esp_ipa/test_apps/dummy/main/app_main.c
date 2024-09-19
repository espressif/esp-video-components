/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

#include "esp_ipa.h"
#include "esp_ipa_detect.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-256)

static size_t before_free_8bit;
static size_t before_free_32bit;

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
    const char *ipa_names[] = {
        "awb.gray",
        "agc.threshold"
    };
    const uint8_t ipa_nums = sizeof(ipa_names) / sizeof(ipa_names[0]);
    esp_ipa_pipeline_handle_t handle = NULL;
    esp_ipa_stats_t stats = {
        .flags = IPA_STATS_FLAGS_AWB | IPA_STATS_FLAGS_AE,
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
        }
    };
    esp_ipa_sensor_t info = {
        .width = 1080,
        .height = 720,
        .cur_exposure = 10e3,
        .max_exposure = 30e3,
        .min_exposure = 10e3,
        .cur_gain = 1.0,
        .max_gain = 16.0,
        .min_gain = 1.0
    };
    esp_ipa_metadata_t metadata = {0};

    esp_ipa_pipeline_set_log(true);
    TEST_ESP_OK(esp_ipa_pipeline_create(ipa_nums, ipa_names, &handle));
    esp_ipa_pipeline_print(handle);
    TEST_ESP_OK(esp_ipa_pipeline_init(handle, &info, &metadata));
    TEST_ESP_OK(esp_ipa_pipeline_process(handle, &stats, &info, &metadata));
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
