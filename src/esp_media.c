/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_media.h"
#include "esp_video_buffer.h"
#include "esp_video.h"

#include "cJSON.h"

#ifdef CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE_NAME
#define EMBED_FILE_NAME_START   "_binary_" CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE_NAME "_start"
#define EMBED_FILE_NAME_END    "_binary_" CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE_NAME "_end"
extern const char media_config[] asm(EMBED_FILE_NAME_START);
#endif

#define CONFIG_MEDIA_EVENT_NUM                    10
#define CONFIG_MEDIA_TASK_STACK_SIZE              1024*3
#define CONFIG_MEDIA_TASK_PRIORITY                5

#define PAD_MAX_NUM                               10

typedef struct esp_list {
    struct esp_list *pre;
    struct esp_list *next;
    void *payload;
} esp_list_t;

struct esp_media {
    esp_list_t *pipeline;
    int pipelines_num;
    char *name;
};

struct esp_entity {
    esp_list_t *source_pads;
    esp_list_t *sink_pads;
    int source_num;
    int sink_num;
    bool is_user_node;           /* whether the entity is a user node */
    bool is_initial_node;        /* whether the entity is the one the data starts from */
    struct esp_video *device;
    esp_media_t *media;
    esp_entity_ops_t *ops;
};

struct esp_pipeline {
    esp_pad_t *entry_pad;
    esp_media_t *media;
    struct esp_video_buffer *vb;
    size_t vb_size;
    size_t vb_num;
    char *name;
};

struct esp_pad {
    esp_list_t *remote_pads;
    int remote_pads_num;
    esp_list_t *bridge_pads;
    int bridge_pads_num;
    esp_pad_type_t type;
    esp_entity_t *entity;
    esp_pipeline_t *pipeline;
};

// internal
typedef esp_err_t (*entities_iterate_walk_cb_t)(esp_pad_t *pad, void *param);
static esp_err_t entities_iterate_walk_custom(esp_pad_t *pad, entities_iterate_walk_cb_t cb, void *param);
static esp_err_t esp_pads_bind_pipeline_iterate(esp_pad_t *pad, void *param);
static esp_err_t pads_iterate_walk_custom(esp_pad_t *pad, entities_iterate_walk_cb_t cb, void *param);

static esp_err_t entities_walk(esp_pad_t *pad, esp_media_event_cmd_t cmd, struct esp_video_buffer_element *vb);
static esp_err_t esp_media_remove_pipeline(esp_media_t *media, esp_pipeline_t *pipeline);

static QueueHandle_t media_queue = NULL;
static const char *TAG = "esp_media";

static esp_err_t entity_event_default_cb(struct esp_pad *pad, esp_media_event_cmd_t cmd, void *in, void **out)
{
    struct esp_video_buffer_element *element = (struct esp_video_buffer_element *)in;
    esp_entity_t *entity = esp_pad_get_entity(pad);
    if (!entity) {
        return ESP_FAIL;
    }
    if (cmd == ESP_MEIDA_EVENT_CMD_DATA_RECV) {
        void *ptr = esp_video_recvdone_buffer(esp_entity_get_device(entity), element->buffer, element->valid_size, element->valid_offset);
        if (ptr) {
            element = container_of(ptr, struct esp_video_buffer_element, buffer);
            *out = element;
        }

        if (esp_entity_is_user_node(esp_pad_get_entity(pad))) {
            return ESP_ERR_NOT_FINISHED;
        }
    }
    return ESP_OK;
}

static esp_entity_ops_t entity_default_ops = {
    .event_cb = entity_event_default_cb
};

