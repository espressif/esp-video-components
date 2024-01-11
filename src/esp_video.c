/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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
#include "esp_color_formats.h"

#include "freertos/portmacro.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#define portMUX_INITIALIZE(mux)             spinlock_initialize(mux)
#endif

#define ALLOC_RAM_ATTR              (MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)
#define VIDEO_OPERATION_CHECK       1

static _lock_t s_video_lock;
static SLIST_HEAD(esp_video_list, esp_video) s_video_list = SLIST_HEAD_INITIALIZER(s_video_list);

static const char *TAG = "esp_video";

struct esp_video *esp_video_device_get_object(const char *name)
{
    struct esp_video *video;

    _lock_acquire(&s_video_lock);
    SLIST_FOREACH(video, &s_video_list, node) {
        ESP_VIDEO_LOGI("dev_name=%s", video->dev_name);
        if (!strcmp(video->dev_name, name)) {
            _lock_release(&s_video_lock);
            return video;
        }
    }

    _lock_release(&s_video_lock);
    return NULL;
}

/**
 * @brief Create video object.
 *
 * @param name         video device name
 * @param cam_dev      camera devcie
 * @param ops          video operations
 * @param priv         video private data
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_create(const char *name, esp_camera_device_t *cam_dev,
                                   const struct esp_video_ops *ops, void *priv)
{
    bool found = false;
    struct esp_video *video;
    uint32_t size;
    int id = -1;

#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    if (!name || !ops) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return NULL;
    }

    if (!ops->set_format) {
        ESP_VIDEO_LOGE("Video Operation set_format is needed");
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

    portMUX_INITIALIZE(&video->lock);
    SLIST_INIT(&video->done_list);
    video->dev_name = (char *)&video[1];
    strcpy(video->dev_name, name);
    video->ops      = ops;
    video->priv     = priv;
    video->id       = id;
    video->cam_dev  = cam_dev;
    SLIST_INSERT_HEAD(&s_video_list, video, node);

#if defined(CONFIG_ESP_VIDEO_API_LINUX) && !defined(CONFIG_ESP_VIDEO_MEDIA_CONTROLLER)
    char vfs_name[8];

    esp_err_t ret = snprintf(vfs_name, sizeof(vfs_name), "video%d", id);
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
#if defined(CONFIG_ESP_VIDEO_API_LINUX) && !defined(CONFIG_ESP_VIDEO_MEDIA_CONTROLLER)
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

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    esp_entity_delete(video->entity);
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
        /* video device operation "init" sets buffer information and video format */

        ret = video->ops->init(video);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->init=%x", ret);
            return NULL;
        } else {
            video->buffer = NULL;
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
#ifndef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
                esp_video_buffer_destroy(video->buffer);
                video->buffer = NULL;
#else
                if (esp_video_device_is_user_node(video)) {
                    esp_pipeline_destory_video_buffer(esp_pad_get_pipeline(video->priv));
                    video->buffer = NULL;
                }
#endif
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
            ESP_VIDEO_LOGE("video->ops->start_capture=%x", ret);
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

    ret = video->ops->set_format(video, format);
    if (ret != ESP_OK) {
        ESP_VIDEO_LOGE("video->ops->set_format=%x", ret);
        return ret;
    } else {
        memcpy(&video->format, format, sizeof(struct esp_video_format));
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
    struct esp_video_buffer_info *info;

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

    /* buffer_size is configured when setting format */
    info = &video->buf_info;
    if (!info->size || !info->align_size || !info->caps) {
        ESP_VIDEO_LOGE("Failed to check buffer information: size=%" PRIu32 " align=%" PRIu32 " cap=%" PRIx32,
                       info->size, info->align_size, info->caps);
        return ESP_ERR_INVALID_STATE;
    }

    info->count = count;

    if (video->done_sem) {
        vSemaphoreDelete(video->done_sem);
    }

    if (video->buffer) {
        SLIST_INIT(&video->done_list);
#ifndef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
        esp_video_buffer_destroy(video->buffer);
        video->buffer = NULL;
#else
        if (esp_video_device_is_user_node(video)) {
            esp_pipeline_destory_video_buffer(esp_pad_get_pipeline(video->priv));
            video->buffer = NULL;
        }
#endif
    }

    video->done_sem = xSemaphoreCreateCounting(info->count, 0);
    if (!video->done_sem) {
        ESP_VIDEO_LOGE("Failed to create done_sem for video");
        return ESP_ERR_NO_MEM;
    }

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    esp_pipeline_t *pipeline = NULL;
    if (esp_video_device_is_user_node(video)) {
        pipeline = esp_pad_get_pipeline(video->priv);
        esp_pipeline_create_video_buffer(pipeline);
        video->buffer = esp_pipeline_get_video_buffer(pipeline);
    }

    ESP_VIDEO_LOGI("%s buffer created %p", video->dev_name, video->buffer);
#else
    video->buffer = esp_video_buffer_create(info);
#endif
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

    *count = video->buf_info.count;

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

    *length = video->buf_info.size;

    return ESP_OK;
}

/**
 * @brief Get video buffer count.
 *
 * @param video Video object
 * @param attr  Buffer Information pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_buffer_info(struct esp_video *video, struct esp_video_buffer_info *info)
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

    info->count = video->buf_info.count;
    info->size  = video->buf_info.size;
    info->align_size = video->buf_info.align_size;
    info->caps = video->buf_info.caps;

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

void *IRAM_ATTR esp_video_recvdone_buffer(struct esp_video *video, uint8_t *buffer, uint32_t size, uint32_t offset)
{
    struct esp_video_buffer_element *element = esp_video_buffer_get_element_by_buffer(video->buffer, buffer);

    bool user_node = true;
    void *ret_ptr = NULL;
    /* Check if the buffer is overflow */
    if (size + offset > esp_video_buffer_element_get_buffer_size(element)) {
        abort();
    }

    // ToDo: Process device data
    ret_ptr = buffer;
    element->valid_offset = offset;
    esp_video_buffer_element_set_valid_size(element, size);
#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    if (!esp_video_device_is_user_node(video)) {
        user_node = false;
    }
