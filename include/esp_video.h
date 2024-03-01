/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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

#define VIDEO_PRIV_DATA(t, v)               ((t)(v)->priv)
#define VIDEO_CAM_DEV(v)                    ((v)->cam_dev)

#define STREAM_FORMAT(s)                    (&(s)->format)
#define STREAM_BUF_INFO(s)                  (&(s)->buf_info)

#define STREAM_BUFFER_SIZE(s)               (STREAM_BUF_INFO(s)->size)

#define SET_BUF_INFO(bi, s, a, c)           \
{                                           \
    (bi)->size = (s);                       \
    (bi)->align_size = (a);                 \
    (bi)->caps = (c);                       \
}

#define SET_FORMAT(fmt, _fps, _width, _height, _pixel_format, _bpp)     \
{                                                                       \
    (fmt)->fps = (_fps);                                                \
    (fmt)->width = (_width);                                            \
    (fmt)->height = (_height);                                          \
    (fmt)->pixel_format = (_pixel_format);                              \
    (fmt)->bpp = (_bpp);                                                \
}

#define SET_STREAM_BUF_INFO(st, s, a, c)                                \
    SET_BUF_INFO(STREAM_BUF_INFO(st), s, a, c)

#define SET_STREAM_FORMAT(st, fps, w, h, pixel_format, bpp)             \
    SET_FORMAT(STREAM_FORMAT(st), fps, w, h, pixel_format, bpp)

#define CAPTURE_VIDEO_STREAM(v)             ((v)->stream)
#define CAPTURE_VIDEO_BUF_SIZE(v)           STREAM_BUFFER_SIZE(CAPTURE_VIDEO_STREAM(v))

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
#define CAPTURE_VIDEO_DONE_BUF(v, b, n)     esp_video_media_done_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE, b, n)
#else
#define CAPTURE_VIDEO_DONE_BUF(v, b, n)     esp_video_done_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE, b, n)
#endif

#define CAPTURE_VIDEO_SET_FORMAT(v, fps, w, h, pixel_format, bpp)       \
    SET_STREAM_FORMAT(CAPTURE_VIDEO_STREAM(v), fps, w, h, pixel_format, bpp)

#define CAPTURE_VIDEO_SET_BUF_INFO(v, s, a, c)                          \
    SET_STREAM_BUF_INFO(CAPTURE_VIDEO_STREAM(v), s, a, c)

#define CAPTURE_VIDEO_GET_QUEUED_BUF(v)                                 \
    esp_video_get_queued_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE);

#define M2M_VIDEO_CAPTURE_STREAM(v)         (&(v)->stream[0])
#define M2M_VIDEO_OUTPUT_STREAM(v)          (&(v)->stream[1])

enum esp_video_event {
    ESP_VIDEO_BUFFER_VALID = 0,
    ESP_VIDEO_M2M_TRIGGER,
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
    uint32_t width;                         /*!< Video frame width */
    uint32_t height;                        /*!< Video frame height */
    uint32_t pixel_format;                  /*!< Video frame pixel format */
    uint8_t bpp;                            /*!< Video frame bits per pixel */
    uint32_t fps;                           /*!< Video outputting frames per second */
};

/**
 * @brief Video object.
 */
struct esp_video;

/**
 * @brief Video operations object.
 */
struct esp_video_ops {

    /*!< Initializa video hardware and allocate software resource, and must set buffer information and video format */

    esp_err_t (*init)(struct esp_video *video);

    /*!< De-initializa video hardware and free software resource */

    esp_err_t (*deinit)(struct esp_video *video);

    /*!< Start data stream */

    esp_err_t (*start)(struct esp_video *video, uint32_t type);

    /*!< Start data stream */

    esp_err_t (*stop)(struct esp_video *video, uint32_t type);

    /*!< Get video capability including data stream format and so on */

    esp_err_t (*capability)(struct esp_video *video, struct esp_video_capability *capability);

    /*!< Get video description string */

    esp_err_t (*description)(struct esp_video *video, char *buffer, uint32_t size);

    /*!< Set video format configuration */

    esp_err_t (*set_format)(struct esp_video *video, uint32_t type, const struct esp_video_format *format);

    /*!< Notify driver event triggers */

    esp_err_t (*notify)(struct esp_video *video, enum esp_video_event event, void *arg);
};

/**
 * @brief Video stream object.
 */
struct esp_video_stream {
    struct esp_video_format format;         /*!< Video stream format */
    struct esp_video_buffer_info buf_info;  /*!< Video stream buffer information */

    esp_video_buffer_list_t queued_list;    /*!< Workqueue buffer elements list */
    esp_video_buffer_list_t done_list;      /*!< Done buffer elements list */

    struct esp_video_buffer *buffer;        /*!< Video stream buffer */
    SemaphoreHandle_t ready_sem;            /*!< Video stream buffer element ready semaphore */
};

/**
 * @brief Video object.
 */
struct esp_video {
    SLIST_ENTRY(esp_video) node;            /*!< List node */

    int id;                                 /*!< Video device ID */
    const struct esp_video_ops *ops;        /*!< Video operations */
    char *dev_name;                         /*!< Video device port name */
    uint32_t caps;                          /*!< video physical device capabilities */
    uint32_t device_caps;                   /*!< video software device capabilities */

    void *priv;                             /*!< Video device private data */

    esp_camera_device_t *cam_dev;           /*!< Camera device object */