esp_pad_t *initial_pad;
static esp_err_t esp_media_config_loader(const char *config_string)
{
    esp_err_t err_ret = ESP_FAIL;
    esp_media_t *media = NULL;
    cJSON *root = NULL;
    if (!config_string) {
        goto exit;
    }

    media = esp_media_create();
    if (!media) {
        goto exit;
    }

    root = cJSON_Parse(config_string);
    if (!root) {
        goto exit;
    }

    // parse media
    cJSON *media_cjson = cJSON_GetObjectItem(root, "media");
    if (!root) {
        goto exit;
    }

    // parse entities
    cJSON *entities = cJSON_GetObjectItem(media_cjson, "entities");
    if (!entities) {
        goto exit;
    }

    int entities_num = cJSON_GetArraySize(entities);
    if (entities_num == 0) {
        goto exit;
    }

    for (int index = 0; index < entities_num; index++) {
        cJSON *entity_cjson = cJSON_GetArrayItem(entities, index);
        if (!entity_cjson) {
            goto exit;
        }

        cJSON *name = cJSON_GetObjectItem(entity_cjson, "name");
        cJSON *sink_pad_cjson = cJSON_GetObjectItem(entity_cjson, "sink_pads");
        cJSON *source_pad_cjson = cJSON_GetObjectItem(entity_cjson, "source_pads");

        struct esp_video *video = esp_video_device_get_object(name->valuestring);
        if (!video) {
            ESP_LOGE(TAG, "Not found %s device", entity_cjson->valuestring);
            goto exit;
        }
        esp_entity_t *entity = esp_entity_create(source_pad_cjson->valueint, sink_pad_cjson->valueint, video);

        esp_entity_regist_ops(entity, &entity_default_ops);
        cJSON *bridges = cJSON_GetObjectItem(entity_cjson, "bridges");
        int bridges_num = cJSON_GetArraySize(bridges);
        for (int i = 0; i < bridges_num; i++) {
            cJSON *bridge = cJSON_GetArrayItem(bridges, i);
            cJSON *bridge_source_pad = cJSON_GetObjectItem(bridge, "source_pad");
            cJSON *bridge_sink_pad = cJSON_GetObjectItem(bridge, "sink_pad");
            esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity, ESP_PAD_TYPE_SOURCE, bridge_source_pad->valueint),
                                  esp_entity_get_pad_by_index(entity, ESP_PAD_TYPE_SINK, bridge_sink_pad->valueint));
        }
    }

    // parse pipeline
    cJSON *pipelines = cJSON_GetObjectItem(media_cjson, "pipelines");
    if (!pipelines) {
        goto exit;
    }
    int pipelines_num = cJSON_GetArraySize(pipelines);

    for (int index = 0; index < pipelines_num; index++) {
        esp_pipeline_t *pipeline = esp_pipeline_create(0, 0);
        if (!pipeline) {
            goto exit;
        }
        esp_media_add_pipeline(media, pipeline);
        cJSON *pipeline_node = cJSON_GetArrayItem(pipelines, index);
        cJSON *entities = cJSON_GetObjectItem(pipeline_node, "entities");
        int entities_num = cJSON_GetArraySize(entities);
        for (int index = 0; index < entities_num; index++) {
            struct esp_video *video = NULL;
            struct esp_video *pre_video = NULL;
            cJSON *entities_cjson = cJSON_GetArrayItem(entities, index);

            cJSON *pre_entity_name_cjson = cJSON_GetObjectItem(entities_cjson, "pre_entity");
            cJSON *entity_cjson = cJSON_GetObjectItem(entities_cjson, "entity");
            cJSON *sink_pad_cjson = cJSON_GetObjectItem(entities_cjson, "sink_pad");

            if (pre_entity_name_cjson) {
                pre_video = esp_video_device_get_object(pre_entity_name_cjson->valuestring);
                if (!pre_video) {
                    ESP_LOGE(TAG, "Not found pre %s device", entity_cjson->valuestring);
                    goto exit;
                }
            }
            video = esp_video_device_get_object(entity_cjson->valuestring);

            if (!video) {
                ESP_LOGE(TAG, "Not found %s device", entity_cjson->valuestring);
                goto exit;
            }
            esp_pad_t *pad = esp_entity_get_pad_by_index(video->entity, ESP_PAD_TYPE_SINK, sink_pad_cjson->valueint);
            if (pre_video) {
                esp_pad_t *pre_pad = esp_pipeline_get_pad_by_entity(pipeline, pre_video->entity);
                pre_pad = pre_pad->bridge_pads->payload;
                esp_pads_link(pre_pad, pad);
            } else {
                esp_pipeline_update_entry_entity(pipeline, pad);
            }
        }
        esp_media_add_pipeline(media, pipeline);
        initial_pad = pipeline->entry_pad;
    }
    err_ret = ESP_OK;

