/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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

#include "esp_video_vfs.h"

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

typedef struct esp_video esp_user_node_device_t;

typedef struct esp_list {
    struct esp_list *pre;
    struct esp_list *next;
    void *payload;
} esp_list_t;

struct esp_media {
    esp_list_t *pipeline;
    int pipelines_num;
    esp_list_t *entities;
    char *name;
};

struct esp_entity {
    esp_list_t *source_pads;
    esp_list_t *sink_pads;
    int source_num;
    int sink_num;
    bool is_initial_node;        /* whether the entity is the one the data starts from */
    struct esp_video *device;
    esp_user_node_device_t *user_node;
    esp_media_t *media;
    esp_entity_ops_t *ops;
};

struct esp_pipeline {
    esp_pad_t *entry_pad;
    esp_pad_t *last_pad;
    esp_media_t *media;
    struct esp_video_buffer *vb;
    char *name;
};

struct esp_pad {
    esp_list_t *remote_pads;
    int remote_pads_num;
    esp_pad_t *bridge_pad;
    esp_pad_type_t type;
    esp_entity_t *entity;
    esp_pipeline_t *pipeline;
};

typedef struct {
    int cmd;
    va_list args;
} __user_node_ioctl_param_t;

// internal
typedef esp_err_t (*entities_iterate_walk_cb_t)(esp_pad_t *pad, void *param);
static esp_err_t esp_pipeline_entities_iterate_walk_custom(esp_pad_t *pad, entities_iterate_walk_cb_t cb,
        entities_iterate_walk_cb_t completed_cb, void *param, bool reverse);
static esp_err_t esp_pads_bind_pipeline_iterate(esp_pad_t *pad, void *param);

static esp_err_t entities_walk(esp_pad_t *pad, esp_media_event_cmd_t cmd, struct esp_video_buffer_element *vb);
static esp_err_t esp_media_cleanup_individual_entities(esp_media_t *media);

static QueueHandle_t media_queue = NULL;
static const char *TAG = "esp_media";

// This API can not meet AEG-1182, but currently, it is enough
void IRAM_ATTR esp_video_media_recvdone_buffer(struct esp_video *video, void *buffer, size_t size, uint32_t  offset)
{
    struct esp_video_buffer_element *element = esp_video_buffer_get_element_by_buffer(video->buffer, buffer);
    esp_pad_t *pad = NULL;
    esp_media_event_t event;

    for (int i = 0; ; i++) {
        pad = esp_entity_get_pad_by_index(video->entity, ESP_PAD_TYPE_SINK, i);
        if (!pad) {
            break;
        }

        if (esp_pad_get_pipeline(pad)) {
            memset(&event, 0x0, sizeof(event));
            event.cmd = ESP_MEIDA_EVENT_CMD_DATA_RECV;
            event.pad = pad;
            event.param = element;
            element->valid_size = size;
            esp_media_event_post(&event, 0);
        }
    }
}

