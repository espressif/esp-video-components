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

#define ALLOC_RAM_ATTR                      MALLOC_CAP_8BIT

#define ESP_VIDEO_BUFFER_ALIGN(s, a)      (((s) + ((a) - 1)) & (~((a) - 1)))

struct esp_video_buffer *g_video_buffer;
static const char *TAG = "esp_video_buffer";

/**
 * @brief Create video buffer object.
 *
 * @param count Buffer element count
 * @param size  Buffer element size
 * @param align_size Buffer aligned size, unit: byte, normally 4 bytes * n aligned
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer *esp_video_buffer_create(uint32_t count, uint32_t size, uint32_t align_size)
{
    uint32_t element_size;
    struct esp_video_buffer *buffer;
    uint32_t aligned = 0;

    // AEG-1117 struct esp_video_buffer_element is 8 bytes aligned now
#define VIDEO_BUFFER_ELEMENT_ALIGN_SIZE         8
    assert(align_size <= VIDEO_BUFFER_ELEMENT_ALIGN_SIZE);
    assert((align_size == 1) || (VIDEO_BUFFER_ELEMENT_ALIGN_SIZE % align_size == 0));
    aligned = VIDEO_BUFFER_ELEMENT_ALIGN_SIZE;
    element_size = ESP_VIDEO_BUFFER_ALIGN(sizeof(struct esp_video_buffer_element), aligned) + ESP_VIDEO_BUFFER_ALIGN(size, aligned);

    buffer = heap_caps_malloc(sizeof(struct esp_video_buffer), ALLOC_RAM_ATTR);
    if (!buffer) {
        ESP_VIDEO_LOGE("Failed to malloc for video buffer");
        return NULL;
    }

    buffer->element = heap_caps_aligned_alloc(aligned, element_size * count, ALLOC_RAM_ATTR);
    if (!buffer->element) {
        ESP_VIDEO_LOGE("Failed to malloc for video buffer element");
        heap_caps_free(buffer);
        return NULL;
    }
    portMUX_INITIALIZE(&buffer->lock);
    SLIST_INIT(&buffer->free_list);
    buffer->element_count = count;
    buffer->element_size = size;
    buffer->align_size = align_size;
    for (int i = 0; i < count; i++) {
        struct esp_video_buffer_element *element = (struct esp_video_buffer_element *)((uint8_t *)buffer->element + element_size * i);
        assert(((uint32_t)element->buffer % align_size) == 0);
        element->index = i;
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
    if (!buffer) {
        return NULL;
    }

    return esp_video_buffer_create(buffer->element_count, buffer->element_size, buffer->align_size);
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
        element->video_buffer = buffer;
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
    assert (n == buffer->element_count);

    heap_caps_free(buffer->element);
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
