/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "esp_heap_caps.h"
#include "esp_video_log.h"
#include "esp_video_buffer.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#define portMUX_INITIALIZE(mux)             spinlock_initialize(mux)
#endif

#define ESP_VIDEO_BUFFER_ALIGN(s, a)      (((s) + ((a) - 1)) & (~((a) - 1)))

static const char *TAG = "esp_video_buffer";

/**
 * @brief Create video buffer object.
 *
 * @param info Buffer information pointer.
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer *esp_video_buffer_create(const struct esp_video_buffer_info *info)
{
    uint32_t size;
    struct esp_video_buffer *buffer;

    size = sizeof(struct esp_video_buffer) + sizeof(struct esp_video_buffer_element) * info->count;
    buffer = heap_caps_calloc(1, size, info->caps);
    if (!buffer) {
        ESP_VIDEO_LOGE("Failed to malloc for video buffer");
        return NULL;
    }

    SLIST_INIT(&buffer->free_list);
    for (int i = 0; i < info->count; i++) {
        struct esp_video_buffer_element *element = &buffer->element[i];

        element->buffer = heap_caps_aligned_alloc(info->align_size, info->size, info->caps);
        if (element->buffer) {
            element->index = i;
            element->video_buffer = buffer;
            SLIST_INSERT_HEAD(&buffer->free_list, element, node);
        } else {
            goto errout_alloc_buffer;
        }
    }

    portMUX_INITIALIZE(&buffer->lock);
    buffer->info.count = info->count;
    buffer->info.size = info->size;
    buffer->info.align_size = info->align_size;
    buffer->info.caps = info->caps;

    return buffer;

errout_alloc_buffer:
    for (int i = 0; i < info->count; i++) {
        struct esp_video_buffer_element *element = &buffer->element[i];

        if (element->buffer) {
            heap_caps_free(element->buffer);
        }
    }

    heap_caps_free(buffer);
    return NULL;
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
    if (!buffer) {
        return NULL;
    }

    return esp_video_buffer_create(&buffer->info);
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
struct esp_video_buffer_element *IRAM_ATTR esp_video_buffer_alloc(struct esp_video_buffer *buffer)
{
    struct esp_video_buffer_element *element = NULL;

    portENTER_CRITICAL_SAFE(&buffer->lock);
    if (!SLIST_EMPTY(&buffer->free_list)) {
        element = SLIST_FIRST(&buffer->free_list);
        SLIST_REMOVE(&buffer->free_list, element, esp_video_buffer_element, node);
    }
    portEXIT_CRITICAL_SAFE(&buffer->lock);

    if (element) {
        element->valid_offset = 0;
        element->valid_size = 0;
    }

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
    portENTER_CRITICAL_SAFE(&buffer->lock);
    SLIST_INSERT_HEAD(&buffer->free_list, element, node);
    portEXIT_CRITICAL_SAFE(&buffer->lock);
}

/**
 * @brief Get current free element number.
 *
 * @param buffer  Video buffer object
 *
 * @return Free element number
 */
uint32_t esp_video_buffer_get_element_num(struct esp_video_buffer *buffer)
{
    uint32_t num = 0;
    struct esp_video_buffer_element *element;

    portENTER_CRITICAL_SAFE(&buffer->lock);
    SLIST_FOREACH(element, &buffer->free_list, node) {
        num++;
    }
    portEXIT_CRITICAL_SAFE(&buffer->lock);

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
    uint32_t n;

    n = esp_video_buffer_get_element_num(buffer);
    assert (n == buffer->info.count);

    for (int i = 0; i < buffer->info.count; i++) {
        heap_caps_free(buffer->element[i].buffer);
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
struct esp_video_buffer_element *esp_video_buffer_element_clone(const struct esp_video_buffer_element *element)
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
struct esp_video_buffer_element *IRAM_ATTR esp_video_buffer_get_element_by_buffer(struct esp_video_buffer *buffer, uint8_t *ptr)
{
    for (int i = 0; i < buffer->info.count; i++) {
        if (buffer->element[i].buffer == ptr) {
            return &buffer->element[i];
        }
    }

    return NULL;
}