static esp_err_t user_node_device_iterate_init(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = esp_pad_get_entity(pad);
    struct esp_video *video = entity->device;
    if (!esp_video_open(video->dev_name)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t user_node_device_init(esp_user_node_device_t *user_node_device)
{
    esp_pad_t *pad = (esp_pad_t *)user_node_device->priv;

    int ret = esp_pipeline_entities_iterate_walk_custom(pad, user_node_device_iterate_init, NULL, NULL, true);
    return ret;
}

static esp_err_t user_node_device_iterate_deinit(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = esp_pad_get_entity(pad);
    struct esp_video *video = entity->device;

    return esp_video_close(video);
}

static esp_err_t user_node_device_deinit(esp_user_node_device_t *user_node_device)
{
    esp_pad_t *pad = (esp_pad_t *)user_node_device->priv;

    return esp_pipeline_entities_iterate_walk_custom(pad, user_node_device_iterate_deinit, NULL, NULL, true);
}

static esp_err_t user_node_device_iterate_start_capture(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = esp_pad_get_entity(pad);
    struct esp_video *video = entity->device;
    return esp_video_start_capture(video);
}

static esp_err_t user_node_device_start_capture(esp_user_node_device_t *user_node_device)
{
    esp_pad_t *pad = (esp_pad_t *)user_node_device->priv;

    return esp_pipeline_entities_iterate_walk_custom(pad, user_node_device_iterate_start_capture, NULL, NULL, true);
}

static esp_err_t user_node_device_iterate_stop_capture(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = esp_pad_get_entity(pad);
    struct esp_video *video = entity->device;
    return esp_video_stop_capture(video);
}

static esp_err_t user_node_device_stop_capture(esp_user_node_device_t *user_node_device)
{
    esp_pad_t *pad = (esp_pad_t *)user_node_device->priv;
    return esp_pipeline_entities_iterate_walk_custom(pad, user_node_device_iterate_stop_capture, NULL, NULL, true);
}

static esp_err_t user_node_device_iterate_set_format(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = esp_pad_get_entity(pad);
    struct esp_video *video = entity->device;
    return esp_video_set_format(video, param);
}

static esp_err_t user_node_device_set_format(esp_user_node_device_t *user_node_device, const struct esp_video_format *format)
{
    esp_pad_t *pad = (esp_pad_t *)user_node_device->priv;
    return esp_pipeline_entities_iterate_walk_custom(pad, user_node_device_iterate_set_format, NULL, (void *)format, true);
}

static esp_err_t user_node_device_iterate_get_capability(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = esp_pad_get_entity(pad);
    struct esp_video *video = entity->device;
    return esp_video_get_capability(video, param);
}

static esp_err_t user_node_device_get_capability(esp_user_node_device_t *user_node_device, struct esp_video_capability *capability)
{
    esp_pad_t *pad = (esp_pad_t *)user_node_device->priv;

    return esp_pipeline_entities_iterate_walk_custom(pad, user_node_device_iterate_get_capability, NULL, capability, true);
}

static esp_err_t user_node_device_iterate_get_description(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = esp_pad_get_entity(pad);
    struct esp_video *video = entity->device;
    return esp_video_get_description(video, param, 0);
}

static esp_err_t user_node_device_get_description(esp_user_node_device_t *user_node_device, char *buffer, uint32_t size)
{
    esp_pad_t *pad = (esp_pad_t *)user_node_device->priv;
    // ToDo Check size
    return esp_pipeline_entities_iterate_walk_custom(pad, user_node_device_iterate_get_description, NULL, buffer, true);
}

static esp_err_t user_node_device_iterate_ioctl(esp_pad_t *pad, void *param)
{
    __user_node_ioctl_param_t *ioctl_param = param;
    return esp_video_ioctl(pad->entity->device, ioctl_param->cmd, ioctl_param->args);
}

static esp_err_t get_video_buffer_size_cb(esp_pad_t *pad, void *param);
esp_err_t esp_video_media_ioctl(struct esp_video *user_device, int cmd, va_list args)
{
    bool iterate = true;
    int ret = ESP_OK;

    if ((cmd == VIDIOC_DQBUF)
            || (cmd == VIDIOC_QUERYBUF)
            || (cmd == VIDIOC_MMAP)) {
        iterate = false;
        ret = esp_video_ioctl(user_device, cmd, args);
    } else if (cmd == VIDIOC_REQBUFS) {
        memset(&user_device->buf_info, 0x0, sizeof(user_device->buf_info));
        user_device->buf_info.align_size = 1;
        esp_pipeline_entities_iterate_walk_custom(user_device->priv, get_video_buffer_size_cb, NULL, &user_device->buf_info, true);
        ret = esp_video_ioctl(user_device, cmd, args);
    } else if (cmd == VIDIOC_QBUF) {
        esp_video_ioctl(user_device, cmd, args);

        __user_node_ioctl_param_t ioctl_param;
        memset(&ioctl_param, 0x0, sizeof(ioctl_param));
        ioctl_param.cmd = cmd;
        ioctl_param.args = args;

        ret = esp_pipeline_entities_iterate_walk_custom(user_device->priv, user_node_device_iterate_ioctl, NULL, &ioctl_param, true);
    }

    if (iterate && (ret == ESP_OK)) {
        __user_node_ioctl_param_t ioctl_param;
        memset(&ioctl_param, 0x0, sizeof(ioctl_param));
        ioctl_param.cmd = cmd;
        ioctl_param.args = args;

        ret = esp_pipeline_entities_iterate_walk_custom(user_device->priv, user_node_device_iterate_ioctl, NULL, &ioctl_param, true);
    }

    if (cmd == VIDIOC_S_FMT) {
        memset(&user_device->buf_info, 0x0, sizeof(user_device->buf_info));
        user_device->buf_info.align_size = 1;
        esp_pipeline_entities_iterate_walk_custom(user_device->priv, get_video_buffer_size_cb, NULL, &user_device->buf_info, true);
    }

    return ret;
}

static const struct esp_video_ops s_user_node_device_ops = {
    .init          = user_node_device_init,
    .deinit        = user_node_device_deinit,
    .start_capture = user_node_device_start_capture,
    .stop_capture  = user_node_device_stop_capture,
    .set_format    = user_node_device_set_format,
    .capability    = user_node_device_get_capability,
    .description   = user_node_device_get_description
};

static esp_err_t entity_event_default_cb(struct esp_pad *pad, esp_media_event_cmd_t cmd, void *in, void **out)
{
    struct esp_video_buffer_element *element = (struct esp_video_buffer_element *)in;
    esp_entity_t *entity = esp_pad_get_entity(pad);
    if (!entity) {
        return ESP_FAIL;
    }
    if (cmd == ESP_MEIDA_EVENT_CMD_DATA_RECV) {
        struct esp_video *video = esp_entity_get_device(entity);

        esp_video_done_element(video, element);
        *out = element;

        if (pad->entity->user_node) {
            esp_video_done_element(entity->user_node, element);
            return ESP_ERR_NOT_FINISHED;
        }
    }
    return ESP_OK;
}

static esp_entity_ops_t entity_default_ops = {
    .event_cb = entity_event_default_cb
};

esp_err_t esp_media_config_loader(const char *config_string)
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
        if (!sink_pad_cjson || !source_pad_cjson) {
            goto exit;
        }

        struct esp_video *video = esp_video_device_get_object(name->valuestring);

        if (!video) {
            ESP_LOGE(TAG, "Not found %s device", name->valuestring);
            goto exit;
        }
        esp_entity_t *entity = esp_entity_create(source_pad_cjson->valueint, sink_pad_cjson->valueint, video);
        if (!entity) {
            goto exit;
        }

        if (esp_entity_register_ops(entity, &entity_default_ops) != ESP_OK) {
            esp_entity_delete(entity);
            entity = NULL;
            goto exit;
        }
        cJSON *bridges = cJSON_GetObjectItem(entity_cjson, "bridges");
        if (!bridges) {
            esp_entity_delete(entity);
            entity = NULL;
            goto exit;
        }
        int bridges_num = cJSON_GetArraySize(bridges);
        for (int i = 0; i < bridges_num; i++) {
            cJSON *bridge = cJSON_GetArrayItem(bridges, i);
            cJSON *bridge_source_pad = cJSON_GetObjectItem(bridge, "source_pad");
            cJSON *bridge_sink_pad = cJSON_GetObjectItem(bridge, "sink_pad");
            if (esp_entity_pad_bridge(esp_entity_get_pad_by_index(entity, ESP_PAD_TYPE_SOURCE, bridge_source_pad->valueint),
                                      esp_entity_get_pad_by_index(entity, ESP_PAD_TYPE_SINK, bridge_sink_pad->valueint)) != ESP_OK) {
                esp_entity_delete(entity);
                entity = NULL;
                goto exit;
            }
        }
        esp_media_add_entity(media, entity);
    }

    // parse pipeline
    cJSON *pipelines = cJSON_GetObjectItem(media_cjson, "pipelines");
    if (!pipelines) {
        goto exit;
    }
    int pipelines_num = cJSON_GetArraySize(pipelines);

    for (int index = 0; index < pipelines_num; index++) {
        esp_pipeline_t *pipeline = esp_pipeline_create();
        if (!pipeline) {
            goto exit;
        }

        if (esp_media_add_pipeline(media, pipeline) != ESP_OK) {
            esp_pipeline_cleanup(pipeline);
            goto exit;
        }

        cJSON *pipeline_node = cJSON_GetArrayItem(pipelines, index);
        cJSON *user_nodes_cjson = cJSON_GetObjectItem(pipeline_node, "user_node");
        int user_node_num = cJSON_GetArraySize(user_nodes_cjson);
        for (int i = 0; i < user_node_num; i++) {
            cJSON *user_node_cjson = cJSON_GetArrayItem(user_nodes_cjson, i);
            cJSON *user_node_name_cjson = cJSON_GetObjectItem(user_node_cjson, "name");
            cJSON *entities = cJSON_GetObjectItem(user_node_cjson, "entities");
            int entities_num = cJSON_GetArraySize(entities);

            esp_pad_t *last_pad = NULL;
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
                    esp_pad_t *pre_pad = esp_pipeline_get_pad_by_entity(pipeline, pre_video->entity, ESP_PAD_TYPE_SOURCE);
                    if (esp_pads_link(pre_pad, pad) != ESP_OK) {
                        goto exit;
                    }
                    last_pad = pad->bridge_pad;
                } else {
                    if (!last_pad) {
                        last_pad = pad->bridge_pad;
                    }

                    esp_pad_t *entry_pad = esp_pipeline_get_entry_pad(pipeline);
                    if (entry_pad && (entry_pad != pad)) {
                        goto exit;
                    }

                    if (esp_pipeline_update_entry_pad(pipeline, pad) != ESP_OK) {
                        goto exit;
                    }
                }
            }
            pipeline->last_pad = last_pad;

            esp_user_node_device_t *user_node_device = esp_video_create(user_node_name_cjson->valuestring, NULL, &s_user_node_device_ops, last_pad);
            if (!user_node_device) {
                goto exit;
            }
            if (esp_vfs_dev_video_register(user_node_device->dev_name, user_node_device) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to register video VFS dev name=%s", user_node_device->dev_name);
                esp_video_destroy(user_node_device);
                goto exit;
            } else {
                last_pad->entity->user_node = user_node_device;
            }
        }
    }
    if (root) {
        cJSON_Delete(root);
        root = NULL;
    }
    err_ret = ESP_OK;

    return err_ret;
