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

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_VIDEO_CTRL_FPS      1

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

    /*!< Set data stream attribution before starting capturing, and the attribution should be listed in "capability" */

    esp_err_t (*set_attr)(struct esp_video *video, int cmd, void *arg);

    /*!< Get data stream attribution */

    esp_err_t (*get_attr)(struct esp_video *video, int cmd, void *arg);

    /*!< Get video capability including data stream format and so on */

    esp_err_t (*capability)(struct esp_video *video, struct esp_video_capability *capability);

    /*!< Get video description string */

    esp_err_t (*description)(struct esp_video *video, char *buffer, size_t size);
};

/**
 * @brief Video object.
 */
struct esp_video {
    SLIST_ENTRY(esp_video) node;            /*!< List node */

    const struct esp_video_ops *ops;        /*!< Video operations */
    char *dev_name;                         /*!< Video devic name */
    void *priv;                             /*!< Video device private data */

    esp_video_buffer_list_t done_list;      /*!< Done buffer elements list  */
    portMUX_TYPE lock;                      /*!< Buffer lock */
    SemaphoreHandle_t done_sem;             /*!< Buffer sync semaphore */

    struct esp_video_buffer *buffer;        /*!< Video Buffer */
};

/**
 * @brief Initialize video system board support package.
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_bsp_init(void);

/**
 * @brief Initialize video system.
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_init(void);

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
struct esp_video *esp_video_create(const char *name, const struct esp_video_ops *ops, void *priv, size_t buffer_count, size_t buffer_size);

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
esp_err_t esp_video_set_attr(struct esp_video *video, int cmd, void *arg);

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
esp_err_t esp_video_get_attr(struct esp_video *video, int cmd, void *arg);

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
 *
 * @return None
 */
void esp_video_recvdone_buffer(struct esp_video *video, uint8_t *buffer);

/**
 * @brief Receive buffer from video device. 
 *
 * @param video Video object
 * @param ticks Wait OS tick
 *
 * @return
 *      - Video buffer object pointer on success
 *      - NULL if failed
 */
uint8_t *esp_video_recv_buffer(struct esp_video *video, uint32_t ticks);

/**
 * @brief Free one video buffer.
 *
 * @param video  Video object
 * @param buffer Video buffer allocated by "esp_video_alloc_buffer"
 *
 * @return None
 */
void esp_video_free_buffer(struct esp_video *video, uint8_t *buffer);

#ifdef __cplusplus
}
#endif
