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
#define portMUX_INITIALIZE(mux) spinlock_initialize(mux)
#endif

#define ALLOC_RAM_ATTR (MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)

static _lock_t s_video_lock;
static SLIST_HEAD(esp_video_list, esp_video) s_video_list = SLIST_HEAD_INITIALIZER(s_video_list);
static const char *TAG = "esp_video";

/**
 * @brief Get video stream object pointer by stream type.
 *
 * @param video  Video object
 * @param type   Video stream type
 *
 * @return Video stream object pointer
 */
struct esp_video_stream *IRAM_ATTR esp_video_get_stream(struct esp_video *video, enum v4l2_buf_type type)
{
    struct esp_video_stream *stream = NULL;

    if (video->caps & V4L2_CAP_VIDEO_CAPTURE) {
        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            stream = video->stream;
        }
    } else if (video->caps & V4L2_CAP_VIDEO_OUTPUT) {
        if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
            stream = video->stream;
        }
    } else if (video->caps & V4L2_CAP_VIDEO_M2M) {
        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            stream = &video->stream[0];
        } else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
            stream = &video->stream[1];
        }
    }

    return stream;
}

/**
 * @brief Get video object by name
 *
 * @param name The video object name
 *
 * @return Video object pointer if found by name
 */
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
 * @param name         video driver name
 * @param cam_dev      camera devcie
 * @param ops          video operations
 * @param priv         video private data
 * @param caps         video physical device capabilities
 * @param device_caps  video software device capabilities
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_create(const char *name, esp_camera_device_t *cam_dev,
                                   const struct esp_video_ops *ops, void *priv,
                                   uint32_t caps, uint32_t device_caps)
{
    bool found = false;
    struct esp_video *video;
    uint32_t size;
    int id = -1;
    int stream_count;

#ifdef CONFIG_ESP_VIDEO_CHECK_PARAMETERS
    if (!name || !ops) {
        ESP_VIDEO_LOGE("Input arguments are invalid");
        return NULL;
    }

    if (!ops->set_format) {
        ESP_VIDEO_LOGE("Video operation set_format is needed");
        return NULL;
    }

    if ((device_caps & V4L2_CAP_VIDEO_M2M) && !ops->notify) {
        ESP_VIDEO_LOGE("Video operation notify is needed for M2M device");
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

    stream_count = caps & V4L2_CAP_VIDEO_M2M ? 2 : 1;
    video->stream = heap_caps_calloc(stream_count, sizeof(struct esp_video_stream), ALLOC_RAM_ATTR);
    if (!video->stream) {
        ESP_VIDEO_LOGE("Failed to malloc for stream");
        goto errout_malloc_stream;
    }

    video->dev_name = (char *)&video[1];
    strcpy(video->dev_name, name);
    video->ops = ops;
    video->priv = priv;
    video->id = id;
    video->cam_dev = cam_dev;
    video->caps = caps;
    video->device_caps = device_caps;
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
    heap_caps_free(video->stream);
#endif
errout_malloc_stream:
    heap_caps_free(video);
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
            int stream_count = video->caps & V4L2_CAP_VIDEO_M2M ? 2 : 1;

            for (int i = 0; i < stream_count; i++) {
                video->stream[i].buffer = NULL;
            }
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
            int stream_count = video->caps & V4L2_CAP_VIDEO_M2M ? 2 : 1;

            for (int i = 0; i < stream_count; i++) {
                struct esp_video_stream *stream = &video->stream[i];

                if (stream->ready_sem) {
                    vSemaphoreDelete(stream->ready_sem);
                    stream->ready_sem = NULL;
                }

                if (stream->buffer) {
#ifndef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
                    esp_video_buffer_destroy(stream->buffer);
                    stream->buffer = NULL;
#else
                    if (esp_video_device_is_user_node(video)) {
                        esp_pipeline_destory_video_buffer(esp_pad_get_pipeline(video->priv));
                        stream->buffer = NULL;
                    }
#endif
                }
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
 * @param type  Video stream type
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_start_capture(struct esp_video *video, uint32_t type)
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

    if (video->ops->start) {
        ret = video->ops->start(video, type);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->start=%x", ret);
            return ret;
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->start=NULL");
    }

    return ESP_OK;
}

/**
 * @brief Stop capturing video data stream.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_stop_capture(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    struct esp_video_stream *stream;

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

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    if (video->ops->stop) {
        ret = video->ops->stop(video, type);
        if (ret != ESP_OK) {
            ESP_VIDEO_LOGE("video->ops->stop=%x", ret);
            return ret;
        }
    } else {
        ESP_VIDEO_LOGI("video->ops->stop=NULL");
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
 * @param type   Video stream type
 * @param format Video stream format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_format(struct esp_video *video, uint32_t type, struct esp_video_format *format)
{
    struct esp_video_stream *stream;

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

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(format, &stream->format, sizeof(struct esp_video_format));

    return ESP_OK;
}

/**
 * @brief Set video format information.
 *
 * @param video  Video object
 * @param type   Video stream type
 * @param format Video stream format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_format(struct esp_video *video, uint32_t type, const struct esp_video_format *format)
{
    esp_err_t ret;
    struct esp_video_stream *stream;

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

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = video->ops->set_format(video, type, format);
    if (ret != ESP_OK) {
        ESP_VIDEO_LOGE("video->ops->set_format=%x", ret);
        return ret;
    } else {
        memcpy(&stream->format, format, sizeof(struct esp_video_format));
    }

    return ESP_OK;
}

/**
 * @brief Setup video buffer.
 *
 * @param video Video object
 * @param type  Video stream type
 * @param count Video buffer count
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_setup_buffer(struct esp_video *video, uint32_t type, uint32_t count)
{
    struct esp_video_stream *stream;
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

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    info = &stream->buf_info;
    if (!info->size || !info->align_size || !info->caps) {
        ESP_VIDEO_LOGE("Failed to check buffer information: size=%" PRIu32 " align=%" PRIu32 " cap=%" PRIx32,
                       info->size, info->align_size, info->caps);
        abort();
        return ESP_ERR_INVALID_STATE;
    }

    info->count = count;

    if (stream->ready_sem) {
        vSemaphoreDelete(stream->ready_sem);
        stream->ready_sem = NULL;
    }

    if (stream->buffer) {
#ifndef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
        esp_video_buffer_destroy(stream->buffer);
        stream->buffer = NULL;
#else
        if (esp_video_device_is_user_node(video)) {
            esp_pipeline_destory_video_buffer(esp_pad_get_pipeline(video->priv));
            stream->buffer = NULL;
        }
#endif
    }

    stream->ready_sem = xSemaphoreCreateCounting(info->count, 0);
    if (!stream->ready_sem) {
        ESP_VIDEO_LOGE("Failed to create done_sem for video stream");
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
    stream->buffer = esp_video_buffer_create(info);
#endif
    if (!stream->buffer) {
        vSemaphoreDelete(stream->ready_sem);
        stream->ready_sem = NULL;
        ESP_VIDEO_LOGE("Failed to create buffer");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/**
 * @brief Get video buffer count.
 *
 * @param video Video object
 * @param type  Video stream type
 * @param attr  Video stream buffer information pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_buffer_info(struct esp_video *video, uint32_t type, struct esp_video_buffer_info *info)
{
    struct esp_video_stream *stream;
    struct esp_video_buffer_info *buffer_info;

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

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }
    buffer_info = &stream->buf_info;

    info->count = buffer_info->count;
    info->size = buffer_info->size;
    info->align_size = buffer_info->align_size;
    info->caps = buffer_info->caps;

    return ESP_OK;
}

/**
 * @brief Get buffer element from buffer queued list.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *IRAM_ATTR esp_video_get_queued_element(struct esp_video *video, uint32_t type)
{
    struct esp_video_stream *stream;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return NULL;
    }

    return esp_video_buffer_get_queued_element(stream->buffer);
}

/**
 * @brief Get buffer element's payload from buffer queued list.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
uint8_t *IRAM_ATTR esp_video_get_queued_buffer(struct esp_video *video, uint32_t type)
{
    struct esp_video_buffer_element *element;

    element = esp_video_get_queued_element(video, type);
    if (!element) {
        return NULL;
    }

    return element->buffer;
}

/**
 * @brief Get buffer element from buffer done list.
 *
 * @param video Video object
 * @param type  Video stream type
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_get_done_element(struct esp_video *video, uint32_t type)
{
    struct esp_video_stream *stream;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return NULL;
    }

    return esp_video_buffer_get_done_element(stream->buffer);
}

/**
 * @brief Put element into done lost and give semaphore.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param element Video buffer element object get by "esp_video_get_queued_element"
 *
 * @return None
 */
void IRAM_ATTR esp_video_done_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element)
{
    bool user_node = true;

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    if (!esp_video_device_is_user_node(video)) {
        user_node = false;
    }
#endif

    if (user_node) {
        struct esp_video_stream *stream;

        stream = esp_video_get_stream(video, type);
        if (!stream) {
            return;
        }

        esp_video_buffer_done(stream->buffer, element);

        if (xPortInIsrContext()) {
            BaseType_t wakeup = pdFALSE;

            xSemaphoreGiveFromISR(stream->ready_sem, &wakeup);
            if (wakeup == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        } else {
            xSemaphoreGive(stream->ready_sem);
        }
    }
}

/**
 * @brief Process a video buffer element's payload which receives data done.
 *
 * @param video  Video object
 * @param type   Video stream type
 * @param buffer Video buffer element's payload
 * @param n      Video buffer element's payload valid data size
 *
 * @return None
 */
void IRAM_ATTR esp_video_done_buffer(struct esp_video *video, uint32_t type, uint8_t *buffer, uint32_t n)
{
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return;
    }

    element = esp_video_buffer_get_element_by_buffer(stream->buffer, buffer);
    if (element) {
        element->valid_size = n;
        esp_video_done_element(video, type, element);
    }
}

/**
 * @brief Put buffer element into queued list.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param element Video buffer element
 *
 * @return None
 */
void esp_video_queue_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element)
{
    uint32_t val = type;
    struct esp_video_buffer *vbuf = element->video_buffer;

    esp_video_buffer_queue(vbuf, element);

    if (video->ops->notify) {
        video->ops->notify(video, ESP_VIDEO_BUFFER_VALID, &val);
    }
}

