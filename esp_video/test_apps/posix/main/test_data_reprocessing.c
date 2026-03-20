/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "unity.h"

TEST_CASE("RISCV swap byte", "[video]")
{
    int64_t c_time_us = 0;
    int64_t r_time_us = 0;
    int64_t total_bytes = 0;
    extern void esp_video_swap_byte_riscv(void *src, void *dst, uint32_t size);

    for (int i = 0; i < 100; i++) {
        uint8_t *src;
        uint8_t *dst;
        uint8_t *result;
        uint32_t size = (((uint32_t)rand() % 10240) / 32 + 1) * 32;
        /**
         * Add some bytes to the end of the buffer to check if the swap causes the buffer to be overflowed
         */
        uint32_t res = (uint32_t)rand() % 64 + 32;

        src = malloc(size + res);
        TEST_ASSERT_NOT_NULL(src);
        dst = malloc(size + res);
        TEST_ASSERT_NOT_NULL(dst);
        result = malloc(size + res);
        TEST_ASSERT_NOT_NULL(result);

        for (int j = 0; j < size + res; j++) {
            src[j] = rand() % 256;
        }

        int64_t t = esp_timer_get_time();
        for (int j = 0; j < size; j += 4) {
            result[j + 0] = src[j + 1];
            result[j + 1] = src[j + 0];

            result[j + 2] = src[j + 3];
            result[j + 3] = src[j + 2];
        }
        c_time_us += esp_timer_get_time() - t;

        for (int j = 0; j < res; j++) {
            result[size + j] = 0;
        }

        memset(dst, 0, size + res);
        t = esp_timer_get_time();
        esp_video_swap_byte_riscv(src, dst, size);
        r_time_us += esp_timer_get_time() - t;

        TEST_ASSERT_EQUAL_INT(0, memcmp(result, dst, size));
        TEST_ASSERT_EQUAL_INT(0, memcmp(result + size, dst + size, res));

        total_bytes += size;

        free(src);
        free(dst);
        free(result);
    }

    if (c_time_us > 0 && r_time_us > 0) {
        printf("c speed: %lld MB/s, riscv speed: %lld MB/s\n",
               total_bytes * 1000000 / c_time_us / 1024 / 1024,
               total_bytes * 1000000 / r_time_us / 1024 / 1024);
    } else {
        printf("c time: %lld us, riscv time: %lld us, total: %lld bytes\n",
               c_time_us, r_time_us, total_bytes);
    }
}
