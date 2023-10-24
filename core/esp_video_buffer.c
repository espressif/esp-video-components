/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_video_buffer.h"

#define ALLOC_RAM_ATTR                      (MALLOC_CAP_8BIT)

#define __ESP_VIDEO_BUFFER_ALIGN(s, a)      (((s) + ((a) - 1)) & (~((a) - 1)))
#define ESP_VIDEO_BUFFER_ALIGN_SIZE         4
#define ESP_VIDEO_BUFFER_ALIGN(s)           __ESP_VIDEO_BUFFER_ALIGN(s, ESP_VIDEO_BUFFER_ALIGN_SIZE)

struct esp_video_buffer *g_video_buffer;
static const char *TAG = "esp_video_buffer";

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
struct esp_video_buffer *esp_video_buffer_create(size_t count, size_t size)
{
    size_t mem_size;
    size_t element_size;
    struct esp_video_buffer *buffer;

    size  = ESP_VIDEO_BUFFER_ALIGN(size);

    element_size = size + sizeof(struct esp_video_buffer_element);
    mem_size = sizeof(struct esp_video_buffer) + element_size * count;

    buffer = heap_caps_malloc(mem_size, ALLOC_RAM_ATTR);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to malloc for video buffer");
        return NULL;
    }

    portMUX_INITIALIZE(&buffer->lock);
    SLIST_INIT(&buffer->free_list);
    buffer->element_count = count;
    buffer->element_size  = size;
    for (int i = 0; i < count; i++) {
        struct esp_video_buffer_element *element =
            (struct esp_video_buffer_element *)((char *)buffer->element + element_size * i);     

        SLIST_INSERT_HEAD(&buffer->free_list, element, node);
    }

    return buffer;
}

/**
 * @brief Clone a new video buffer
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer *esp_video_buffer_clone(const struct esp_video_buffer *buffer)
{
    size_t count = buffer->element_count;
    size_t size = buffer->element_size;

    return esp_video_buffer_create(count, size);
}

/**
 * @brief Allocate one buffer element, remove it from free list.
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_buffer_alloc(struct esp_video_buffer *buffer)
{
    struct esp_video_buffer_element *element = NULL;

    portENTER_CRITICAL(&buffer->lock);
    if (!SLIST_EMPTY(&buffer->free_list)) {
        element = SLIST_FIRST(&buffer->free_list);
        SLIST_REMOVE(&buffer->free_list, element, esp_video_buffer_element, node); 
    }
    portEXIT_CRITICAL(&buffer->lock);

    element->video_buffer = buffer;
    element->valid_size = 0;

    return element;
}

/**
 * @brief Free one buffer element, insert it to free list.
 *
 * @param buffer  Video buffer object
 * @param element Video buffer element object
 *
 * @return None
 */
void esp_video_buffer_free(struct esp_video_buffer *buffer, struct esp_video_buffer_element *element)
{
    portENTER_CRITICAL(&buffer->lock);
    SLIST_INSERT_HEAD(&buffer->free_list, element, node);
    portEXIT_CRITICAL(&buffer->lock);
}

/**
 * @brief Get current free element number.
 *
 * @param buffer  Video buffer object
 *
 * @return Free element number
 */
size_t esp_video_buffer_get_element_num(struct esp_video_buffer *buffer)
{
    size_t num = 0;
    struct esp_video_buffer_element *element;

    portENTER_CRITICAL(&buffer->lock);
    SLIST_FOREACH(element, &buffer->free_list, node) {
        num++;
    }
    portEXIT_CRITICAL(&buffer->lock);

    return num;
}

/**
 * @brief Destroy video buffer object.
 *
 * @param buffer Video buffer object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_buffer_destroy(struct esp_video_buffer *buffer)
{
    size_t n;
    
    n = esp_video_buffer_get_element_num(buffer);
    if (n != buffer->element_count) {
        return ESP_ERR_INVALID_STATE;
    }

    heap_caps_free(buffer);

    return ESP_OK;
}

/**
 * @brief Clone a new video buffer element
 *
 * @param element Video buffer element object
 *
 * @return None
 */
const struct esp_video_buffer_element *esp_video_buffer_element_clone(const struct esp_video_buffer_element *element)
{
    struct esp_video_buffer_element *new_element;

    new_element = esp_video_buffer_alloc(element->video_buffer);
    if (!new_element) {
        return NULL;
    }

    new_element->valid_size = element->valid_size;
    memcpy(new_element->buffer, element->buffer, element->valid_size);

    return new_element;
}