exit:
    if (root) {
        cJSON_Delete(root);
    }

    esp_media_cleanup(media);

    return err_ret;
}

esp_err_t esp_media_async_done_cb(struct esp_pad *pad, esp_media_event_cmd_t cmd, void *in, void *out)
{
    esp_media_event_t event;

    if (in != out) {
        esp_video_buffer_element_queue(in);
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
    return pad ? pad->bridge_pad : NULL;
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
            remote_list = remote_list->next;
        }
        remote_list->next = list;
        list->pre = remote_list;
        list->next = NULL;
    }
    pad->remote_pads_num++;

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

    if (source->bridge_pad) {
        source->bridge_pad->bridge_pad = NULL;
    }

    if (sink->bridge_pad) {
        sink->bridge_pad->bridge_pad = NULL;
    }

    source->bridge_pad = sink;
    sink->bridge_pad = source;

    return ESP_OK;
}

static esp_err_t esp_video_device_unbind_video_buffer(esp_pad_t *pad, void *param)
{
    struct esp_video *video = pad->entity->device;
    if (video && video->buffer) {
        video->buffer = NULL;
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

    if (!source->bridge_pad || !sink->bridge_pad) {
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
            esp_pads_unlink(source, sink);
            return ESP_FAIL;
        }
    }
    if (esp_pipeline_entities_iterate_walk_custom(sink, esp_pads_bind_pipeline_iterate,
            NULL, esp_pad_get_pipeline(source), false) != ESP_OK) {
        return ESP_FAIL;
    }
    sink_entity->is_initial_node = false;

    esp_pipeline_t *pipeline = esp_pad_get_pipeline(source);
    if (pipeline && pipeline->vb) {
        if (esp_pipeline_destory_video_buffer(pipeline) != ESP_OK) {
            return ESP_FAIL;
        }
    }

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
    default:
        return NULL;
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

    esp_pipeline_t *pipeline = esp_pad_get_pipeline(source);
    esp_list_t *list = source->remote_pads;

    while (list) {
        if (list->payload == sink) {
            if (pipeline && pipeline->vb) {
                esp_pipeline_destory_video_buffer(pipeline);
            }

            if (list->pre) {
                list->pre->next = list->next;
            } else {
                source->remote_pads = list->next;
            }

            if (list->next) {
                list->next->pre = list->pre;
            }
            free(list);
            break;
        }
        list = list->next;
    }

    list = sink->remote_pads;
    while (list) {
        if (list->payload == source) {
            if (list->pre) {
                list->pre->next = list->next;
            } else {
                sink->remote_pads = list->next;
            }

            if (list->next) {
                list->next->pre = list->pre;
            }
            free(list);
            break;
        }
        list = list->next;
    }

    if (!sink->remote_pads) {
        sink->entity->is_initial_node = true;
    }

    return ESP_OK;
}

