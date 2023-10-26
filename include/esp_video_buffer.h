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

#ifndef container_of
#define container_of(ptr, type, member) \
  ((type *)((uintptr_t)(ptr) - offsetof(type, member)))
#endif

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
 * @brief Video buffer element object.
 */
struct esp_video_buffer_element {
    esp_video_buffer_node_t node;                   /*!< List node */
    struct esp_video_buffer *video_buffer;          /*!< Source buffer object */
    size_t valid_size;                              /*!< Valid data size */
    uint8_t buffer[0];                              /*!< Buffer space to fill data */ 
};

/**
 * @brief Video buffer object.
 */
struct esp_video_buffer {
    esp_video_buffer_list_t free_list;              /*!< Free buffer elements list  */ 
    portMUX_TYPE lock;                              /*!< Buffer lock */

    size_t element_count;                           /*!< Element count */ 
    size_t element_size;                            /*!< Element buffer size without other member */ 

    struct esp_video_buffer_element element[0];     /*!< Element buffer */
};

/**
 * @brief Create video buffer object.
 *
 * @param count Buffer element count
 * @param size  Buffer element size
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer *esp_video_buffer_create(size_t count, size_t size);

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
 * @brief Allocate one buffer element, remove it from free list.
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_buffer_alloc(struct esp_video_buffer *buffer);

/**
 * @brief Free one buffer element, insert it to free list.
 *
 * @param buffer  Video buffer object
 * @param element Video buffer element object
 *
 * @return None
 */
void esp_video_buffer_free(struct esp_video_buffer *buffer, struct esp_video_buffer_element *element);

/**
 * @brief Get current free element number.
 *
 * @param buffer  Video buffer object
 *
 * @return Free element number
 */
size_t esp_video_buffer_get_element_num(struct esp_video_buffer *buffer);

/**
 * @brief Free one element, insert it to source free list.
 *
 * @param element Video buffer element object
 *
 * @return None
 */
static inline void esp_video_buffer_element_free(struct esp_video_buffer_element *element)
{
    esp_video_buffer_free(element->video_buffer, element);
}

/**
 * @brief Clone a new video buffer
 *
 * @param element Video buffer element object
 *
 * @return None
 */
struct esp_video_buffer_element *esp_video_buffer_element_clone(const struct esp_video_buffer_element *element);


/**
 * @brief Get one element buffer total size
 *
 * @param element Video buffer element object
 *
 * @return Buffer total size
 */
static inline size_t esp_video_buffer_element_get_buffer_size(struct esp_video_buffer_element *element)
{
    return element->video_buffer->element_size;
}

/**
 * @brief Get one element buffer valid data size
 *
 * @param element Video buffer element object
 *
 * @return Buffer valid data size
 */
static inline size_t esp_video_buffer_element_get_valid_size(struct esp_video_buffer_element *element)
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
static inline void esp_video_buffer_element_set_valid_size(struct esp_video_buffer_element *element, size_t valid_size)
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

#ifdef __cplusplus
}
#endif