    portMUX_TYPE stream_lock;               /*!< Stream list lock */
    struct esp_video_stream *stream;        /*!< Video device stream, capture-only or output-only device has 1 stream, M2M device has 2 streams */

#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    esp_entity_t *entity;                   /*!< The entity of this device */
#endif
};

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
                                   uint32_t caps, uint32_t device_caps);

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
 * @param type  Video stream type
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_start_capture(struct esp_video *video, uint32_t type);

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
esp_err_t esp_video_stop_capture(struct esp_video *video, uint32_t type);

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
 * @param video     Video object
 * @param type      Video stream type
 * @param format    Video stream format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_format(struct esp_video *video, uint32_t type, struct esp_video_format *format);

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
esp_err_t esp_video_set_format(struct esp_video *video, uint32_t type, const struct esp_video_format *format);

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
esp_err_t esp_video_setup_buffer(struct esp_video *video, uint32_t type, uint32_t count);

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
esp_err_t esp_video_get_buffer_info(struct esp_video *video, uint32_t type, struct esp_video_buffer_info *info);

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
struct esp_video_buffer_element *esp_video_get_queued_element(struct esp_video *video, uint32_t type);

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
uint8_t *esp_video_get_queued_buffer(struct esp_video *video, uint32_t type);

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
struct esp_video_buffer_element *esp_video_get_done_element(struct esp_video *video, uint32_t type);

/**
 * @brief Process a done video buffer element.
 *
 * @param video  Video object
 * @param stream Video stream object
 * @param buffer Video buffer element object allocated by "esp_video_get_queued_element"
 *
 * @return None
 */
void esp_video_stream_done_element(struct esp_video *video, struct esp_video_stream *stream, struct esp_video_buffer_element *element);

/**
 * @brief Put element into done lost and give semaphore.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param element Video buffer element object get by "esp_video_get_queued_element"
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_done_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element);

/**
 * @brief Process a video buffer element's payload which receives data done.
 *
 * @param video  Video object
 * @param type   Video stream type
 * @param buffer Video buffer element's payload
 * @param n      Video buffer element's payload valid data size
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_done_buffer(struct esp_video *video, uint32_t type, uint8_t *buffer, uint32_t n);

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
struct esp_video_buffer_element *esp_video_recv_element(struct esp_video *video, uint32_t type, uint32_t ticks);

/**
 * @brief Put buffer element into queued list.
 *
 * @param video   Video object
 * @param type    Video stream type
 * @param element Video buffer element
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_queue_element(struct esp_video *video, uint32_t type, struct esp_video_buffer_element *element);

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
esp_err_t esp_video_queue_element_index(struct esp_video *video, uint32_t type, int index);

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
uint8_t *esp_video_get_element_index_payload(struct esp_video *video, uint32_t type, int index);

/**
 * @brief Get video object by name
 *
 * @param name The video object name
 *
 * @return Video object pointer if found by name
 */
struct esp_video *esp_video_device_get_object(const char *name);

/**
 * @brief Get video stream object pointer by stream type.
 *
 * @param video  Video object
 * @param type   Video stream type
 *
 * @return Video stream object pointer
 */
struct esp_video_stream *esp_video_get_stream(struct esp_video *video, enum v4l2_buf_type type);

/**
 * @brief Get video buffer type.
 *
 * @param video  Video object
 *
 * @return the type left shift bits
 */
uint32_t esp_video_get_buffer_type_bits(struct esp_video *video);

/**
 * @brief Set video stream buffer
 *
 * @param video  Video object
 * @param type   Video stream type
 * @param buffer video buffer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_stream_buffer(struct esp_video *video, enum v4l2_buf_type type, struct esp_video_buffer *buffer);

/**
 * @brief Set video priv data
 *
 * @param video  Video object
 * @param priv   priv data to be set
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_priv_data(struct esp_video *video, void *priv);

/**
 * @brief Put buffer elements into M2M buffer queue list.
 *
 * @param video       Video object
 * @param src_type    Video resource stream type
 * @param src_element Video resource stream buffer element
 * @param dst_type    Video destination stream type
 * @param dst_element Video destination stream buffer element
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_queue_m2m_elements(struct esp_video *video,
                                       uint32_t src_type,
                                       struct esp_video_buffer_element *src_element,
                                       uint32_t dst_type,
                                       struct esp_video_buffer_element *dst_element);

/**
 * @brief Put buffer elements into M2M buffer done list.
 *
 * @param video       Video object
 * @param src_type    Video resource stream type
 * @param src_element Video resource stream buffer element
 * @param dst_type    Video destination stream type
 * @param dst_element Video destination stream buffer element
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_done_m2m_elements(struct esp_video *video,
                                      uint32_t src_type,
                                      struct esp_video_buffer_element *src_element,
                                      uint32_t dst_type,
                                      struct esp_video_buffer_element *dst_element);

/**
 * @brief Get buffer elements from M2M buffer queue list.
 *
 * @param video       Video object
 * @param src_type    Video resource stream type
 * @param src_element Video resource stream buffer element buffer
 * @param dst_type    Video destination stream type
 * @param dst_element Video destination stream buffer element buffer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_m2m_queued_elements(struct esp_video *video,
        uint32_t src_type,
        struct esp_video_buffer_element **src_element,
        uint32_t dst_type,
        struct esp_video_buffer_element **dst_element);

#ifdef __cplusplus
}
#endif