/**
 * @brief Put buffer element index into queued list.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param element Video buffer element
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_queue_element_index(struct esp_video *video, uint32_t type, int index)
{
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    element = ESP_VIDEO_BUFFER_ELEMENT(stream->buffer, index);

    esp_video_queue_element(video, type, element);

    return ESP_OK;
}

/**
 * @brief Get buffer element payload.
 *
 * @param video Video object
 * @param type  Video stream type
 * @param index Video buffer element index
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
uint8_t *esp_video_get_element_index_payload(struct esp_video *video, uint32_t type, int index)
{
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return NULL;
    }

    element = ESP_VIDEO_BUFFER_ELEMENT(stream->buffer, index);

    return element->buffer;
}

/**
 * @brief Receive buffer element from video device.
 *
 * @param video Video object
 * @param type  Video stream type
 * @param ticks Wait OS tick
 *
 * @return
 *      - Video buffer element object pointer on success
 *      - NULL if failed
 */
struct esp_video_buffer_element *esp_video_recv_element(struct esp_video *video, uint32_t type, uint32_t ticks)
{
    BaseType_t ret;
    struct esp_video_stream *stream;
    struct esp_video_buffer_element *element;

    stream = esp_video_get_stream(video, type);
    if (!stream) {
        return NULL;
    }

    if (video->device_caps & V4L2_CAP_VIDEO_M2M) {
        uint32_t val = type;

        /**
         * Software M2M device: this callback call can do real codec process.
         * Hardware M2M device: this callback call can start hardware if necessary.
         */

        ret = video->ops->notify(video, ESP_VIDEO_M2M_TRIGGER, &val);
        if (ret != ESP_OK) {
            return NULL;
        }
    }

    ret = xSemaphoreTake(stream->ready_sem, (TickType_t)ticks);
    if (ret != pdTRUE) {
        return NULL;
    }

    element = esp_video_buffer_get_done_element(stream->buffer);

    return element;
}