esp_list_t *esp_pads_list_create(esp_entity_t *entity, uint8_t pad_num, esp_pad_type_t type)
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
    if (!pad) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_list_t *remote_pads_list = pad->remote_pads;
    while (remote_pads_list) {
        esp_list_t *next_list = remote_pads_list->next;
        free(remote_pads_list);
        remote_pads_list = next_list;
    }
    pad->remote_pads = NULL;
    free(pad);

    return ESP_OK;
}

esp_err_t esp_pad_purge(esp_pad_t *pad)
{
    if (!pad) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_list_t *remote_pads_list = pad->remote_pads;
    while (remote_pads_list) {
        esp_list_t *curr_list = remote_pads_list;
        remote_pads_list = remote_pads_list->next;
        esp_pads_unlink(pad, curr_list->payload);

    }
    pad->remote_pads = NULL;
    if (pad->bridge_pad) {
        pad->bridge_pad->bridge_pad = NULL;
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
        esp_list_t *pre_list = list;
        list = list->next;
        esp_pad_purge(pre_list->payload);
        free(pre_list);
    }

    return ESP_OK;
}

//////// entity
esp_entity_t *esp_entity_create(int source_num, int sink_num, struct esp_video *device)
{
    if (source_num <= 0 || sink_num <= 0) {
        return NULL;
    }

    esp_entity_t *entity = (esp_entity_t *)malloc(sizeof(esp_entity_t));

    if (entity) {
        memset(entity, 0x0, sizeof(esp_entity_t));
        entity->source_pads = esp_pads_list_create(entity, source_num, ESP_PAD_TYPE_SOURCE);
        entity->sink_pads = esp_pads_list_create(entity, sink_num, ESP_PAD_TYPE_SINK);
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
        entity->is_initial_node = true;
    }
    return entity;
}

