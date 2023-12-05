/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// test on macos
// cmd:
//     gcc media.c test.c -g  -o test.o
//     ./test.o


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "esp_media.h"
#include "esp_video_buffer.h"

esp_pipeline_t* pipeline1;
esp_pipeline_t* pipeline2;
esp_pipeline_t* pipeline3; // for async test

void* async_data = NULL;
void* test1(void* arg)
{
    int count = 0;
    esp_media_event_t event;
    memset(&event, 0x0, sizeof(event));

    event.cmd = ESP_MEIDA_EVENT_CMD_START;
    // esp_event_post(ESP_MESH_LITE_EVENT, ESP_MEIDA_EVENT_DATA_RECV, pipeline1->entry_pad, sizeof(esp_pad_t), 0);
    // esp_event_post(ESP_MESH_LITE_EVENT, ESP_MEIDA_EVENT_DATA_RECV, pipeline2->entry_pad, sizeof(esp_pad_t), 0);
    event.pad = esp_pipeline_get_entry_entity(pipeline1);
    esp_media_event_post(&event, 0);

    event.pad = esp_pipeline_get_entry_entity(pipeline2);
    esp_media_event_post(&event, 0);

    event.pad = esp_pipeline_get_entry_entity(pipeline3);
    esp_media_event_post(&event, 0);

    while(1)
    {
        event.cmd = ESP_MEIDA_EVENT_CMD_DATA_RECV;
        count++;
        if (count == 1) {
            event.pad = esp_pipeline_get_entry_entity(pipeline1);
            event.param = esp_video_buffer_alloc_from_pipeline(pipeline1);
        } else if (count == 2) {
            event.pad = esp_pipeline_get_entry_entity(pipeline2);
            event.param = esp_video_buffer_alloc_from_pipeline(pipeline2);
        } else {
            count = 0;
            event.pad = esp_pipeline_get_entry_entity(pipeline3);
            event.param = esp_video_buffer_alloc_from_pipeline(pipeline3);
        }

        printf("%s %d line event.pad=%p, %p, %d\r\n", __func__, __LINE__, event.pad, event.param, count);
        // esp_event_post(ESP_MESH_LITE_EVENT, ESP_MEIDA_EVENT_DATA_RECV, g_event.pad, sizeof(esp_pad_t), 0);
        esp_media_event_post(&event, 0);

        if (async_data) {
            esp_video_buffer_element_free(async_data);
            async_data = NULL;
        }
        sleep(1);
    }
}

esp_err_t entity1_event_cb(struct esp_pad *pad, esp_media_event_cmd_t cmd, void *in, void **out)
{
    // struct esp_video_buffer_element* vb = esp_video_buffer_alloc_from_pipeline(esp_pad_get_pipeline(pad));

    // printf("%s %d line %p\r\n", __func__, __LINE__, vb);
    printf("%s %d line %p pipeline %p\r\n", __func__, __LINE__, in, esp_pad_get_pipeline(pad));

    // esp_video_buffer_element_free(vb);

    return ESP_OK;
}

esp_entity_ops_t entity1_ops = {
    .event_cb = entity1_event_cb
};

esp_err_t entity2_event_cb(struct esp_pad *pad, esp_media_event_cmd_t cmd, void *in, void **out)
{
    printf("%s %d line %p pipeline %p\r\n", __func__, __LINE__, in, esp_pad_get_pipeline(pad));
    if (pad == esp_entity_get_pad_by_index(esp_pad_get_entity(pad), ESP_PAD_TYPE_SINK, 2)) {
        esp_media_event_t event;
        memset(&event, 0x0, sizeof(event));
        event.cmd = ESP_MEIDA_EVENT_CMD_DATA_RECV;
        event.pad = esp_pad_get_bridge_pad(pad);
        event.param = in;
        printf("block pipeline\r\n");
        esp_media_event_post(&event, 0);
        return ESP_ERR_NOT_FINISHED;
    }

    // free, because now there is no user to read
    if ((in != NULL)) {
        esp_video_buffer_element_free(in);
    }
    return ESP_OK;
}

esp_entity_ops_t entity2_ops = {
    .event_cb = entity2_event_cb
};

esp_err_t entity3_event_cb(struct esp_pad *pad, esp_media_event_cmd_t cmd, void *in, void **out)
{
    printf("%s %d line %p pipeline %p\r\n", __func__, __LINE__, in, esp_pad_get_pipeline(pad));

    if (pad == esp_entity_get_pad_by_index(esp_pad_get_entity(pad), ESP_PAD_TYPE_SINK, 2)) {
        async_data = in;
        return ESP_ERR_NOT_FINISHED;
    }

    // free, because now there is no user to read
    if ((in != NULL)) {
        esp_video_buffer_element_free(in);
    }
    return ESP_OK;
}

esp_entity_ops_t entity3_ops = {
    .event_cb = entity3_event_cb
};

void media_loop(void* param);
void app_main(void)
{
    esp_entity_t* entity1 = esp_entity_create(3, 3, 0);
    esp_entity_regist_ops(entity1, &entity1_ops);
    esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SOURCE, 0), esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SINK, 0));
    esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SOURCE, 1), esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SINK, 1));
    esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SOURCE, 2), esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SINK, 2));

    esp_entity_t* entity2 = esp_entity_create(3, 3, 0);
    esp_entity_regist_ops(entity2, &entity2_ops);
    esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity2, ESP_PAD_TYPE_SOURCE, 0), esp_entity_get_pad_by_index(entity2, ESP_PAD_TYPE_SINK, 0));
    esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity2, ESP_PAD_TYPE_SOURCE, 2), esp_entity_get_pad_by_index(entity2, ESP_PAD_TYPE_SINK, 2));

    esp_entity_t* entity3 = esp_entity_create(3, 3, 0);
    esp_entity_regist_ops(entity3, &entity3_ops);
    esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity3, ESP_PAD_TYPE_SOURCE, 0), esp_entity_get_pad_by_index(entity3, ESP_PAD_TYPE_SINK, 0));
    esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity3, ESP_PAD_TYPE_SOURCE, 1), esp_entity_get_pad_by_index(entity3, ESP_PAD_TYPE_SINK, 1));
    esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity3, ESP_PAD_TYPE_SOURCE, 2), esp_entity_get_pad_by_index(entity3, ESP_PAD_TYPE_SINK, 2));

    esp_pads_link_by_index(entity1, 0, entity2, 0);
    esp_pads_link_by_index(entity1, 0, entity3, 0);

    esp_pads_link_by_index(entity1, 1, entity3, 1);

    esp_pads_link_by_index(entity1, 2, entity2, 2);
    esp_pads_link_by_index(entity2, 2, entity3, 2);

    esp_media_t* media = esp_media_create();
    pipeline1 = esp_pipeline_create(5, 10);
    pipeline2 = esp_pipeline_create(5, 10);
    pipeline3 = esp_pipeline_create(5, 10);
    esp_pipeline_update_entry_entity(pipeline1, esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SINK, 0));
    esp_pipeline_update_entry_entity(pipeline2, esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SINK, 1));
    esp_pipeline_update_entry_entity(pipeline3, esp_entity_get_pad_by_index(entity1, ESP_PAD_TYPE_SINK, 2));

    esp_media_add_pipeline(media, pipeline1);
    esp_media_add_pipeline(media, pipeline2);
    esp_media_add_pipeline(media, pipeline3);

    pthread_t pid;
    pthread_create(&pid, NULL, test1, pipeline1);
    pthread_detach(pid);

    media_start();
}
