/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_video_buffer.h"

typedef enum {
    ESP_PAD_TYPE_SOURCE,
    ESP_PAD_TYPE_SINK
} esp_pad_type_t;

typedef enum {
    ESP_MEIDA_EVENT_CMD_START,
    ESP_MEIDA_EVENT_CMD_DATA_RECV,
    ESP_MEIDA_EVENT_CMD_STOP,

    ESP_MEIDA_EVENT_CMD_IOCTL,
} esp_media_event_cmd_t;

struct esp_pad;
struct esp_media;
struct esp_entity;
struct esp_pipeline;

typedef struct esp_pad esp_pad_t;
typedef struct esp_media esp_media_t;
typedef struct esp_entity esp_entity_t;
typedef struct esp_pipeline esp_pipeline_t;

typedef esp_err_t (*esp_entity_event_cb_t)(struct esp_pad* pad, esp_media_event_cmd_t cmd, void* in, void** out);

typedef struct esp_entity_ops {
    esp_entity_event_cb_t event_cb;
    // void (*ioctl)(cmd, param);
} esp_entity_ops_t;

typedef union {
    esp_pipeline_t* pipeline;
} esp_media_event_param_t;

typedef struct esp_media_event {
    esp_pad_t* pad;
    esp_media_event_cmd_t cmd;
    // esp_pipeline_t* pipeline;
    void* param;
} esp_media_event_t;

/**
 * @brief Create a entity.
 *
 * @param source_num the number of source pads in entity
 * @param sink_num the number of sink pads in entity
 * @param obj the object pointer this entity  
 * 
 * @return
 *      - Entity object pointer if successful
 *      - NULL if failed
 */
esp_entity_t* esp_entity_create(int source_num, int sink_num, void* obj);

/**
 * @brief link the source pad and sink pad in different entiries
 *
 * @param source the source pad
 * @param sink the sink pad
 * 
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_pads_link(esp_pad_t* source, esp_pad_t* sink);

/**
 * @brief link the source pad and sink pad in different entiries by the indexes.
 *
 * @param src_entity the entity of source pad
 * @param src_pad_index the index of source pads
 * @param sink_entity the entity of sink pad
 * @param sink_pad_index the index of sink pads
 * 
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_pads_link_by_index(esp_entity_t* src_entity, int src_pad_index, esp_entity_t* sink_entity, int sink_pad_index);

/**
 * @brief get entity by pad.
 *
 * @param pad the pad of the entity
 * 
 * @return
 *      - entity object pointer if successful
 *      - NULL if not found
 */
esp_entity_t* esp_pad_get_entity(esp_pad_t* pad);

/**
 * @brief get bridge pad by pad.
 *
 * @param pad the pad object pointer 
 * 
 * @return
 *      - bridge pad object pointer if successful
 *      - NULL if not found
 */
esp_pad_t* esp_pad_get_bridge_pad(esp_pad_t* pad);

/**
 * @brief get pad by index.
 *
 * @param entity which entity we want to find in
 * @param type the type of the pad
 * @param index the index to find
 * 
 * @return
 *      - pad object pointer if successful
 *      - NULL if not found
 */
esp_pad_t* esp_entity_get_pad_by_index(esp_entity_t* entity, esp_pad_type_t type, int index);

/**
 * @brief link the source pad and sink pad in different entiries
 *
 * @param source the source pad
 * @param sink the sink pad
 * 
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_entity_pad_bridge(esp_pad_t* source, esp_pad_t* sink);

/**
 * @brief get pipeline form pad
 *
 * @param pad the pad which to be found
 * 
 * @return
 *      - Pipeline object pointer if successful
 *      - NULL if failed
 */
esp_pipeline_t* esp_pad_get_pipeline(esp_pad_t* pad);

/**
 * @brief Create a pipeline.
 *
 * @param vb_num the number of video buffer
 * @param timeout the size of video buffer
 * 
 * @return
 *      - Pipeline object pointer if successful
 *      - NULL if failed
 */
esp_pipeline_t* esp_pipeline_create(int vb_num, int vb_size);

/**
 * @brief Add a pipeline into media.
 *
 * @param media the number of video buffer
 * @param new_pipeline pipeline which will be added into media
 * 
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_media_add_pipeline(esp_media_t* media, esp_pipeline_t* new_pipeline);

/**
 * @brief Create a media.
 * 
 * @return
 *      - Media object pointer if successful
 *      - NULL if failed
 */
esp_media_t* esp_media_create(void);

/**
 * @brief regist APIs for an entity.
 * 
 * @param entity the entity which need be registed APIs
 * @param ops an APIs set
 * 
 * @return
 *      - Media object pointer if successful
 *      - NULL if failed
 */
esp_err_t esp_entity_regist_ops(esp_entity_t* entity, esp_entity_ops_t* ops);

/**
 * @brief update the entry pad of pipeline.
 * 
 * @param pipeline which pipeline we want to update entry pad
 * @param pad which pad we want to use, if pad is ESP_PAD_TYPE_SOURCE type, the entity of pad will do
 *            nothing, if pad is ESP_PAD_TYPE_SINK type, the entity of pad will do callback
 * 
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_pipeline_update_entry_entity(esp_pipeline_t* pipeline,esp_pad_t* pad);

/**
 * @brief get the entry pad of pipeline.
 * 
 * @param pipeline which pipeline to get entry pad
 * 
 * @return
 *      - Pad object pointer if successful
 *      - NULL if failed
 */
esp_pad_t* esp_pipeline_get_entry_entity(esp_pipeline_t* pipeline);

/**
 * @brief alloc a buffer from a pipeline.
 * 
 * @param pipeline which pipeline we want to alloc video buffer from
 * 
 * @return
 *      - Video buffer object pointer if successful
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_buffer_alloc_from_pipeline(esp_pipeline_t *pipeline);

/**
 * @brief Post an event to media.
 *
 * @param event event will be posted
 * @param timeout timeout
 * 
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_media_event_post(esp_media_event_t* event, TickType_t timeout);

/**
 * @brief start media task.
 * 
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t media_start(void);