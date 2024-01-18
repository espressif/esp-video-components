/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "dvp_dma_internal.h"

#define DVP_DMA_PSRAM_TRANS_ALIGN_SIZE             CONFIG_DVP_DMA_PSRAM_ALIGN_SIZE
#define DVP_DMA_SRAM_TRANS_ALIGN_SIZE              (4)

static const char *TAG = "dvp_dma";

esp_err_t dvp_dma_deinit(dvp_dma_t *dvp_dma)
{
    if (dvp_dma->gdma_chan) {
        ESP_RETURN_ON_ERROR(gdma_disconnect(dvp_dma->gdma_chan), TAG, "disconnect dma channel failed");
        ESP_RETURN_ON_ERROR(gdma_del_channel(dvp_dma->gdma_chan), TAG, "delete dma channel failed");
    }

    return ESP_OK;
}

esp_err_t dvp_dma_init(dvp_dma_t *dvp_dma)
{
    esp_err_t ret = ESP_OK;
    //alloc rx gdma channel
    gdma_channel_alloc_config_t rx_alloc_config = {
        .direction = GDMA_CHANNEL_DIRECTION_RX,
    };
    ret = gdma_new_channel(&rx_alloc_config, &(dvp_dma->gdma_chan));
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_GOTO_ON_ERROR(gdma_connect(dvp_dma->gdma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_CAM, 0))), err, TAG, "connect failed");

    gdma_strategy_config_t strategy_config = {
        .auto_update_desc = false,
        .owner_check = false
    };

    ESP_GOTO_ON_ERROR(gdma_apply_strategy(dvp_dma->gdma_chan, &strategy_config), err, TAG, "apply strategy failed");
    // set DMA transfer ability
    gdma_transfer_ability_t ability = {
        .psram_trans_align = DVP_DMA_PSRAM_TRANS_ALIGN_SIZE,
        .sram_trans_align = DVP_DMA_SRAM_TRANS_ALIGN_SIZE,
    };
    ESP_GOTO_ON_ERROR(gdma_set_transfer_ability(dvp_dma->gdma_chan, &ability), err, TAG, "set trans ability failed");

    return ESP_OK;
err:
    dvp_dma_deinit(dvp_dma);
    return ret;
}

esp_err_t dvp_dma_start(dvp_dma_t *dvp_dma, dma_descriptor_t *addr)
{
    return gdma_start(dvp_dma->gdma_chan, (intptr_t)addr);
}

esp_err_t dvp_dma_stop(dvp_dma_t *dvp_dma)
{
    return gdma_stop(dvp_dma->gdma_chan);
}

esp_err_t dvp_dma_reset(dvp_dma_t *dvp_dma)
{
    return gdma_reset(dvp_dma->gdma_chan);
}
