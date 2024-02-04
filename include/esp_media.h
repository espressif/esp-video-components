/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_video_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

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

struct esp_video;

typedef struct esp_pad esp_pad_t;
typedef struct esp_media esp_media_t;
typedef struct esp_entity esp_entity_t;
typedef struct esp_pipeline esp_pipeline_t;

typedef esp_err_t (*esp_entity_event_cb_t)(struct esp_pad *pad, esp_media_event_cmd_t cmd, void *in, void **out);

typedef struct esp_entity_ops {
    esp_entity_event_cb_t event_cb;
} esp_entity_ops_t;

typedef union {
    esp_pipeline_t *pipeline;
} esp_media_event_param_t;

typedef struct esp_media_event {
    esp_pad_t *pad;
    esp_media_event_cmd_t cmd;
    void *param;
} esp_media_event_t;

/**
 * @brief link the source pad and sink pad between the two entities
 *
 * @param source the source pad
 * @param sink the sink pad
 * @note  this will trigger the pipeline of the pad changes: The latter pad follows the former one.
 *
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_pads_link(esp_pad_t *source, esp_pad_t *sink);

/**
 * @brief unlink the source pad and sink pad between the two entities
 *
 * @param source the source pad
 * @param sink the sink pad
 *
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_pads_unlink(esp_pad_t *source, esp_pad_t *sink);

/**
 * @brief link the source pad and sink pad between the two entities by the indexes.
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
esp_err_t esp_pads_link_by_index(esp_entity_t *src_entity, int src_pad_index, esp_entity_t *sink_entity, int sink_pad_index);

/**
 * @brief get the entity by the pad.
 *
 * @param pad the pad of the entity
 *
 * @return
 *      - entity object pointer if successful
 *      - NULL if not found
 */
esp_entity_t *esp_pad_get_entity(esp_pad_t *pad);

/**
 * @brief get the pad's bridge pad.
 *
 * @param pad the pad object pointer
 *
 * @return
 *      - bridge pad object pointer if successful
 *      - NULL if not found
 */
esp_pad_t *esp_pad_get_bridge_pad(esp_pad_t *pad);

/**
 * @brief get the pipeline which the pad is in
 *
 * @param pad the pad which to be found
 *
 * @return
 *      - Pipeline object pointer if successful
 *      - NULL if failed
 */
esp_pipeline_t *esp_pad_get_pipeline(esp_pad_t *pad);

/**
 * @brief Create an entity.
 *
 * @param source_num the number of source pads in entity
 * @param sink_num the number of sink pads in entity
 * @param device the video device pointer
 * @note  one pad only can belong to one pipeline, if you want to create one entity belong to multiple pipelines,
 *        please create multiple pads, and make sure one pad only in one pipeline
 *
 * @return
 *      - Entity object pointer if successful
 *      - NULL if failed
 */
esp_entity_t *esp_entity_create(int source_num, int sink_num, struct esp_video *device);

/**
 * @brief Create an entity and add the new entity into a media.
 *
 * @param media the media object pointer
 * @param source_num the number of source pads in entity
 * @param sink_num the number of sink pads in entity
 * @param device the video device pointer
 * @note  one pad only can belong to one pipeline, if you want to create one entity belong to multiple pipelines,
 *        please create multiple pads, and make sure one pad only in one pipeline
 *
 * @return
 *      - Entity object pointer if successful
 *      - NULL if failed
 */
esp_entity_t *esp_entity_create_with_media(esp_media_t *media, int source_num, int sink_num, struct esp_video *device);

/**
 * @brief Delete an entity. This API will also delete the pads of the entity
 *
 * @param entity the entity to be deleted
 *
 * @return
 *      - ESP_OK if successful
 *      - Others if failed
 */
esp_err_t esp_entity_delete(esp_entity_t *entity);

/**
 * @brief get the pad of the entity by the index.
 *
 * @param entity which entity we want to find in
 * @param type the type of the pad
 * @param index the index to find
 *
 * @return
 *      - pad object pointer if successful
 *      - NULL if not found
 */
esp_pad_t *esp_entity_get_pad_by_index(esp_entity_t *entity, esp_pad_type_t type, int index);

/**
 * @brief bridge the source and sink pad in one entity
 *
 * @param source the source pad
 * @param sink the sink pad
 * @note one source pad only can bridge one sink, and vice versa.
 *       If the source or sink pad had bridged the other pad, it will firstly disconnect the old one,
 *       and then bridge the new one.
 *
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_entity_pad_bridge(esp_pad_t *source, esp_pad_t *sink);

/**
 * @brief register APIs for an entity.
 *
 * @param entity the entity which need be registered APIs
 * @param ops an APIs set
 *
 * @return
 *      - Media object pointer if successful
 *      - NULL if failed
 */
esp_err_t esp_entity_register_ops(esp_entity_t *entity, esp_entity_ops_t *ops);

/**
 * @brief Get the video device pointer of the entity.
 *
 * @param entity the entity to get
 *
 * @return
 *      - Video device pointer if successfully
 *      - NULL if failed
 */
struct esp_video *esp_entity_get_device(esp_entity_t *entity);

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
esp_err_t esp_pipeline_update_entry_pad(esp_pipeline_t *pipeline, esp_pad_t *pad);

/**
 * @brief get the entry pad of the pipeline.
 *
 * @param pipeline which pipeline to get entry pad
 *
 * @return
 *      - Pad object pointer if successful
 *      - NULL if failed
 */
esp_pad_t *esp_pipeline_get_entry_pad(esp_pipeline_t *pipeline);

