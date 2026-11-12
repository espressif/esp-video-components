/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAST_STARTUP_CAPTURE_BUFFER_COUNT   2

/**
 * @brief Camera capture device and JPEG M2M encoder used by the fast startup examples.
 */
typedef struct {
    int cap_fd;                                                 /*!< Camera device */
    int m2m_fd;                                                 /*!< JPEG encoder device */
    uint8_t *cap_buffer[FAST_STARTUP_CAPTURE_BUFFER_COUNT];     /*!< Camera buffers */
    uint8_t *m2m_cap_buffer;                                    /*!< JPEG output buffer */
    uint32_t capture_fmt;                                       /*!< Camera pixel format, also the JPEG input format */
    bool camera_streaming;
    bool jpeg_cap_streaming;
    bool jpeg_out_streaming;
} fast_startup_capture_t;

/**
 * @brief Open the camera and the JPEG encoder and set up their buffers.
 *
 * The camera format is the first one the JPEG encoder accepts. Nothing is streamed yet.
 *
 * @param cap              Capture object, zero initialized
 * @param cam_dev          Camera device path
 * @param recv_timeout_ms  DQBUF timeout of both devices
 * @param jpeg_quality     JPEG compression quality
 *
 * @return ESP_OK on success, otherwise an error code
 */
esp_err_t fast_startup_capture_init(fast_startup_capture_t *cap, const char *cam_dev,
                                    uint32_t recv_timeout_ms, int jpeg_quality);

/**
 * @brief Set the camera DQBUF timeout.
 */
esp_err_t fast_startup_capture_set_timeout(fast_startup_capture_t *cap, uint32_t timeout_ms);

/**
 * @brief Stream on the camera, and the JPEG encoder first when jpeg is true.
 *
 * Call esp_video_isp_pipeline_set_ipa_state() before this, so the first frame uses that state.
 */
esp_err_t fast_startup_capture_start(fast_startup_capture_t *cap, bool jpeg);

/**
 * @brief Stream off whatever fast_startup_capture_start() turned on.
 */
void fast_startup_capture_stop(fast_startup_capture_t *cap);

/**
 * @brief Dequeue one camera frame and queue it back without using it.
 */
esp_err_t fast_startup_capture_skip_frame(fast_startup_capture_t *cap);

/**
 * @brief Dequeue one camera frame, encode it to JPEG and write it to path.
 *
 * A partly written file is removed.
 */
esp_err_t fast_startup_capture_save_jpeg(fast_startup_capture_t *cap, const char *path);

#ifdef __cplusplus
}
#endif