#endif
    if (user_node) {
        portENTER_CRITICAL_SAFE(&video->lock);
        SLIST_INSERT_HEAD(&video->done_list, element, node);
        portEXIT_CRITICAL_SAFE(&video->lock);
        if (xPortInIsrContext()) {
            BaseType_t wakeup;
            xSemaphoreGiveFromISR(video->done_sem, &wakeup);
            if (wakeup == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        } else {
            xSemaphoreGive(video->done_sem);
        }
    }

    return ret_ptr;
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
    struct esp_video_buffer_element *element = esp_video_buffer_get_element_by_buffer(video->buffer, buffer);

    esp_video_buffer_free(video->buffer, element);

    if (video->ops->notify) {
        video->ops->notify(video, ESP_VIDEO_BUFFER_VALID, buffer);
    }
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

    if (video->ops->notify) {
        video->ops->notify(video, ESP_VIDEO_BUFFER_VALID, element->buffer);
    }
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
    struct esp_video_buffer_element *element = esp_video_buffer_get_element_by_buffer(video->buffer, buffer);

    return esp_video_buffer_element_get_index(element);
}

/**
 * @brief Get video buffer data index
 *
 * @param video Video object
 * @param buffer Video data buffer
 *
 * @return Video buffer data index
 */
uint8_t *esp_video_get_buffer_by_offset(struct esp_video *video, uint32_t offset)
{
    struct esp_video_buffer_element *element = esp_video_buffer_get_element_by_offset(video->buffer, offset);

    return esp_video_buffer_element_get_buffer(element);
}

/**
 * Todo: AEG-1094
 */
static const int s_control_id_map_table[][2] = {
    { V4L2_CID_3A_LOCK, CAM_SENSOR_3A_LOCK },
    { V4L2_CID_FLASH_LED_MODE, CAM_SENSOR_FLASH_LED }
};

