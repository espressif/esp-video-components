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
#include "esp_video.h"

#define ALLOC_RAM_ATTR              (MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)
#define VIDEO_OPERATION_CHECK       1

static _lock_t s_video_lock;
static SLIST_HEAD(esp_video_list, esp_video) s_video_list = SLIST_HEAD_INITIALIZER(s_video_list);

static const char *TAG = "esp_video";

/**
 * @brief Initialize video system.
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_init(void)
{
    esp_err_t ret;

    ret = esp_video_bsp_init();
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Create video object.
 *
 * @param name         video device name
 * @param ops          video operations
 * @param priv         video private data
 * @param buffer_count video buffer count for lowlevel driver
 * @param buffer_size  video buffer size for lowlevel driver
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_create(const char *name, const struct esp_video_ops *ops, void *priv, size_t buffer_count, size_t buffer_size)
{
    bool found = false;
    struct esp_video *video;
    size_t size;

    if (!name || !ops || !buffer_count || !buffer_size) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return NULL;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(video, &s_video_list, node) {
        if (!strcmp(video->dev_name, name)) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (found) {
        ESP_LOGE(TAG, "video name=%s has been registered", name);
        goto errout_video_exits;
    }

    size = sizeof(struct esp_video) + strlen(name) + 1;
    video = heap_caps_malloc(size, ALLOC_RAM_ATTR);
    if (!video) {
        ESP_LOGE(TAG, "Failed to malloc for video");
        goto errout_video_exits;
    }

    video->buffer = esp_video_buffer_create(buffer_count, buffer_size);
    if (!video->buffer) {
        ESP_LOGE(TAG, "Failed to malloc for video");
        goto errout_no_buffer;
    }

    video->done_sem = xSemaphoreCreateCounting(buffer_count, 0);
    if (!video->done_sem) {
        heap_caps_free(video);
        ESP_LOGE(TAG, "Failed to create done_sem for video");
        goto errout_no_semaphore;
    }

    portMUX_INITIALIZE(&video->lock);
    SLIST_INIT(&video->done_list);
    video->dev_name = (char *)&video[1];
    strcpy(video->dev_name, name);
    video->ops      = ops;
    video->priv     = priv;

    _lock_acquire(&s_video_lock);
    SLIST_INSERT_HEAD(&s_video_list, video, node);
    _lock_release(&s_video_lock);

    return video;

errout_no_semaphore:
    esp_video_buffer_destroy(video->buffer);
errout_no_buffer:
    heap_caps_free(video);
errout_video_exits:
    return NULL;
}

/**
 * @brief Destroy video object.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy(struct esp_video *video)
{
#if VIDEO_OPERATION_CHECK
    bool found = false;
    struct esp_video *it, *tmp;
#endif

    if (!video) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

#if VIDEO_OPERATION_CHECK
    _lock_acquire(&s_video_lock);
    SLIST_FOREACH_SAFE(it, &s_video_list, node, tmp) {
        if (it == video) {
            SLIST_REMOVE(&s_video_list, video, esp_video, node);
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#else
    _lock_acquire(&s_video_lock);
    SLIST_REMOVE(&s_video_list, video, esp_video, node);
    _lock_release(&s_video_lock);
#endif

    vSemaphoreDelete(video->done_sem);
    esp_video_buffer_destroy(video->buffer);
    heap_caps_free(video);

    return ESP_OK;
}

/**
 * @brief Open a video device, this function will initializa hardware.
 *
 * @param name video device name
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_open(const char *name)
{
    esp_err_t ret;
    bool found = false;
    struct esp_video *video;

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(video, &s_video_list, node) {
        if (!strcmp(video->dev_name, name)) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return NULL;
    }

    if (video->ops->init) {
        ret = video->ops->init(video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize video lowlevel ret=%x", ret);
            return NULL;
        }
    } else {
        ESP_LOGI(TAG, "video->ops->init=NULL");
    }

    return video;
}

/**
 * @brief Close a video device, this function will de-initializa hardware.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_close(struct esp_video *video)
{
    esp_err_t ret;
    struct esp_video_buffer_element *element;
#if VIDEO_OPERATION_CHECK
    bool found = false;
    struct esp_video *it;
#endif

    if (!video) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

#if VIDEO_OPERATION_CHECK
    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->deinit) {
        ret = video->ops->deinit(video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to de-initialize video lowlevel ret=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "video->ops->deinit=NULL");
    }

    SLIST_FOREACH(element, &video->done_list, node) {
        esp_video_buffer_free(video->buffer, element);
    }

    return ESP_OK;
}

/**
 * @brief Start capturing video data stream.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_start_capture(struct esp_video *video)
{
    esp_err_t ret;
#if VIDEO_OPERATION_CHECK
    bool found = false;
    struct esp_video *it;
#endif

    if (!video) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

#if VIDEO_OPERATION_CHECK
    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->start_capture) {
        ret = video->ops->start_capture(video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start capture video lowlevel ret=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "video->ops->start_capture=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Stop capturing video data stream.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_stop_capture(struct esp_video *video)
{
    esp_err_t ret;
#if VIDEO_OPERATION_CHECK
    bool found = false;
    struct esp_video *it;
#endif

    if (!video) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

#if VIDEO_OPERATION_CHECK
    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->stop_capture) {
        ret = video->ops->stop_capture(video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop capture video lowlevel ret=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "video->ops->stop_capture=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Set video data stream attribution.
 *
 * @param video Video object
 * @param cmd   Video set command
 * @param arg   Video set parameters pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_attr(struct esp_video *video, int cmd, void *arg)
{
    esp_err_t ret;
#if VIDEO_OPERATION_CHECK
    bool found = false;
    struct esp_video *it;
#endif

    if (!video) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

#if VIDEO_OPERATION_CHECK
    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->set_attr) {
        ret = video->ops->set_attr(video, cmd, arg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set attribution video lowlevel ret=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "video->ops->set_attr=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Get video data stream attribution.
 *
 * @param video Video object
 * @param cmd   Video get command
 * @param arg   Video get parameters pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_attr(struct esp_video *video, int cmd, void *arg)
{
    esp_err_t ret;
#if VIDEO_OPERATION_CHECK
    bool found = false;
    struct esp_video *it;
#endif

    if (!video) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

#if VIDEO_OPERATION_CHECK
    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->get_attr) {
        ret = video->ops->get_attr(video, cmd, arg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get attribution video lowlevel ret=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "video->ops->get_attr=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Get video capability.
 *
 * @param video      Video object
 * @param capability Video capability object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    esp_err_t ret;
#if VIDEO_OPERATION_CHECK
    bool found = false;
    struct esp_video *it;
#endif

    if (!video) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

#if VIDEO_OPERATION_CHECK
    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->capability) {
        ret = video->ops->capability(video, capability);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get capability video lowlevel ret=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "video->ops->capability=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Get video description string.
 *
 * @param video  Video object
 * @param buffer String buffer
 * @param size   String buffer size
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_description(struct esp_video *video, char *buffer, uint16_t size)
{
    esp_err_t ret;
#if VIDEO_OPERATION_CHECK
    bool found = false;
    struct esp_video *it;
#endif

    if (!video) {
        ESP_LOGE(TAG, "Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

#if VIDEO_OPERATION_CHECK
    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_LOGE(TAG, "Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->description) {
        ret = video->ops->description(video, buffer, size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get description video lowlevel ret=%x", ret);
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "video->ops->description=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Allocate one video buffer.
 *
 * @param video Video object
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
uint8_t *esp_video_alloc_buffer(struct esp_video *video)
{
    struct esp_video_buffer_element *element;

    element = esp_video_buffer_alloc(video->buffer);
    if (!element) {
        return NULL;
    }

    return element->buffer;
}

/**
 * @brief Process a video buffer which receives data done. 
 *
 * @param video  Video object
 * @param buffer Video buffer allocated by "esp_video_alloc_buffer"
 * @param size   Actual received data size
 *
 * @return None
 */
