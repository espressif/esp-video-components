/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "esp_heap_caps.h"
#include "esp_video.h"
#include "esp_video_log.h"
#include "esp_video_bsp.h"
#include "esp_video_vfs.h"

#include "freertos/portmacro.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#define portMUX_INITIALIZE(mux)             spinlock_initialize(mux)
#endif

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
        ESP_VIDEO_LOGE("Failed to initialize video BSP");
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
struct esp_video *esp_video_create(const char *name, const struct esp_video_ops *ops, void *priv, uint32_t buffer_count, uint32_t buffer_size)
{
    esp_err_t ret;
    bool found = false;
    struct esp_video *video;
    uint32_t size;
    int id = -1;

#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    if (!name || !ops || !buffer_count || !buffer_size) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return NULL;
    }

    _lock_acquire(&s_video_lock);

    SLIST_FOREACH(video, &s_video_list, node) {
        if (!strcmp(video->dev_name, name)) {
            found = true;
            break;
        }
    }

    if (found) {
        ESP_VIDEO_LOGE("video name=%s has been registered", name);
        goto errout_video_exits;
    }
#else
    _lock_acquire(&s_video_lock);
#endif

    /* Search valid ID */

    for (int i = 0; i < INT32_MAX; i++) {
        found = false;

        SLIST_FOREACH(video, &s_video_list, node) {
            if (i == video->id) {
                found = true;
                break;
            }
        }
    
        if (!found) {
            id = i;
            break;
        }
    }

    size = sizeof(struct esp_video) + strlen(name) + 1;
    video = heap_caps_calloc(1, size, ALLOC_RAM_ATTR);
    if (!video) {
        ESP_VIDEO_LOGE("Failed to malloc for video");
        goto errout_video_exits;
    }

    video->min_buffer_count = buffer_count;
    video->min_buffer_size  = buffer_size;

    portMUX_INITIALIZE(&video->lock);
    SLIST_INIT(&video->done_list);
    video->dev_name = (char *)&video[1];
    strcpy(video->dev_name, name);
    video->ops      = ops;
    video->priv     = priv;
    video->id       = id;
    SLIST_INSERT_HEAD(&s_video_list, video, node);

#ifdef CONFIG_ESP_VIDEO_API_LINUX
    char vfs_name[8];

    ret = snprintf(vfs_name, sizeof(vfs_name), "video%d", id);
    if (ret <= 0) {
        ESP_VIDEO_LOGE("Failed to register video VFS dev");
        goto errout_register_vfs;
    }

    ret = esp_vfs_dev_video_register(vfs_name, video);
    if (ret != ESP_OK) {
        ESP_VIDEO_LOGE("Failed to register video VFS dev name=%s", vfs_name);
        goto errout_register_vfs;
    }
#endif

    _lock_release(&s_video_lock);
    return video;

#ifdef CONFIG_ESP_VIDEO_API_LINUX
errout_register_vfs:
    SLIST_REMOVE(&s_video_list, video, esp_video, node);
    heap_caps_free(video);
#endif
errout_video_exits:
    _lock_release(&s_video_lock);
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
    esp_err_t ret;
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it, *tmp;

    if (!video) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

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
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#else
    _lock_acquire(&s_video_lock);
    SLIST_REMOVE(&s_video_list, video, esp_video, node);
    _lock_release(&s_video_lock);
#endif

#ifdef CONFIG_ESP_VIDEO_API_LINUX
    char vfs_name[8];

    ret = snprintf(vfs_name, sizeof(vfs_name), "video%d", video->id);
    if (ret <= 0) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_vfs_dev_video_unregister(vfs_name);
    if (ret <= 0) {
        ESP_VIDEO_LOGE("Failed to unregister video VFS dev name=%s", vfs_name);
        return ESP_ERR_NO_MEM;
    }