esp_entity_t *esp_entity_create_with_media(esp_media_t *media, int source_num, int sink_num, struct esp_video *device)
{
    esp_entity_t *entity = esp_entity_create(source_num, sink_num, device);
    if (!entity) {
        return entity;
    }

    if (media) {
        if (esp_media_add_entity(media, entity) != ESP_OK) {
            esp_entity_delete(entity);
            entity = NULL;
            return entity;
        }
    }

    return entity;
}

esp_err_t esp_entity_delete(esp_entity_t *entity)
{
    if (!entity) {
        return ESP_FAIL;
    }

    esp_media_remove_entity(entity->media, entity);
    esp_pads_list_delete(entity->source_pads);
    esp_pads_list_delete(entity->sink_pads);

    if (entity->device) {
        entity->device->entity = NULL;
    }

    if (entity->user_node) {
        esp_vfs_dev_video_unregister(entity->user_node->dev_name);
        esp_video_destroy(entity->user_node);
    }
    free(entity);

    return ESP_OK;
}

esp_err_t esp_entity_register_ops(esp_entity_t *entity, esp_entity_ops_t *ops)
{
    entity->ops = ops;
    return ESP_OK;
}

bool esp_video_device_is_user_node(struct esp_video *video)
{
    if (video && !video->entity) {
        return true;
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

esp_pad_t *esp_pipeline_get_pad_by_entity(esp_pipeline_t *pipeline, esp_entity_t *entity, esp_pad_type_t type)
{
    if (!entity || !pipeline) {
        return NULL;
    }

    esp_pad_t *pad = pipeline->entry_pad;

    if (pad && pad->type == ESP_PAD_TYPE_SOURCE) {
        pad = pad->bridge_pad;
    }

    if (!pad) {
        return NULL;
    }

    esp_entity_t *entity_tmp = esp_pad_get_entity(pad);

    while (entity_tmp) {
        if (entity_tmp == entity) {
            if (type == ESP_PAD_TYPE_SOURCE) {
                pad = pad->bridge_pad;
            }
            return pad;
        }

        pad = pad->bridge_pad; //now pad is source type
        if (!pad || !pad->remote_pads) {
            return NULL;
        }
        pad = pad->remote_pads->payload;
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
            if (!vb_fork) {
                return ESP_FAIL;
            }
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
            esp_video_buffer_element_queue(vb);
            return ESP_FAIL;
        }

        if ((out != vb) && (vb != NULL)) {
            // free vb
            esp_video_buffer_element_queue(vb);
            vb = out;
        }
    }

    return pads_walk(pad->bridge_pad, cmd, vb);
}

static esp_err_t pads_iterate_walk_custom(esp_pad_t *pad,
        entities_iterate_walk_cb_t cb,
        entities_iterate_walk_cb_t completed_cb,
        void *param,
        bool reverse)
{
    if (!pad) {
        return ESP_OK;
    }
    esp_list_t *list = pad->remote_pads;

    while (list) {
        esp_pad_t *remote_pad = list->payload;
        esp_list_t *next_list = NULL;
        if (reverse) {
            next_list = list->pre;
        } else {
            next_list = list->next;
        }
        if (esp_pipeline_entities_iterate_walk_custom(remote_pad, cb, completed_cb, param, reverse) != ESP_OK) {
            return ESP_FAIL;
        }
        list = next_list;
    }

    return ESP_OK;
}

static esp_err_t esp_pipeline_entities_iterate_walk_custom(esp_pad_t *pad,
        entities_iterate_walk_cb_t cb,
        entities_iterate_walk_cb_t completed_cb,
        void *param,
        bool reverse)
{
    esp_err_t ret = ESP_FAIL;
    if (!pad) {
        return ESP_FAIL;
    }

    if (!reverse && pad->type == ESP_PAD_TYPE_SOURCE) {
        return pads_iterate_walk_custom(pad, cb, completed_cb, param, reverse);
    } else if (reverse && pad->type == ESP_PAD_TYPE_SINK) {
        return pads_iterate_walk_custom(pad, cb, completed_cb, param, reverse);
    }

    if (cb) {
        ret = cb(pad, param);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ret = pads_iterate_walk_custom(pad->bridge_pad, cb, completed_cb, param, reverse);
    if (ret != ESP_OK) {
        return ret;
    }

    if (completed_cb) {
        ret = completed_cb(pad, param);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

//////// pipeline
static void pipeline_walk (esp_pad_t *pad, esp_media_event_cmd_t cmd, struct esp_video_buffer_element *vb)
{
    entities_walk(pad, cmd, vb);
}

esp_pipeline_t *esp_pipeline_create(void)
{
    esp_pipeline_t *pipeline = (esp_pipeline_t *)malloc(sizeof(esp_pipeline_t));
    if (!pipeline) {
        return NULL;
    }
    memset(pipeline, 0x0, sizeof(esp_pipeline_t));

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
    esp_pad_t *bridge_pad = pad->bridge_pad;
    pad->pipeline = param;
    if (bridge_pad) {
        bridge_pad->pipeline = param;
    }

    return ESP_OK;
}

esp_err_t esp_pipeline_update_entry_pad(esp_pipeline_t *pipeline, esp_pad_t *pad)
{
    if (!pipeline) {
        return ESP_FAIL;
    }

    if ((pad->pipeline) && (pad->pipeline != pipeline)) {
        return ESP_FAIL;
    }

    esp_pipeline_entities_iterate_walk_custom(pad, esp_pads_bind_pipeline_iterate, NULL, pipeline, false);

    pipeline->entry_pad = pad;

    return ESP_OK;
}

esp_pad_t *esp_pipeline_get_entry_pad(esp_pipeline_t *pipeline)
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

    esp_pipeline_destory_video_buffer(pipeline);
    esp_media_remove_pipeline(pipeline->media, pipeline);
    free(pipeline);

    return ESP_OK;
}

static esp_err_t esp_pipeline_entities_cleanup(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = esp_pad_get_entity(pad);
    esp_list_t *pads_list = entity->source_pads;
    bool need_pop = true;

    while (pads_list) {
        esp_pad_t *temp_pad = pads_list->payload;
        if (temp_pad->pipeline && (temp_pad->pipeline != pad->pipeline)) {
            need_pop = false;
            break;
        }
        pads_list = pads_list->next;
    }

    if (need_pop) {
        pads_list = entity->sink_pads;
        while (pads_list) {
            esp_pad_t *temp_pad = pads_list->payload;
            if (temp_pad->pipeline && (temp_pad->pipeline != pad->pipeline)) {
                need_pop = false;
                break;
            }
            pads_list = pads_list->next;
        }
    }
    if (need_pop) {
        esp_entity_delete(pad->entity);
    }

    return ESP_OK;
}

esp_err_t esp_pipeline_cleanup(esp_pipeline_t *pipeline)
{
    if (!pipeline) {
        return ESP_FAIL;
    }
    esp_media_remove_pipeline(pipeline->media, pipeline);
    esp_pipeline_destory_video_buffer(pipeline);
    esp_pipeline_entities_iterate_walk_custom(pipeline->entry_pad, NULL, esp_pipeline_entities_cleanup, NULL, false);

    free(pipeline);

    return ESP_OK;
}

struct esp_video_buffer_element *esp_pipeline_alloc_video_buffer(esp_pipeline_t *pipeline)
{
    if (!pipeline) {
        return NULL;
    }

    return esp_video_buffer_get_queued_element(pipeline->vb);
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

esp_err_t esp_media_cleanup(esp_media_t *media)
{
    if (!media) {
        return ESP_FAIL;
    }

    esp_list_t *list = media->pipeline;
    while (list) {
        esp_list_t *temp = list->next;
        esp_pipeline_cleanup(list->payload);
        list = temp;
    }

    esp_media_cleanup_individual_entities(media);
    free(media);

    return ESP_OK;
}

esp_media_t *esp_media_create(void)
{
    esp_media_t *media = (esp_media_t *)malloc(sizeof(esp_media_t));
    if (!media) {
        return NULL;
    }
    memset(media, 0x0, sizeof(esp_media_t));

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
            return ESP_OK;
        }
        list = list->next;
    }

    if (!list) {
        esp_list_t *new_list = (esp_list_t *)malloc(sizeof(esp_list_t));

        if (!new_list) {
            return ESP_FAIL;
        }
        memset(new_list, 0x0, sizeof(esp_list_t));
        new_list->payload = new_pipeline;
        new_pipeline->media = media;

        if (!media->pipeline) {
            media->pipeline = new_list;
        } else {
            list = media->pipeline;
            while (list->next) {
                list = list->next;
            }
            list->next = new_list;
            new_list->pre = list;
        }
    }

    return ESP_OK;
}

esp_err_t esp_media_remove_pipeline(esp_media_t *media, esp_pipeline_t *pipeline)
{
    if (!media || !pipeline) {
        return ESP_FAIL;
    }

    esp_list_t *list = media->pipeline;

    while (list) {
        esp_pipeline_t *tmp_pipeline = list->payload;
        if (tmp_pipeline == pipeline) {
            pipeline->media = NULL;
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
        list = list->next;
    }

    return ESP_OK;
}

static uint32_t gcd(uint32_t a, uint32_t b)
{
    if (b == 0) {
        return a;
    } else {
        return gcd(b, a % b);
    }
}

static uint32_t lcm(uint32_t a, uint32_t b)
{
    return (a * b) / gcd(a, b);
}

static esp_err_t get_video_buffer_size_cb(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = pad->entity;
    struct esp_video *video = entity->device;

    struct esp_video_buffer_info *info = param;
    struct esp_video_buffer_info buffer_info;
    memset(&buffer_info, 0x0, sizeof(buffer_info));

    if (esp_video_get_buffer_info(video, &buffer_info) == ESP_OK) {
        if (info->count < buffer_info.count) {
            info->count = buffer_info.count;
        }

        if (info->size < buffer_info.size) {
            info->size = buffer_info.size;
        }

        info->align_size = lcm(info->align_size, buffer_info.align_size);
        info->caps |= buffer_info.caps;
    }

    return ESP_OK;
}

static esp_err_t pipeline_update_vb_cb(esp_pad_t *pad, void *param)
{
    esp_entity_t *entity = pad->entity;
    struct esp_video *video = entity->device;

    video->buffer = param;
    return ESP_OK;
}

esp_err_t esp_pipeline_create_video_buffer(esp_pipeline_t *pipeline)
{
    if (!pipeline) {
        return ESP_FAIL;
    }

    if (pipeline->vb) {
        esp_pipeline_destory_video_buffer(pipeline);
    }

    esp_user_node_device_t *device = pipeline->last_pad->entity->user_node;

    struct esp_video_buffer_info info;
    memcpy(&info, &device->buf_info, sizeof(info));

    esp_pipeline_entities_iterate_walk_custom(pipeline->entry_pad, get_video_buffer_size_cb, NULL, &info, false);
    pipeline->vb = esp_video_buffer_create(&info);
    esp_pipeline_entities_iterate_walk_custom(pipeline->entry_pad, pipeline_update_vb_cb, NULL,  pipeline->vb, false);

    return ESP_OK;
}

esp_err_t esp_pipeline_destory_video_buffer(esp_pipeline_t *pipeline)
{
    if (!pipeline) {
        return ESP_FAIL;
    }

    if (pipeline->vb) {
        esp_pipeline_entities_iterate_walk_custom(pipeline->entry_pad, esp_video_device_unbind_video_buffer, NULL, NULL, false);
        esp_video_buffer_destroy(pipeline->vb);
        pipeline->vb = NULL;
    }

    return ESP_OK;
}

esp_media_t *esp_pipeline_get_media(esp_pipeline_t *pipeline)
{
    if (!pipeline) {
        return NULL;
    }

    return pipeline->media;
}

esp_err_t esp_media_add_entity(esp_media_t *media, esp_entity_t *new_entity)
{
    if (!media || !new_entity) {
        return ESP_FAIL;
    }

    esp_list_t *list = media->entities;

    while (list) {
        esp_entity_t *entity = list->payload;
        if (entity == new_entity) {
            return ESP_OK;
        }
        list = list->next;
    }

    if (!list) {
        esp_list_t *new_list = (esp_list_t *)malloc(sizeof(esp_list_t));
        if (!new_list) {
            return ESP_FAIL;
        }
        memset(new_list, 0x0, sizeof(esp_list_t));
        new_list->payload = new_entity;
        new_entity->media = media;

        if (!media->entities) {
            media->entities = new_list;
        } else {
            list = media->entities;
            while (list->next) {
                list = list->next;
            }
            list->next = new_list;
            new_list->pre = list;
        }
    }

    return ESP_OK;
}

esp_err_t esp_media_remove_entity(esp_media_t *media, esp_entity_t *entity)
{
    if (!media || !entity) {
        return ESP_FAIL;
    }

    esp_list_t *list = media->entities;

    while (list) {
        esp_entity_t *tmp_entity = list->payload;
        if (tmp_entity == entity) {
            if (list->pre) {
                list->pre->next = list->next;
            } else {
                media->entities = list->next;
            }

            if (list->next) {
                list->next->pre = list->pre;
            }
            free(list);
            tmp_entity->media = NULL;
            break;
        }
        list = list->next;
    }

    return ESP_OK;
}

static esp_err_t esp_media_cleanup_individual_entities(esp_media_t *media)
{
    if (!media) {
        return ESP_FAIL;
    }

    esp_list_t *list = media->entities;
    media->entities = NULL;
    while (list) {
        esp_list_t *tmp = list->next;
        esp_entity_delete(list->payload);
        free(list);
        list = tmp;
    }
    return ESP_OK;
}

static void media_task(void *param)
{
    esp_media_event_t event;
    while (1) {
        if (xQueueReceive(media_queue, (void * )&event, (TickType_t)portMAX_DELAY)) {
            assert(event.pad && event.pad->pipeline);
            switch (event.cmd) {
            case ESP_MEIDA_EVENT_CMD_START:
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


esp_err_t esp_media_start(void)
{
    if (!media_queue) {
        media_queue = xQueueCreate(CONFIG_MEDIA_EVENT_NUM, sizeof(esp_media_event_t));
        if (!media_queue) {
            return ESP_FAIL;
        }
    }

#ifdef CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE_NAME
    esp_media_config_loader(media_config);
#endif
    if (xTaskCreate(media_task, "media_Task", CONFIG_MEDIA_TASK_STACK_SIZE, NULL, CONFIG_MEDIA_TASK_PRIORITY, NULL) == pdTRUE) {
        return ESP_OK;
    }

    return ESP_FAIL;
}