/**
 * @brief Create a video buffer for the pipeline
 *
 * @param pipeline Create the video buffer for this pipeline
 *
 * @return
 *      - ESP_OK if successful
 *      - Others if failed
 */
esp_err_t esp_pipeline_create_video_buffer(esp_pipeline_t *pipeline);

/**
 * @brief Destory the pipeline video buffer
 *
 * @param pipeline the pipeline to be destoryed video buffer
 *
 * @return
 *      - ESP_OK if successful
 *      - Others if failed
 */
esp_err_t esp_pipeline_destory_video_buffer(esp_pipeline_t *pipeline);

/**
 * @brief alloc a video buffer element from the video buffer pool of a pipeline.
 *
 * @param pipeline which pipeline we want to alloc video buffer from
 *
 * @return
 *      - Video buffer object pointer if successful
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_pipeline_alloc_video_buffer(esp_pipeline_t *pipeline);

/**
 * @brief Get the video buffer pool of a pipeline.
 *
 * @param pipeline the pipeline which we want to get video buffer from
 *
 * @return
 *      - Video buffer object pointer if successful
 *      - NULL if failed
 */
struct esp_video_buffer *esp_pipeline_get_video_buffer(esp_pipeline_t *pipeline);

/**
 * @brief Get the pad of the entity in a pipeline.
 *
 * @param pipeline the pipeline which the pad is located in
 * @param entity   the entity which the pad is located in
 * @param type     the pad type
 *
 * @return
 *      - the pad pointer if successful
 *      - NULL if failed
 */
esp_pad_t *esp_pipeline_get_pad_by_entity(esp_pipeline_t *pipeline, esp_entity_t *entity, esp_pad_type_t type);

/**
 * @brief Create a pipeline.
 *
 * @return
 *      - Pipeline object pointer if successful
 *      - NULL if failed
 */
esp_pipeline_t *esp_pipeline_create(void);

/**
 * @brief Remove the pipeline from it's media and delete it.
 *        This will free video buffer of the pipeline
 *
 * @param pipeline pipeline to be deleted
 *
 * @return
 *      - ESP_OK if successful
 *      - Others if failed
 */
esp_err_t esp_pipeline_delete(esp_pipeline_t *pipeline);

/**
 * @brief Remove the pipeline from it's media and delete it.
 *        This will free video buffer of the pipeline and delete the entities which are only belonged to this pipeline
 *
 * @param pipeline pipeline to be cleaned up
 *
 * @return
 *      - ESP_OK if successful
 *      - Others if failed
 */
esp_err_t esp_pipeline_cleanup(esp_pipeline_t *pipeline);

/**
 * @brief Get media object by pipeline
 *
 * @param pipeline the pipeline pointer
 *
 * @return
 *      - NULL if failed
 *      - media object pointer if successful
 */
esp_media_t *esp_pipeline_get_media(esp_pipeline_t *pipeline);

/**
 * @brief Add an entity into a media.
 *
 * @param media  media object
 * @param new_entity the entity to be added
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_media_add_entity(esp_media_t *media, esp_entity_t *new_entity);

/**
 * @brief Remove an entity from a media.
 *
 * @param media  media object
 * @param entity the entity to be removed
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_media_remove_entity(esp_media_t *media, esp_entity_t *entity);

/**
 * @brief Add a pipeline into the media.
 *
 * @param media the media which add pipeline into
 * @param new_pipeline the pipeline to be added
 *
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_media_add_pipeline(esp_media_t *media, esp_pipeline_t *new_pipeline);

/**
 * @brief Remove a pipeline from the media.
 *
 * @param media the media which remove pipeline from
 * @param new_pipeline the pipeline to be removed
 * @note  this app only remove pipeline from the media, it will not free the pipeline,
 *        if you want to free the pipeline, please call esp_pipeline_cleanup after this api
 *
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_media_remove_pipeline(esp_media_t *media, esp_pipeline_t *pipeline);

/**
 * @brief Create a media.
 *
 * @return
 *      - Media object pointer if successful
 *      - NULL if failed
 */
esp_media_t *esp_media_create(void);

/**
 * @brief Cleapup media, this will clean up the pipelines, entities, pads in this media.
 *
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_media_cleanup(esp_media_t *media);

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
esp_err_t esp_media_event_post(esp_media_event_t *event, TickType_t timeout);

/**
 * @brief start media task.
 *
 * @return
 *      - ESP_OK if successful
 *      - others if failed
 */
esp_err_t esp_media_start(void);

/**
 * @brief Load and parse the pipeline config which is the JSON string.
 *
 * @param config_string JSON string
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_media_config_loader(const char *config_string);

/**
 * @brief media device ioctl.
 *
 * @param video video object
 * @param cmd ioctl cmd which is defined in include/linux/videodev2.h
 * @param args the args list of the ioctl cmd
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_media_ioctl(struct esp_video *video, int cmd, va_list args);

/**
 * @brief Check the video device whether it is an end node, which means if it is an user node.
 *
 * @param video the video device
 *
 * @return
 *      - True if the entity is an user node
 *      - False if the entity is not an user node
 */
bool esp_video_device_is_user_node(struct esp_video *video);

/**
 * @brief Process a video buffer which receives data done.
 *
 * @param video  Video object
 * @param buffer Video buffer allocated by "esp_video_alloc_buffer"
 * @param size   Actual received data size
 *
 * @return None
 */
void esp_video_media_recvdone_buffer(struct esp_video *video, void *buffer, size_t size, uint32_t  offset);
#ifdef __cplusplus
}
#endif