#endif

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
        ESP_VIDEO_LOGE("Not find video=%s", name);
        return NULL;
    }

    if (video->ops->init) {
        ret = video->ops->init(video);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->init=%x", ret);
            return NULL;
        } else {
            memset(&video->format, 0, sizeof(struct esp_video_format));
            video->buffer_size  = video->min_buffer_size;
            video->buffer_count = video->min_buffer_count;
            video->buffer       = NULL;
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->init=NULL");
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
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    if (!video) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->deinit) {
        ret = video->ops->deinit(video);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->deinit=%x", ret);
            return ret;
        } else {
            if (video->done_sem) {
                vSemaphoreDelete(video->done_sem);
                video->done_sem = NULL;
            }
            
            if (video->buffer) {
                SLIST_INIT(&video->done_list);
                esp_video_buffer_destroy(video->buffer);
                video->buffer = NULL;
            }
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->deinit=NULL");
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
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    if (!video) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->start_capture) {
        ret = video->ops->start_capture(video);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("ideo->ops->start_capture=%x", ret);
            return ret;
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->start_capture=NULL");
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
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    if (!video) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->stop_capture) {
        ret = video->ops->stop_capture(video);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->stop_capture=%x", ret);
            return ret;
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->stop_capture=NULL");
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
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    if (!video || !capability) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->capability) {
        ret = video->ops->capability(video, capability);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->capability=%x", ret);
            return ret;
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->capability=NULL");
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
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    if (!video || !buffer || !size) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->description) {
        ret = video->ops->description(video, buffer, size);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->description=%x", ret);
            return ret;
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->description=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Get video format information.
 *
 * @param video  Video object
 * @param format Format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_format(struct esp_video *video, struct esp_video_format *format)
{
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    if (!video || !format) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    memcpy(format, &video->format, sizeof(struct esp_video_format));

    return ESP_OK;
}

/**
 * @brief Set video format information.
 *
 * @param video  Video object
 * @param format Format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_format(struct esp_video *video, const struct esp_video_format *format)
{
    esp_err_t ret;
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    if (!video || !format) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return ESP_ERR_INVALID_ARG;
    }

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    if (video->ops->set_format) {
        ret = video->ops->set_format(video, format);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->set_format=%x", ret);
            return ret;
        } else {
            memcpy(&video->format, format, sizeof(struct esp_video_format));
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->set_format=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Setup video buffer.
 *
 * @param video Video object
 * @param count Buffer count
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_setup_buffer(struct esp_video *video, uint32_t count)
{
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }

    if (count < video->min_buffer_count) {
        ESP_VIDEO_LOGE("count=%d < %d", (int)count, (int)video->min_buffer_count);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    video->buffer_count = count;

    if (video->done_sem) {
        vSemaphoreDelete(video->done_sem);
    }
    
    if (video->buffer) {
        SLIST_INIT(&video->done_list);
        esp_video_buffer_destroy(video->buffer);
    }

    video->done_sem = xSemaphoreCreateCounting(video->buffer_count, 0);
    if (!video->done_sem) {
        ESP_VIDEO_LOGE("Failed to create done_sem for video");
        return ESP_ERR_NO_MEM;
    }

    video->buffer = esp_video_buffer_create(video->buffer_count, video->buffer_size);
    if (!video->buffer) {
        vSemaphoreDelete(video->done_sem);
        video->done_sem = NULL;
        ESP_VIDEO_LOGE("Failed to create buffer");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/**
 * @brief Get video buffer count.
 *
 * @param video Video object
 * @param count Buffer count pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_buffer_count(struct esp_video *video, uint32_t *count)
{
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    *count = video->buffer_count;

    return ESP_OK;
}

/**
 * @brief Get video buffer length.
 *
 * @param video Video object
 * @param index Video buffer index
 * @param length Buffer length pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_buffer_length(struct esp_video *video, uint32_t index, uint32_t *length)
{
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    *length = video->buffer_size;

    return ESP_OK;
}

/**
 * @brief Get video buffer offset.
 *
 * @param video Video object
 * @param index Video buffer index
 * @param count Buffer length pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_buffer_offset(struct esp_video *video, uint32_t index, uint32_t *offset)
{
    struct esp_video_buffer_element *element;
#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    bool found = false;
    struct esp_video *it;

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(it, &s_video_list, node) {
        if (it == video) {
            found = true;
            break;
        }
    }
    _lock_release(&s_video_lock);

    if (!found) {
        ESP_VIDEO_LOGE("Not find video=%p", video);
        return ESP_ERR_INVALID_ARG;
    }
#endif

    element = esp_video_buffer_get_element_by_index(video->buffer, index);
    *offset = esp_video_buffer_get_element_offset(video->buffer, element);

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
uint8_t *IRAM_ATTR esp_video_alloc_buffer(struct esp_video *video)
{
    struct esp_video_buffer_element *element;

    element = esp_video_buffer_alloc(video->buffer);
    if (!element) {
        return NULL;
    }

    return esp_video_buffer_element_get_buffer(element);
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
void esp_video_recvdone_buffer(struct esp_video *video, uint8_t *buffer, uint32_t size, uint32_t offset)
{
    struct esp_video_buffer_element *element = 
        container_of(buffer, struct esp_video_buffer_element, buffer);

    /* Check if the buffer is overflow */
    if (size + offset > esp_video_buffer_element_get_buffer_size(element)) {
        abort();
    }
    
    portENTER_CRITICAL_SAFE(&video->lock);
    element->valid_offset = offset;
    esp_video_buffer_element_set_valid_size(element, size);
    SLIST_INSERT_HEAD(&video->done_list, element, node);
    portEXIT_CRITICAL_SAFE(&video->lock);
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
 * @brief Free one video buffer by index.
 *
 * @param video Video object
 * @param index Video buffer index
 *
 * @return None
 */
void esp_video_free_buffer_index(struct esp_video *video, uint32_t index)
{
    struct esp_video_buffer_element *element = esp_video_buffer_get_element_by_index(video->buffer, index);

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
uint8_t *esp_video_recv_buffer(struct esp_video *video, uint32_t *recv_size, uint32_t *offset, uint32_t ticks)
{
    BaseType_t ret;
    struct esp_video_buffer_element *element;

    ret = xSemaphoreTake(video->done_sem, (TickType_t)ticks);
    if (ret != pdTRUE) {
        return NULL;
    }

    portENTER_CRITICAL_SAFE(&video->lock);
    element = SLIST_FIRST(&video->done_list);
    SLIST_REMOVE(&video->done_list, element, esp_video_buffer_element, node); 
    portEXIT_CRITICAL_SAFE(&video->lock);

    *recv_size = esp_video_buffer_element_get_valid_size(element);
    *offset = element->valid_offset;

    return element->buffer;
}

/**
 * @brief Get video buffer data index
 *
 * @param video Video object
 * @param buffer Video data buffer
 *
 * @return Video buffer data index
 */
uint32_t esp_video_get_buffer_index(struct esp_video *video, uint8_t *buffer)
{
    struct esp_video_buffer_element *element = 
        container_of(buffer, struct esp_video_buffer_element, buffer);
    
    return esp_video_buffer_element_get_index(element);
}

uint8_t *esp_video_get_buffer_by_offset(struct esp_video *video, uint32_t offset)
{
    struct esp_video_buffer_element *element = esp_video_buffer_get_element_by_offset(video->buffer, offset);

    return esp_video_buffer_element_get_buffer(element);
}
