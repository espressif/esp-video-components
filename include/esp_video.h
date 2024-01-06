/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <sys/queue.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_video_buffer.h"
#include "esp_camera.h"


#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
#include "esp_media.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

enum esp_video_event {
    ESP_VIDEO_BUFFER_VALID = 0,
};

/**
 * @brief Video capability object.
 */
struct esp_video_capability {

    /* Data format field */

    uint32_t fmt_rgb565  : 1;
    uint32_t fmt_yuv     : 1;
    uint32_t fmt_jpeg    : 1;

    /* Resolution ratio field */;

    uint32_t rr_320p     : 1;
    uint32_t rr_480p     : 1;
    uint32_t rr_720p     : 1;
    uint32_t rr_1080p    : 1;
};

/**
 * @brief Video format object.
 */
struct esp_video_format {
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint32_t pixel_bytes;
    uint32_t fps;
};

/**
 * @brief Video object.
 */
struct esp_video;

/**
 * @brief Video operations object.
 */
struct esp_video_ops {

    /*!< Initializa video hardware and allocate software resource */

    esp_err_t (*init)(struct esp_video *video);

    /*!< De-initializa video hardware and free software resource */

    esp_err_t (*deinit)(struct esp_video *video);

    /*!< Start capturing data stream */

    esp_err_t (*start_capture)(struct esp_video *video);

    /*!< Start capturing data stream */

    esp_err_t (*stop_capture)(struct esp_video *video);

    /*!< Get video capability including data stream format and so on */

    esp_err_t (*capability)(struct esp_video *video, struct esp_video_capability *capability);

    /*!< Get video description string */

    esp_err_t (*description)(struct esp_video *video, char *buffer, uint32_t size);

    /*!< Set video format configuration, and device driver must update buffer_size in this API */

    esp_err_t (*set_format)(struct esp_video *video, const struct esp_video_format *format);

    /*!< Notify driver event triggers */

    void (*notify)(struct esp_video *video, enum esp_video_event event, void *arg);
};

/**
 * @brief Video object.
 */
struct esp_video {
    SLIST_ENTRY(esp_video) node;            /*!< List node */

    int id;                                 /*!< Video device ID */
    const struct esp_video_ops *ops;        /*!< Video operations */
    char *dev_name;                         /*!< Video devic name */
    void *priv;                             /*!< Video device private data */

    struct esp_video_format format;         /*!< Current video format */
    uint32_t buffer_count;                  /*!< User configured buffer count */
    uint32_t buffer_size;                   /*!< User configured buffer size */

    esp_camera_device_t *cam_dev;           /*!< Camera device object */

    esp_video_buffer_list_t done_list;      /*!< Done buffer elements list  */
    portMUX_TYPE lock;                      /*!< Buffer lock */
    SemaphoreHandle_t done_sem;             /*!< Buffer sync semaphore */

    struct esp_video_buffer *buffer;        /*!< Video Buffer */

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    esp_entity_t *entity;                   /*!< The entity of this device */
#endif
};

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
                                   const struct esp_video_ops *ops, void *priv);

/**
 * @brief Destroy video object.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy(struct esp_video *video);

/**
 * @brief Open a video device, this function will initializa hardware.
 *
 * @param name video device name
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_open(const char *name);

/**
 * @brief Close a video device, this function will de-initializa hardware.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_close(struct esp_video *video);

/**
 * @brief Start capturing video data stream.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_start_capture(struct esp_video *video);

/**
 * @brief Stop capturing video data stream.
 *
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_stop_capture(struct esp_video *video);

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
esp_err_t esp_video_get_capability(struct esp_video *video, struct esp_video_capability *capability);

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
esp_err_t esp_video_get_description(struct esp_video *video, char *buffer, uint16_t size);

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
esp_err_t esp_video_get_format(struct esp_video *video, struct esp_video_format *format);

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
esp_err_t esp_video_set_format(struct esp_video *video, const struct esp_video_format *format);

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
esp_err_t esp_video_setup_buffer(struct esp_video *video, uint32_t count);

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
esp_err_t esp_video_get_buffer_count(struct esp_video *video, uint32_t *count);

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
esp_err_t esp_video_get_buffer_length(struct esp_video *video, uint32_t index, uint32_t *length);

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
esp_err_t esp_video_get_buffer_offset(struct esp_video *video, uint32_t index, uint32_t *offset);

/**
 * @brief Allocate one video buffer.
 *
 * @param video Video object
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
uint8_t *esp_video_alloc_buffer(struct esp_video *video);

/**
 * @brief Process a video buffer which receives data done.
 *
 * @param video  Video object
 * @param buffer Video buffer allocated by "esp_video_alloc_buffer"
 * @param size   Actual received data size
 *
 * @return None
 */
void *esp_video_recvdone_buffer(struct esp_video *video, uint8_t *buffer, uint32_t size, uint32_t offset);

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
uint8_t *esp_video_recv_buffer(struct esp_video *video, uint32_t *recv_size, uint32_t *offset, uint32_t ticks);

/**
 * @brief Free one video buffer.
 *
 * @param video  Video object
 * @param buffer Video buffer allocated by "esp_video_alloc_buffer"
 *
 * @return None
 */
void esp_video_free_buffer(struct esp_video *video, uint8_t *buffer);

/**
 * @brief Free one video buffer by index.
 *
 * @param video Video object
 * @param index Video buffer index
 *
 * @return None
 */
void esp_video_free_buffer_index(struct esp_video *video, uint32_t index);

/**
 * @brief Get video buffer data index
 *
 * @param video Video object
 * @param buffer Video data buffer
 *
 * @return Video buffer data index
 */
uint32_t esp_video_get_buffer_index(struct esp_video *video, uint8_t *buffer);

uint8_t *esp_video_get_buffer_by_offset(struct esp_video *video, uint32_t offset);

struct esp_video *esp_video_device_get_object(const char *name);

#ifdef __cplusplus
}
#endif
