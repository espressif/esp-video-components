/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "xclk.h"

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_FREQUENCY          (10000000) // Frequency in Hertz. Set frequency at 10MHz
#define LEDC_OUTPUT_IO          (5) // Define the output GPIO

TEST_CASE("XCLK output operation", "[cam_xclk]")
{
    for (int i = 0; i < 5; i++) {
        TEST_ESP_OK(xclk_enable_out_clock(LEDC_TIMER, LEDC_CHANNEL, LEDC_FREQUENCY, LEDC_OUTPUT_IO));
        vTaskDelay(5 / portTICK_PERIOD_MS);
        TEST_ESP_OK(xclk_disable_out_clock(LEDC_CHANNEL));
    }
}