void esp_video_recvdone_buffer(struct esp_video *video, uint8_t *buffer, size_t size)
{
    struct esp_video_buffer_element *element = 
        container_of(buffer, struct esp_video_buffer_element, buffer);

    /* Check if the buffer is overflow */
    if (size > esp_video_buffer_element_get_buffer_size(element)) {
        abort();
    }

    portENTER_CRITICAL(&video->lock);
    esp_video_buffer_element_set_valid_size(element, size);
    SLIST_INSERT_HEAD(&video->done_list, element, node);
    portEXIT_CRITICAL(&video->lock);

    xSemaphoreGive(video->done_sem);
}

/**
 * @brief Free one video buffer.
 *
 * @param video  Video object
 * @param buffer Video buffer allocated by "esp_video_alloc_buffer"
 *
 * @return None
 */
void esp_video_free_buffer(struct esp_video *video, uint8_t *buffer)
{
    struct esp_video_buffer_element *element = 
        container_of(buffer, struct esp_video_buffer_element, buffer);

    esp_video_buffer_free(video->buffer, element);
}

/**
 * @brief Receive buffer from video device. 
 *
 * @param video Video object
 * @param ticks Wait OS tick
 * @param size  Actual received data size
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
uint8_t *esp_video_recv_buffer(struct esp_video *video, size_t *recv_size, uint32_t ticks)
{
    BaseType_t ret;
    struct esp_video_buffer_element *element;

    ret = xSemaphoreTake(video->done_sem, (TickType_t)ticks);
    if (ret != pdTRUE) {
        return NULL;
    }

    portENTER_CRITICAL(&video->lock);
    element = SLIST_FIRST(&video->done_list);
    SLIST_REMOVE(&video->done_list, element, esp_video_buffer_element, node); 
    portEXIT_CRITICAL(&video->lock);

    *recv_size = esp_video_buffer_element_get_valid_size(element);

    return element->buffer;
}
