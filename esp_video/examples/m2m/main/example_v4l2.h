/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_BUF_OWNER_HEAP          0
#define EXAMPLE_BUF_OWNER_CAMERA        1
#define EXAMPLE_BUF_OWNER_M2M_JPEG_DEC  2
#define EXAMPLE_BUF_OWNER_M2M_JPEG_ENC  3
#define EXAMPLE_BUF_OWNER_M2M_H264_ENC  4

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint8_t *data;
    uint32_t size;
    int8_t buf_index;
    uint8_t buf_owner;
} example_image_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t format;
} example_camera_config_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t input_format;
    int quality;
} example_jpeg_config_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t input_format;
    int bitrate;
    int i_period;
    int min_qp;
    int max_qp;
} example_h264_config_t;

typedef struct example_camera_ctx *example_camera_handle_t;
typedef struct example_jpeg_ctx *example_jpeg_decoder_handle_t;
typedef struct example_jpeg_ctx *example_jpeg_encoder_handle_t;
typedef struct example_h264_ctx *example_h264_encoder_handle_t;

void buffer_free(example_image_t *image);

esp_err_t open_camera(const example_camera_config_t *config, example_camera_handle_t *camera);
void close_camera(void);
esp_err_t camera_capture_image(example_image_t *image);

#if CONFIG_EXAMPLE_M2M_MODE_DECODE || CONFIG_EXAMPLE_M2M_MODE_PIPELINE
esp_err_t open_jpeg_decoder(const example_jpeg_config_t *config, example_jpeg_decoder_handle_t *jpeg_decoder);
void close_jpeg_decoder(example_jpeg_decoder_handle_t jpeg_decoder);
esp_err_t jpeg_decode(const example_image_t *input, example_image_t *output);
esp_err_t camera_connect(example_camera_handle_t camera, example_jpeg_decoder_handle_t jpeg_decoder);
#endif

#if CONFIG_EXAMPLE_M2M_MODE_ENCODE && CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE
esp_err_t open_jpeg_encoder(const example_jpeg_config_t *config, example_jpeg_encoder_handle_t *jpeg_encoder);
void close_jpeg_encoder(example_jpeg_encoder_handle_t jpeg_encoder);
esp_err_t jpeg_encode(const example_image_t *input, example_image_t *output);
esp_err_t jpeg_encode_connect(example_camera_handle_t camera, example_jpeg_encoder_handle_t jpeg_encoder);
#endif

#if (CONFIG_EXAMPLE_M2M_MODE_ENCODE && CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE) || CONFIG_EXAMPLE_M2M_MODE_PIPELINE
esp_err_t open_h264_encoder(const example_h264_config_t *config, example_h264_encoder_handle_t *h264_encoder);
void close_h264_encoder(example_h264_encoder_handle_t h264_encoder);
esp_err_t h264_encode(const example_image_t *input, example_image_t *output);
#endif

#if CONFIG_EXAMPLE_M2M_MODE_PIPELINE
esp_err_t jpeg_decode_connect(example_jpeg_decoder_handle_t jpeg_decoder, example_h264_encoder_handle_t h264_encoder);
#endif

#if CONFIG_EXAMPLE_M2M_MODE_ENCODE && CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
esp_err_t h264_encode_connect(example_camera_handle_t camera, example_h264_encoder_handle_t h264_encoder);
#endif

#ifdef __cplusplus
}
#endif