static esp_err_t v4l2_ext_control_id_map(uint32_t *id)
{
    for (int i = 0; i < ARRAY_SIZE(s_control_id_map_table); i++) {
        if (s_control_id_map_table[i][0] == *id) {
            *id = s_control_id_map_table[i][1];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

static esp_err_t esp_video_ioctl_querycap(struct esp_video *video, struct v4l2_capability *cap)
{
    memset(cap, 0, sizeof(struct v4l2_capability));

    strlcpy((char *)cap->driver, video->dev_name, sizeof(cap->driver));
    cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
    // ToDo:

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_g_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    esp_err_t ret;
    struct esp_video_format format;

    if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_format(video, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    memset(&fmt->fmt, 0, sizeof(fmt->fmt));
    fmt->fmt.pix.width       = format.width;
    fmt->fmt.pix.height      = format.height;
    fmt->fmt.pix.pixelformat = format.pixel_format;

    return ret;
}

static esp_err_t esp_video_ioctl_s_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    esp_err_t ret;
    struct esp_video_format format;
    if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&format, 0, sizeof(struct esp_video_format));
    format.bpp = esp_video_get_bpp_by_format(fmt->fmt.pix.pixelformat);
    if (!format.bpp) {
        return ESP_ERR_INVALID_ARG;
    }
    format.width        = fmt->fmt.pix.width;
    format.height       = fmt->fmt.pix.height;
    format.pixel_format = fmt->fmt.pix.pixelformat;
    ret = esp_video_set_format(video, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    return ret;
}

static esp_err_t esp_video_ioctl_try_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    return esp_video_ioctl_s_fmt(video, fmt);
}

static esp_err_t esp_video_ioctl_streamon(struct esp_video *video, int *arg)
{
    esp_err_t ret;

    ret = esp_video_start_capture(video);

    return ret;
}

static esp_err_t esp_video_ioctl_streamoff(struct esp_video *video, int *arg)
{
    esp_err_t ret;

    ret = esp_video_stop_capture(video);

    return ret;
}

static esp_err_t esp_video_ioctl_reqbufs(struct esp_video *video, struct v4l2_requestbuffers *req_bufs)
{
    esp_err_t ret;

    /* Only support memory buffer MMAP mode */

    if (req_bufs->memory != V4L2_MEMORY_MMAP) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Only support camera capture */

    if (req_bufs->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_INVALID_ARG;
    }

    if (req_bufs->count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    //ToDo:

    ret = esp_video_setup_buffer(video, req_bufs->count);

    return ret;
}

static esp_err_t esp_video_ioctl_querybuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    uint32_t count;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Only support camera capture */

    if (vbuf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_buffer_count(video, &count);
    if (ret != ESP_OK) {
        return ret;
    }

    if (vbuf->index >= count) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_buffer_length(video, vbuf->index, &vbuf->length);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_video_get_buffer_offset(video, vbuf->index, &vbuf->m.offset);
    if (ret != ESP_OK) {
        return ret;
    }

    return ret;
}

static esp_err_t esp_video_ioctl_mmap(struct esp_video *video, struct esp_video_ioctl_mmap *ioctl_mmap)
{
    uint8_t *buffer;

    if (ioctl_mmap->length > video->buf_info.size) {
        return ESP_ERR_INVALID_ARG;
    }

    buffer = esp_video_get_buffer_by_offset(video, ioctl_mmap->offset);
    if (!buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    ioctl_mmap->mapped_ptr = buffer;

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_qbuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    uint32_t count;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Only support camera capture */

    if (vbuf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_buffer_count(video, &count);
    if (ret != ESP_OK) {
        return ret;
    }

    if (vbuf->index >= count) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_video_free_buffer_index(video, vbuf->index);

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_dqbuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    uint32_t count;
    uint32_t recv_size;
    uint32_t offset;
    uint8_t *recv_buf;
    uint32_t ticks = portMAX_DELAY;

    /* Only support memory buffer MMAP mode */

    if (vbuf->memory != V4L2_MEMORY_MMAP) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Only support camera capture */

    if (vbuf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_buffer_count(video, &count);
    if (ret != ESP_OK) {
        return ret;
    }

    if (vbuf->index >= count) {
        return ESP_ERR_INVALID_ARG;
    }

    recv_buf = esp_video_recv_buffer(video, &recv_size, &offset, ticks);
    if (!recv_buf) {
        return ESP_FAIL;
    }

    vbuf->index     = esp_video_get_buffer_index(video, recv_buf);
    vbuf->bytesused = recv_size;
    vbuf->m.offset = offset;

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_s_param(struct esp_video *video, struct v4l2_streamparm *param)
{
    esp_err_t ret;
    struct esp_video_format format;

    if (param->parm.capture.timeperframe.numerator != 1) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_get_format(video, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    format.fps = param->parm.capture.timeperframe.denominator;
    ret = esp_video_set_format(video, &format);
    if (ret != ESP_OK) {
        return ret;
    }

    return ret;
}

static esp_err_t esp_video_ioctl_op_ext_ctrls(struct esp_video *video, struct v4l2_ext_controls *controls, bool set)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    esp_camera_device_t *cam_dev = video->cam_dev;
    //ToDo:
    for (int i = 0; i < controls->count; i++) {
        struct v4l2_ext_control *ctrl = &controls->controls[i];

        if (controls->ctrl_class == V4L2_CTRL_CLASS_PRIV) {
            if (set) {
                ret = esp_camera_set_para_value(cam_dev, ctrl);
            } else {
                ret = esp_camera_get_para_value(cam_dev, ctrl);
            }
        } else {
            uint32_t ctrl_id = ctrl->id;
            uint32_t new_id = ctrl_id;

            ret = v4l2_ext_control_id_map(&new_id);
            if (ret != ESP_OK) {
                break;
            }

            ctrl->id = new_id;
            if (set) {
                ret = esp_camera_set_para_value(cam_dev, ctrl);
            } else {
                ret = esp_camera_get_para_value(cam_dev, ctrl);
            }
            ctrl->id = ctrl_id;

            if (ret != ESP_OK) {
                break;
            }
        }
    }

    return ret;
}

/**
 * Todo: AEG-1095
 */
static esp_err_t esp_video_ioctl_query_ext_ctrls(struct esp_video *video, struct v4l2_query_ext_ctrl *qc)
{
    esp_err_t ret;
    esp_camera_device_t *cam_dev = video->cam_dev;
    //ToDo:
    if (V4L2_CTRL_ID2CLASS(qc->id) == V4L2_CTRL_CLASS_PRIV) {
        ret = esp_camera_query_para_desc(cam_dev, qc);
    } else {
        uint32_t qc_id = qc->id;
        uint32_t new_id = qc_id;

        ret = v4l2_ext_control_id_map(&new_id);
        if (ret != ESP_OK) {
            goto exit;
        }

        qc->id = new_id;
        ret = esp_camera_query_para_desc(cam_dev, qc);
        qc->id = qc_id;

        if (ret != ESP_OK) {
            goto exit;
        }
    }

exit:

    return ret;
}

esp_err_t esp_video_ioctl(struct esp_video *video, int cmd, va_list args)
{
    esp_err_t ret = ESP_OK;
    void *arg_ptr;

    assert(video);

    arg_ptr = va_arg(args, void *);
    if (!arg_ptr) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (cmd) {
    case VIDIOC_QBUF:
        ret = esp_video_ioctl_qbuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_DQBUF:
        ret = esp_video_ioctl_dqbuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_QUERYCAP:
        ret = esp_video_ioctl_querycap(video, (struct v4l2_capability *)arg_ptr);
        break;
    case VIDIOC_G_FMT:
        ret = esp_video_ioctl_g_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_S_FMT:
        ret = esp_video_ioctl_s_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_TRY_FMT:
        ret = esp_video_ioctl_try_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_STREAMON:
        ret = esp_video_ioctl_streamon(video, (int *)arg_ptr);
        break;
    case VIDIOC_STREAMOFF:
        ret = esp_video_ioctl_streamoff(video, (int *)arg_ptr);
        break;
    case VIDIOC_REQBUFS:
        ret = esp_video_ioctl_reqbufs(video, (struct v4l2_requestbuffers *)arg_ptr);
        break;
    case VIDIOC_QUERYBUF:
        ret = esp_video_ioctl_querybuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_S_PARM:
        ret = esp_video_ioctl_s_param(video, (struct v4l2_streamparm *)arg_ptr);
        break;
    case VIDIOC_MMAP:
        ret = esp_video_ioctl_mmap(video, (struct esp_video_ioctl_mmap *)arg_ptr);
        break;
    case VIDIOC_G_EXT_CTRLS:
        ret = esp_video_ioctl_op_ext_ctrls(video, (struct v4l2_ext_controls *)arg_ptr, false);
        break;
    case VIDIOC_S_EXT_CTRLS:
        ret = esp_video_ioctl_op_ext_ctrls(video, (struct v4l2_ext_controls *)arg_ptr, true);
        break;
    case VIDIOC_QUERY_EXT_CTRL:
        ret = esp_video_ioctl_query_ext_ctrls(video, (struct v4l2_query_ext_ctrl *)arg_ptr);
        break;
    case VIDIOC_ENUM_FMT:
    case VIDIOC_ENUM_FRAMESIZES:
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}
