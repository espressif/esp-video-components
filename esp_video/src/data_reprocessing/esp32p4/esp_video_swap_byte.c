/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <assert.h>
#include <sys/param.h>
#include "esp_err.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_video_swap_byte.h"

static const char *TAG = "swap_byte";

BITSCRAMBLER_PROGRAM(esp_video_swap_byte, "esp_video_swap_byte");

/**
 * @brief Create video swap byte
 *
 * @return Swapping byte pointer if success or NULL if failed
 */
esp_video_swap_byte_t *esp_video_swap_byte_create(void)
{
    esp_err_t ret;
    esp_video_swap_byte_t *swap_byte;

    swap_byte = heap_caps_calloc(1, sizeof(esp_video_swap_byte_t), MALLOC_CAP_8BIT | MALLOC_CAP_8BIT);
    if (!swap_byte) {
        return NULL;
    }

    bitscrambler_config_t bs_config = {
        .dir = BITSCRAMBLER_DIR_RX,
        .attach_to = SOC_BITSCRAMBLER_ATTACH_LCD_CAM,
    };

    ESP_GOTO_ON_ERROR(bitscrambler_new(&bs_config, &swap_byte->bs),
                      exit_0, TAG, "Failed to create LCD_CAM bitscrambler");

    ESP_GOTO_ON_ERROR(bitscrambler_enable(swap_byte->bs), exit_0, TAG, "Failed to enable LCD_CAM bitscrambler");

    ESP_GOTO_ON_ERROR(bitscrambler_load_program(swap_byte->bs, esp_video_swap_byte), exit_1, TAG,
                      "Failed to load bitscrambler program");

    (void)ret;

    return swap_byte;

exit_1:
    bitscrambler_free(swap_byte->bs);
exit_0:
    heap_caps_free(swap_byte);
    return NULL;
}

/**
 * @brief Start video swap byte
 *
 * @param swap_byte    Video swap byte object pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t IRAM_ATTR esp_video_swap_byte_start(esp_video_swap_byte_t *swap_byte)
{
    esp_err_t ret;

    if ((ret = bitscrambler_reset(swap_byte->bs)) != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "Failed to reset LCD_CAM bitscrambler");
        return ret;
    }

    if ((ret = bitscrambler_start(swap_byte->bs)) != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "Failed to start LCD_CAM bitscrambler");
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Free video swap byte
 *
 * @param swap byte    Video swap byte object pointer
 *
 * @return None
 */
void esp_video_swap_byte_free(esp_video_swap_byte_t *swap_byte)
{
    bitscrambler_reset(swap_byte->bs);
    bitscrambler_disable(swap_byte->bs);
    bitscrambler_free(swap_byte->bs);
    swap_byte->bs = NULL;
}
