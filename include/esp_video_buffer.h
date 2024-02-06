/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include <stdint.h>
#include <stddef.h>
#include <sys/queue.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_VIDEO_BUFFER_ELEMENT(vb, i)     (&(vb)->element[i])
#define ELEMENT_SIZE(e)                     ((e)->video_buffer->info.size)
#define ELEMENT_BUFFER(e)                   ((e)->buffer)

struct esp_video_buffer_element;

/**
 * @brief Video buffer element.
 */
typedef SLIST_ENTRY(esp_video_buffer_element) esp_video_buffer_node_t;

/**
 * @brief Video buffer list.
 */
typedef SLIST_HEAD(esp_video_buffer_list, esp_video_buffer_element) esp_video_buffer_list_t;


struct esp_video_buffer;

/**
 * @brief Video buffer information object.
 */
struct esp_video_buffer_info {
    uint32_t count;                                   /*!< Buffer count */
    uint32_t size;                                    /*!< Buffer maximum size */
    uint32_t align_size;                              /*!< Buffer align size in byte */
    uint32_t caps;                                    /*!< Buffer capability: refer to esp_heap_caps.h MALLOC_CAP_XXX */
};

/**
 * @brief Video buffer element object.
 */
struct esp_video_buffer_element {
    esp_video_buffer_node_t node;                     /*!< List node */
    struct esp_video_buffer *video_buffer;            /*!< Source buffer object */
    uint32_t index;                                   /*!< List node index */
    uint8_t *buffer;                                  /*!< Buffer space to fill data */

    uint32_t valid_size;                              /*!< Valid data size */
};

/**
 * @brief Video buffer object.
 */
struct esp_video_buffer {
    portMUX_TYPE lock;                              /*!< List lock */
    esp_video_buffer_list_t queued_list;            /*!< Free buffer elements list */
    esp_video_buffer_list_t done_list;              /*!< Free buffer elements list */

    struct esp_video_buffer_info info;              /*!< Buffer information */

    struct esp_video_buffer_element element[0];     /*!< Element buffer */
};

/**
 * @brief Create video buffer object.
 *
 * @param info Buffer information pointer.
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer *esp_video_buffer_create(const struct esp_video_buffer_info *info);

/**
 * @brief Clone a new video buffer
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer *esp_video_buffer_clone(const struct esp_video_buffer *buffer);

/**
 * @brief Destroy video buffer object.
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_buffer_destroy(struct esp_video_buffer *buffer);

/**
 * @brief Clone a new video buffer
 *
 * @param element Video buffer element object
 *
 * @return None
 */
struct esp_video_buffer_element *esp_video_buffer_element_clone(const struct esp_video_buffer_element *element);

/**
 * @brief Get element object pointer by buffer
 *
 * @param buffer Video buffer object
 * @param ptr    Element buffer pointer
 *
 * @return
 *      - Element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_buffer_get_element_by_buffer(struct esp_video_buffer *buffer, uint8_t *ptr);

/**
 * @brief Get one buffer element from queued list and remove it from queued list.
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_buffer_get_queued_element(struct esp_video_buffer *buffer);

/**
 * @brief Get one buffer element from done list and remove it from done list.
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_buffer_get_done_element(struct esp_video_buffer *buffer);

/**
 * @brief Put buffer element into queued list.
 *
 * @param buffer  Video buffer object
 * @param element Video buffer element object
 *
 * @return None
 */
void esp_video_buffer_queue(struct esp_video_buffer *buffer, struct esp_video_buffer_element *element);

/**
 * @brief Put buffer element into done list.
 *
 * @param buffer  Video buffer object
 * @param element Video buffer element object
 *
 * @return None
 */
void esp_video_buffer_done(struct esp_video_buffer *buffer, struct esp_video_buffer_element *element);

/**
 * @brief Put buffer element into queued list.
 *
 * @param element Video buffer element object
 *
 * @return None
 */
static inline void esp_video_buffer_element_queue(struct esp_video_buffer_element *element)
{
    esp_video_buffer_queue(element->video_buffer, element);
}

/**
 * @brief Get one element buffer total size
 *
 * @param element Video buffer element object
 *
 * @return Buffer total size
 */
static inline uint32_t esp_video_buffer_element_get_buffer_size(struct esp_video_buffer_element *element)
{
    return element->video_buffer->info.size;
}

/**
 * @brief Get one element buffer valid data size
 *
 * @param element Video buffer element object
 *
 * @return Buffer valid data size
 */
static inline uint32_t esp_video_buffer_element_get_valid_size(struct esp_video_buffer_element *element)
{
    return element->valid_size;
}

/**
 * @brief Set one element buffer valid data size
 *
 * @param element    Video buffer element object
 * @param valid_size Valid data size
 *
 * @return None
 */
static inline void esp_video_buffer_element_set_valid_size(struct esp_video_buffer_element *element, uint32_t valid_size)
{
    element->valid_size = valid_size;
}

/**
 * @brief Get element buffer pointer
 *
 * @param element Video buffer element object
 *
 * @return Element buffer pointer
 */
static inline uint8_t *esp_video_buffer_element_get_buffer(struct esp_video_buffer_element *element)
{
    return element->buffer;
}

/**
 * @brief Get element index
 *
 * @param element Video buffer element object
 *
 * @return Element index
 */
static inline uint32_t esp_video_buffer_element_get_index(struct esp_video_buffer_element *element)
{
    return element->index;
}

/**
 * @brief Get element offset(index)
 *
 * @param buffer Video buffer object
 * @param element Video buffer element object
 *
 * @return Element offset
 */
static inline uint32_t esp_video_buffer_get_element_offset(struct esp_video_buffer *buffer, struct esp_video_buffer_element *element)
{
    return element->index;
}

/**
 * @brief Get element by offset(index)
 *
 * @param buffer Video buffer object
 * @param offset Element offset(index)
 *
 * @return Element object pointer
 */
static inline struct esp_video_buffer_element *esp_video_buffer_get_element_by_offset(struct esp_video_buffer *buffer, uint32_t offset)
{
    return &buffer->element[offset];
}

#ifdef __cplusplus
}
#endif