exit:
    if (root) {
        cJSON_Delete(root);
    }

    if (media) {
        esp_list_t *list = media->pipeline;
        while (list) {
            esp_list_t *temp = list->next;
            esp_pipeline_delete(list->payload);
            list = temp;
        }
    }
    return err_ret;
}

esp_err_t esp_media_async_done_cb(struct esp_pad *pad, esp_media_event_cmd_t cmd, void *in, void *out)
{
    esp_media_event_t event;

    if (in != out) {
        esp_video_buffer_element_free(in);
    }

    memset(&event, 0x0, sizeof(event));
    event.cmd = cmd;
    event.pad = pad;
    event.param = out;

    return esp_media_event_post(&event, portMAX_DELAY);
}

//////// pad
esp_pad_t *esp_pad_get_bridge_pad(esp_pad_t *pad)
{
    return pad ? pad->bridge_pads->payload : NULL;
}

esp_entity_t *esp_pad_get_entity(esp_pad_t *pad)
{
    return pad ? pad->entity : NULL;
}

static esp_err_t __esp_insert_remote_pad(esp_pad_t *pad, esp_pad_t *remote_pad)
{
    esp_list_t *list = (esp_list_t *)malloc(sizeof(esp_list_t));
    if (!list) {
        return ESP_FAIL;
    }
    memset(list, 0x0, sizeof(esp_list_t));
    list->payload = remote_pad;

    esp_list_t *remote_list = pad->remote_pads;
    if (!remote_list) {
        pad->remote_pads = list;
    } else {
        while (remote_list->next) {
        }
        remote_list->next = list;
        list->pre = remote_list;
        list->next = NULL;
    }
    pad->remote_pads_num++;

    return ESP_OK;
}

static esp_err_t __esp_remove_remote_pad(esp_pad_t *pad, esp_pad_t *remote_pad)
{
    if (!pad) {
        return ESP_FAIL;
    }

    esp_list_t *list =  pad->remote_pads;
    while (list) {
        if (list->payload == remote_pad) {
            if (list->pre) {
                list->pre->next = list->next;
                list->next->pre = list->pre;
            } else {
                pad->remote_pads = list->next;
                list->next->pre = NULL;
            }
            free(list);
            pad->remote_pads_num--;
        }
    }

    return ESP_OK;
}

esp_err_t esp_entity_pad_bridge(esp_pad_t *source, esp_pad_t *sink)
{
    if (!source || !sink) {
        return ESP_FAIL;
    }

    if (esp_pad_get_entity(source) != esp_pad_get_entity(sink)) {
        return ESP_FAIL;
    }

    esp_list_t *list = source->bridge_pads;
    esp_list_t *last_list = NULL;
    esp_list_t *new_source_bridge_list = NULL;
    esp_list_t *new_sink_bridge_list = NULL;

    while (list) {
        if (list->payload == sink) {
            break;
        }
        last_list = list;
        list = list->next;
    }

    if (!list) {
        new_source_bridge_list = (esp_list_t *)malloc(sizeof(esp_list_t));
        if (!new_source_bridge_list) {
            return ESP_FAIL;
        }
        memset(new_source_bridge_list, 0x0, sizeof(esp_list_t));
        new_source_bridge_list->payload = sink;
    }

    list = sink->bridge_pads;
    while (list) {
        if (list->payload == source) {
            break;
        }
        last_list = list;
        list = list->next;
    }

    if (!list) {
        new_sink_bridge_list = (esp_list_t *)malloc(sizeof(esp_list_t));
        if (!new_sink_bridge_list) {
            if (new_source_bridge_list) {
                free(new_source_bridge_list);
            }
            return ESP_FAIL;
        }
        memset(new_sink_bridge_list, 0x0, sizeof(esp_list_t));
        new_sink_bridge_list->payload = source;
    }

    if (new_source_bridge_list) {
        if (!source->bridge_pads) {
            source->bridge_pads = new_source_bridge_list;
        } else {
            last_list->next = new_source_bridge_list;
            new_source_bridge_list->pre = last_list;
        }
        source->bridge_pads_num++;
    }

    if (new_sink_bridge_list) {
        if (!sink->bridge_pads) {
            sink->bridge_pads = new_sink_bridge_list;
        } else {
            last_list->next = new_sink_bridge_list;
            new_sink_bridge_list->pre = last_list;
        }
        sink->bridge_pads_num++;
        source->pipeline = sink->pipeline;
    }

    return ESP_OK;
}

