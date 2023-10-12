/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_video_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

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

    /*!< Set data stream format before starting capturing, and the format should be listed in "capability" */

    esp_err_t (*set_fmt)(struct esp_video *video, int cmd, void *arg);

    /*!< Get data stream format */

    esp_err_t (*get_fmt)(struct esp_video *video, int cmd, void *arg);

    /*!< Get video capability including data stream format and so on */

    esp_err_t (*capability)(struct esp_video *video, struct esp_video_capability *capability);

    /*!< Get video description string */

    esp_err_t (*description)(struct esp_video *video, char *buffer, size_t size);
};

/**
 * @brief Video object.
 */
struct esp_video {
    const esp_video_ops_t *ops;             /*!< Video operations */
    struct esp_video_buffer *buffer;        /*!< Video buffer */
    char *dev_name;                         /*!< Video devic name */
    void *priv;                             /*!< Video device private data */
};

/**
 * @brief Create video object.
 *
 * @param name         video device name
 * @param ops          video operations
 * @param buffer_count video buffer count for lowlevel driver
 * @param buffer_size  video buffer size for lowlevel driver
 *
 * @return
 *      - Video object pointer on success
 *      - NULL if failed
 */
struct esp_video *esp_video_create(const char *name, const esp_video_ops_t *ops, size_t buffer_count, size_t buffer_size);

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
 * @brief Set video data stream format.
 *
 * @param video Video object
 * @param cmd   Video set command
 * @param arg   Video set parameters pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_fmt(struct esp_video *video, int cmd, void *arg);

/**
 * @brief Get video data stream format.
 *
 * @param video Video object
 * @param cmd   Video get command
 * @param arg   Video get parameters pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_fmt(struct esp_video *video, int cmd, void *arg);

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