esp_err_t esp_pads_link(esp_pad_t *source, esp_pad_t *sink)
{
    esp_entity_t *source_entity = esp_pad_get_entity(source);
    esp_entity_t *sink_entity = esp_pad_get_entity(sink);

    if (!source_entity || !sink_entity) {
        return ESP_FAIL;
    }

    if ((source->type != ESP_PAD_TYPE_SOURCE) || (sink->type != ESP_PAD_TYPE_SINK)) {
        return ESP_FAIL;
    }

    esp_list_t *source_remote = source->remote_pads;
    while (source_remote) {
        if (source_remote->payload == sink) {
            break;
        }
        source_remote = source_remote->next;
    }

    esp_list_t *sink_remote = sink->remote_pads;
    while (sink_remote) {
        if (sink_remote->payload == source) {
            break;
        }
        sink_remote = sink_remote->next;
    }

    // if both source_remote and sink_remote are existed, we need do nothing
    if (!source_remote) {
        if (__esp_insert_remote_pad(source, sink) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    if (!sink_remote) {
        if (__esp_insert_remote_pad(sink, source) != ESP_OK) {
            __esp_remove_remote_pad(source, sink);
            return ESP_FAIL;
        }
    }
    entities_iterate_walk_custom(sink, esp_pads_bind_pipeline_iterate, esp_pad_get_pipeline(source));
    source_entity->is_user_node = false;
    sink_entity->is_initial_node = false;
    return ESP_OK;
}

esp_pad_t *esp_entity_get_pad_by_index(esp_entity_t *entity, esp_pad_type_t type, int index)
{
    esp_pad_t *pad = NULL;
    if (!entity) {
        return pad;
    }

    esp_list_t *list = NULL;
    switch (type) {
    case ESP_PAD_TYPE_SOURCE:
        if (index >= entity->source_num) {
            return pad;
        }
        list = entity->source_pads;
        break;
    case ESP_PAD_TYPE_SINK:
        if (index >= entity->sink_num) {
            return pad;
        }
        list = entity->sink_pads;
        break;
    }

    while (list) {
        if (index == 0) {
            pad = list->payload;
            break;
        }
        index--;
        list = list->next;
    }

    return pad;
}

esp_err_t esp_pads_link_by_index(esp_entity_t *src_entity, int src_pad_index, esp_entity_t *sink_entity, int sink_pad_index)
{
    esp_pad_t *source_pad = esp_entity_get_pad_by_index(src_entity, ESP_PAD_TYPE_SOURCE, src_pad_index);
    esp_pad_t *sink_pad = esp_entity_get_pad_by_index(sink_entity, ESP_PAD_TYPE_SINK, sink_pad_index);

    if (!source_pad || !sink_pad) {
        return ESP_FAIL;
    }

    return esp_pads_link(source_pad, sink_pad);
}

esp_err_t esp_pads_unlink(esp_pad_t *source, esp_pad_t *sink)
{
    if (!source || !sink) {
        return ESP_FAIL;
    }

    esp_list_t *list = source->remote_pads;
    while (list) {
        if (list->payload == sink) {
            if (list->pre) {
                list->pre->next = list->next;
            } else {
                source->remote_pads = list->next;
            }

            if (list->next) {
                list->next->pre = list->pre;
            }
        }
    }

    list = sink->remote_pads;
    while (list) {
        if (list->payload == sink) {
            if (list->pre) {
                list->pre->next = list->next;
            } else {
                sink->remote_pads = list->next;
            }

            if (list->next) {
                list->next->pre = list->pre;
            }
        }
    }

    if (!source->remote_pads) {
        source->entity->is_user_node = true;
    }

    if (!sink->remote_pads) {
        sink->entity->is_initial_node = true;
    }

    return ESP_OK;
}

esp_list_t *esp_pad_list_create(esp_entity_t *entity, uint8_t pad_num, esp_pad_type_t type)
{
    esp_list_t *lists = NULL;
    esp_list_t *list = NULL;
    esp_list_t *last = NULL;

    if (!entity || (pad_num > PAD_MAX_NUM) || ((type != ESP_PAD_TYPE_SINK) && (type != ESP_PAD_TYPE_SOURCE))) {
        return lists;
    }

    for (int loop = 0; loop < pad_num; loop++) {
        list = (esp_list_t *)malloc(sizeof(esp_list_t));
        if (!list) {
            goto PAD_LIST_CREATE_FAIL;
        }
        memset(list, 0x0, sizeof(esp_list_t));

        esp_pad_t *pad = (esp_pad_t *)malloc(sizeof(esp_pad_t));
        if (!pad) {
            free(list);
            goto PAD_LIST_CREATE_FAIL;
        }
        memset(pad, 0x0, sizeof(esp_pad_t));
        pad->entity = entity;
        pad->type = type;

        list->payload = pad;
        if (lists == NULL) {
            lists = list;
            last = lists;
        } else {
            last->next = list;
            list->pre = last;
            last = list;
        }
    }

    return lists;

PAD_LIST_CREATE_FAIL:
    if (lists) {
        while (lists) {
            list = lists;
            lists = list->next;
            free(list->payload);
            free(list);
        }
    }
    return lists;
}

esp_pipeline_t *esp_pad_get_pipeline(esp_pad_t *pad)
{
    if (!pad) {
        return NULL;
    }

    return pad->pipeline;
}

esp_err_t esp_pad_delete(esp_pad_t *pad)
{
    esp_list_t *remote_pads_list = pad->remote_pads;
    if (pad) {
        while (remote_pads_list) {
            esp_list_t *next_list = remote_pads_list->next;
            free(remote_pads_list);
            remote_pads_list = next_list;
        }
        free(pad);
    }
    return ESP_OK;
}

esp_err_t esp_pad_purge(esp_pad_t *pad)
{
    if (!pad) {
        return ESP_FAIL;
    }

    esp_list_t *remote_pads_list = pad->remote_pads;
    while (remote_pads_list) {
        esp_pads_unlink(pad, remote_pads_list->payload);
        remote_pads_list = remote_pads_list->next;
    }

    esp_pad_delete(pad);
    return ESP_OK;
}

esp_err_t esp_pads_list_delete(esp_list_t *list)
{
    if (!list) {
        return ESP_FAIL;
    }

    while (list) {
        esp_pad_t *pad = list->payload;
        esp_pad_purge(pad);
        esp_list_t *pre_list = list;
        list = list->next;
        free(pre_list);
    }

    return ESP_OK;
}

//////// entity
esp_entity_t *esp_entity_create(int source_num, int sink_num, struct esp_video *device)
{
    esp_entity_t *entity = (esp_entity_t *)malloc(sizeof(esp_entity_t));

    if (entity) {
        entity->source_pads = esp_pad_list_create(entity, source_num, ESP_PAD_TYPE_SOURCE);
        entity->sink_pads = esp_pad_list_create(entity, sink_num, ESP_PAD_TYPE_SINK);
        if (((source_num > 0) && !entity->source_pads) || ((sink_num > 0) && !entity->sink_pads)) {
            if (entity->source_pads) {
                esp_pads_list_delete(entity->source_pads);
            }
            if (entity->sink_pads) {
                esp_pads_list_delete(entity->sink_pads);
            }
            free(entity);
            return NULL;
        }
        entity->source_num = source_num;
        entity->sink_num = sink_num;
        entity->device = device;
        if (device) {
            if (device->entity) {
                esp_entity_delete(device->entity);
            }
            device->entity = entity;
        }
        entity->is_user_node = true;
        entity->is_initial_node = true;
    }

    return entity;
}

esp_err_t esp_entity_delete(esp_entity_t *entity)
{
    if (!entity) {
        return ESP_FAIL;
    }

    esp_pads_list_delete(entity->source_pads);
    esp_pads_list_delete(entity->sink_pads);

    if (entity->device) {
        entity->device->entity = NULL;
    }
    free(entity);

    return ESP_OK;
}

esp_err_t esp_entity_regist_ops(esp_entity_t *entity, esp_entity_ops_t *ops)
{
    entity->ops = ops;
    return ESP_OK;
}

bool esp_entity_is_user_node(esp_entity_t *entity)
{
    if (entity) {
        return entity->is_user_node;
    }

    return false;
}

bool esp_entity_is_initial_node(esp_entity_t *entity)
{
    if (entity) {
        return entity->is_initial_node;
    }

    return false;
}

struct esp_video *esp_entity_get_device(esp_entity_t *entity)
{
    if (entity) {
        return entity->device;
    }

    return NULL;
}

esp_pad_t *esp_pipeline_get_pad_by_entity(esp_pipeline_t *pipeline, esp_entity_t *entity)
{
    if (!entity || !pipeline) {
        return NULL;
    }

    esp_pad_t *pad = pipeline->entry_pad;
    esp_entity_t *entity_tmp = esp_pad_get_entity(pad);

    while (entity_tmp) {
        if (entity_tmp == entity) {
            return pad;
        }
#ifdef CONFIG_ESP_VIDEO_MEDIA_TODO
        // Todo: AEG-1081
#endif
        esp_pad_t *remote_pad = pad->remote_pads->payload;
        pad = remote_pad->bridge_pads->payload;
        entity_tmp = esp_pad_get_entity(pad);
    }

    return NULL;
}

static esp_err_t pads_walk(esp_pad_t *pad, esp_media_event_cmd_t cmd, struct esp_video_buffer_element *vb)
{
    esp_list_t *list = pad->remote_pads;
    while (list) {
        esp_pad_t *remote_pad = list->payload;
        if (list->next && vb) {
            // fork vb
            struct esp_video_buffer_element *vb_fork = esp_video_buffer_element_clone(vb);
            entities_walk(remote_pad, cmd, vb_fork);
        } else {
            entities_walk(remote_pad, cmd, vb);
        }
        list = list->next;
    }

    return ESP_OK;
}

static esp_err_t entities_walk(esp_pad_t *pad, esp_media_event_cmd_t cmd, struct esp_video_buffer_element *vb)
{
    if (!pad) {
        return ESP_FAIL;
    }

    if (pad->type == ESP_PAD_TYPE_SOURCE) {
        return pads_walk(pad, cmd, vb);
    }

    esp_entity_t *entity = esp_pad_get_entity(pad);
    if (entity->ops && entity->ops->event_cb) {
        struct esp_video_buffer_element *out = vb;
        esp_err_t err = entity->ops->event_cb(pad, cmd, vb, (void **)&out);

        if (err == ESP_ERR_NOT_FINISHED) {
            return ESP_ERR_NOT_FINISHED;
        }

        if (err != ESP_OK) {
            esp_video_buffer_element_free(vb);
            return ESP_FAIL;
        }

        if ((out != vb) && (vb != NULL)) {
            // free vb
            esp_video_buffer_element_free(vb);
            vb = out;
        }
    }
    esp_list_t *bridge_list = pad->bridge_pads;
    while (bridge_list) {
        esp_pad_t *bridge_pad = bridge_list->payload;
        pads_walk(bridge_pad, cmd, vb);
        bridge_list = bridge_list->next;
    }

    return ESP_OK;
}

static esp_err_t pads_iterate_walk_custom(esp_pad_t *pad, entities_iterate_walk_cb_t cb, void *param)
{
    esp_list_t *list = pad->remote_pads;
    while (list) {
        esp_pad_t *remote_pad = list->payload;
        entities_iterate_walk_custom(remote_pad, cb, param);
        list = list->next;
    }

    return ESP_OK;
}

static esp_err_t entities_iterate_walk_custom(esp_pad_t *pad, entities_iterate_walk_cb_t cb, void *param)
{
    if (!pad || !param) {
        return ESP_FAIL;
    }

    if (pad->type == ESP_PAD_TYPE_SOURCE) {
        pads_iterate_walk_custom(pad, cb, param);
        return ESP_OK;
    }

    cb(pad, param);
    esp_list_t *bridge_list = pad->bridge_pads;

    while (bridge_list) {
        esp_pad_t *bridge_pad = bridge_list->payload;
        pads_iterate_walk_custom(bridge_pad, cb, param);
        bridge_list = bridge_list->next;
    }

    return ESP_OK;
}

//////// pipeline
static void pipeline_walk (esp_pad_t *pad, esp_media_event_cmd_t cmd, struct esp_video_buffer_element *vb)
{
    entities_walk(pad, cmd, vb);
}

esp_pipeline_t *esp_pipeline_create(int vb_num, int vb_size)
{
    esp_pipeline_t *pipeline = (esp_pipeline_t *)malloc(sizeof(esp_pipeline_t));
    if (!pipeline) {
        return NULL;
    }
    memset(pipeline, 0x0, sizeof(esp_pipeline_t));

    // pipeline->vb = esp_video_buffer_create(vb_num, vb_size);
    pipeline->vb_num = vb_num;
    pipeline->vb_size = vb_size;

    return pipeline;
}

struct esp_video_buffer *esp_pipeline_get_video_buffer(esp_pipeline_t *pipeline)
{
    if (pipeline) {
        return pipeline->vb;
    }

    return NULL;
}

static esp_err_t esp_pads_bind_pipeline_iterate(esp_pad_t *pad, void *param)
{
    esp_pad_t *bridge_pad = pad->bridge_pads->payload;
    pad->pipeline = param;
    if (bridge_pad) {
        bridge_pad->pipeline = param;
    }

    return ESP_OK;
}

esp_err_t esp_pipeline_update_entry_entity(esp_pipeline_t *pipeline, esp_pad_t *pad)
{
    if (!pipeline) {
        return ESP_FAIL;
    }

    if (pad->pipeline) {
        return ESP_FAIL;
    }

    entities_iterate_walk_custom(pad, esp_pads_bind_pipeline_iterate, pipeline);

    pipeline->entry_pad = pad;

    return ESP_OK;
}

esp_pad_t *esp_pipeline_get_entry_entity(esp_pipeline_t *pipeline)
{
    if (!pipeline) {
        return NULL;
    }

    return pipeline->entry_pad;
}

esp_err_t esp_pipeline_delete(esp_pipeline_t *pipeline)
{
    if (!pipeline) {
        return ESP_FAIL;
    }
#ifdef CONFIG_ESP_VIDEO_MEDIA_TODO
    // Todo: AEG-1080 cleanup entity
#endif

    esp_media_remove_pipeline(pipeline->media, pipeline);
    if (pipeline->vb) {
        esp_video_buffer_destroy(pipeline->vb);
    }
    free(pipeline);

    return ESP_OK;
}

struct esp_video_buffer_element *esp_video_buffer_alloc_from_pipeline(esp_pipeline_t *pipeline)
{
    if (!pipeline) {
        return NULL;
    }

    return esp_video_buffer_alloc(pipeline->vb);
}

//////// media
esp_err_t IRAM_ATTR esp_media_event_post(esp_media_event_t *event, TickType_t timeout)
{
    if (xPortInIsrContext()) {
        BaseType_t do_yield = pdFALSE;
        if (xQueueSendFromISR(media_queue, event, &do_yield) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }

        if (do_yield) {
            portYIELD_FROM_ISR();
        }
    } else {
        if (xQueueSend(media_queue, event, timeout) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
    }

    return ESP_OK;
}

esp_media_t *esp_media_create(void)
{
    esp_media_t *media = (esp_media_t *)malloc(sizeof(esp_media_t));
    if (!media) {
        return NULL;
    }
    memset(media, 0x0, sizeof(esp_media_t));

    if (media_queue) {
        vQueueDelete(media_queue);
    }
    media_queue = xQueueCreate(CONFIG_MEDIA_EVENT_NUM, sizeof(esp_media_event_t));
    if (!media_queue) {
        free (media);
        return NULL;
    }

    return media;
}

esp_err_t esp_media_add_pipeline(esp_media_t *media, esp_pipeline_t *new_pipeline)
{
    if (!media || !new_pipeline) {
        return ESP_FAIL;
    }

    esp_list_t *list = media->pipeline;

    while (list) {
        esp_pipeline_t *pipeline = list->payload;
        if (pipeline == new_pipeline) {
            break;
        }
    }

    if (!list) {
        esp_list_t *new_list = (esp_list_t *)malloc(sizeof(esp_list_t));

        if (!new_list) {
            return ESP_FAIL;
        }
        memset(new_list, 0x0, sizeof(esp_list_t));
        new_list->payload = new_pipeline;

        if (media->pipeline) {
            list = media->pipeline;
            list->pre = new_list;
            new_list->next = list;
        }
        media->pipeline = list;

        new_pipeline->media = media;
    }

    return ESP_OK;
}

static esp_err_t esp_media_remove_pipeline(esp_media_t *media, esp_pipeline_t *pipeline)
{
    if (!media || !pipeline) {
        return ESP_FAIL;
    }

    esp_list_t *list = media->pipeline;

    while (list) {
        esp_pipeline_t *tmp_pipeline = list->payload;
        if (tmp_pipeline == pipeline) {
            if (list->pre) {
                list->pre->next = list->next;
            } else {
                media->pipeline = list->next;
            }

            if (list->next) {
                list->next->pre = list->pre;
            }

            free(list);
            break;
        }
    }

    return ESP_OK;
}

esp_err_t esp_pipeline_create_video_buffer(esp_pipeline_t *pipeline, int count)
{
    if (!pipeline) {
        return ESP_FAIL;
    }
    pipeline->vb_num = count;
    if (pipeline->vb) {
        esp_video_buffer_destroy(pipeline->vb);
    }

#ifdef CONFIG_ESP_VIDEO_MEDIA_TODO
    // Todo: AEG-1075
#ifdef CONFIG_CAMERA_SIM
    extern unsigned int sim_picture_jpeg_len;
    pipeline->vb_size = sim_picture_jpeg_len + 16;
#else
    pipeline->vb_size = 1843200 + 16;
#endif
#endif
    pipeline->vb = esp_video_buffer_create(pipeline->vb_num,  pipeline->vb_size);
    return ESP_OK;
}

static void media_task(void *param)
{
    esp_media_event_t event;
    while (1) {
        if (xQueueReceive(media_queue, (void * )&event, (TickType_t)portMAX_DELAY)) {
            switch (event.cmd) {
            case ESP_MEIDA_EVENT_CMD_START: {
                // get max buffer size
                // esp_pad_t *pad = event.pad;
                // esp_pipeline_t* pipeline = esp_pad_get_pipeline(pad);
                // if (!pipeline && (esp_pipeline_get_entry_entity(pipeline) == pad)) {
                //     pipeline->vb = esp_video_buffer_create(pipeline->vb_num,  pipeline->vb_size);
                // }
            }
            break;
            case ESP_MEIDA_EVENT_CMD_DATA_RECV:
                break;
            case ESP_MEIDA_EVENT_CMD_STOP:
                break;
            case ESP_MEIDA_EVENT_CMD_IOCTL:
                break;
            }
            pipeline_walk(event.pad, event.cmd, event.param);
        }
    }
}


esp_err_t media_start(void)
{
#ifdef CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE_NAME
    esp_media_config_loader(media_config);
#endif
    if (xTaskCreate(media_task, "media_Task", CONFIG_MEDIA_TASK_STACK_SIZE, NULL, CONFIG_MEDIA_TASK_PRIORITY, NULL) == pdTRUE) {
        return ESP_OK;
    }

    return ESP_FAIL;
}
