/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * Camera-side controls reached over the generic unit-control API in usb_host_uvc.
 *
 * Two layers live here:
 *   - The H.264 Extension Unit of the UVC H.264 payload specification, which is where the
 *     camera's own encoder is configured.
 */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "usb/uvc_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Rate-control mode, matching bRateControlMode of the H.264 payload specification
 */
typedef enum {
    ESP_VIDEO_UVC_H264_RC_KEEP      = 0,    /*!< Leave whatever the camera is already doing */
    ESP_VIDEO_UVC_H264_RC_CBR       = 1,    /*!< Constant bitrate */
    ESP_VIDEO_UVC_H264_RC_VBR       = 2,    /*!< Variable bitrate */
    ESP_VIDEO_UVC_H264_RC_CONST_QP  = 3,    /*!< Constant QP; the bitrate request is ignored */
} esp_video_uvc_h264_rc_mode_t;

/**
 * @brief What the camera's H.264 Extension Unit can actually do
 *
 * Filled once per stream by esp_video_uvc_h264_xu_probe(). Every field is a fact read out of
 * the camera - descriptor bits or GET_MIN/GET_MAX - rather than an assumption, because the
 * range a camera accepts is the range a V4L2 application has to be told about.
 */
typedef struct {
    uint8_t  unit_id;               /*!< bUnitID of the H.264 XU, 0 when the camera has none */
    uint16_t version;               /*!< UVCX_VERSION, 0 when unreadable */

    bool     can_set_bitrate_live;  /*!< UVCX_BITRATE_LAYERS: bitrate changes without a restart */
    bool     can_set_rc_mode_live;  /*!< UVCX_RATE_CONTROL_MODE */
    bool     can_set_qp_live;       /*!< UVCX_QP_STEPS_LAYERS */
    bool     can_set_picture_type;  /*!< UVCX_PICTURE_TYPE_CONTROL: on-demand IDR */
    bool     can_reset_encoder;     /*!< UVCX_ENCODER_RESET */

    uint32_t bitrate_min;           /*!< dwBitRate from PROBE GET_MIN */
    uint32_t bitrate_max;           /*!< dwBitRate from PROBE GET_MAX */
    uint32_t bitrate_def;           /*!< dwBitRate from PROBE GET_DEF */
    bool     bitrate_range_known;   /*!< false when the camera stalled GET_MIN/GET_MAX */
} esp_video_uvc_h264_xu_t;

/**
 * @brief Encoder configuration asserted over the H.264 Extension Unit
 *
 * A zero field means "leave the camera's own value alone" throughout, so a caller only has to
 * fill in what it actually wants to control.
 */
typedef struct {
    uint32_t bitrate_bps;           /*!< Target bitrate in bps, absolute. 0 leaves it alone */
    uint32_t peak_bitrate_bps;      /*!< Peak bitrate in bps. 0 means "same as the target" */
    uint16_t iframe_period_ms;      /*!< Key-frame period in ms. 0 leaves it alone */
    uint32_t frame_interval_100ns;  /*!< Frame interval to assert, in 100 ns units. 0 leaves it alone */
    bool     fixed_frame_rate;      /*!< true forbids frame dropping, so quality absorbs a bitrate shortfall */
    esp_video_uvc_h264_rc_mode_t rc_mode;   /*!< Rate-control mode to assert */
} esp_video_uvc_h264_config_t;

/**
 * @brief What the camera committed
 */
typedef struct {
    uint32_t bitrate_bps;           /*!< dwBitRate after COMMIT */
    uint16_t iframe_period_ms;      /*!< wIFramePeriod after COMMIT */
    uint32_t frame_interval_100ns;  /*!< dwFrameInterval after COMMIT */
    bool     fixed_frame_rate;      /*!< Whether the camera kept the fixed-frame-rate flag */
    esp_video_uvc_h264_rc_mode_t rc_mode;   /*!< Rate-control mode after COMMIT */
} esp_video_uvc_h264_committed_t;

/**
 * @brief Discover the camera's H.264 Extension Unit and what it supports
 *
 * Reads the descriptor's bmControls for the optional selectors and the PROBE control's
 * GET_MIN/GET_MAX/GET_DEF for the bitrate range. Issues no SET of any kind, so it is safe to
 * call on a stream of any pixel format.
 *
 * @param[in]  stream_hdl Open UVC stream
 * @param[out] xu         Cleared and filled in. On ESP_ERR_NOT_FOUND it is left all-zero,
 *                        which every other function here reads as "no extension unit"
 * @return
 *     - ESP_OK: the camera has an H.264 extension unit
 *     - ESP_ERR_NOT_FOUND: it does not
 *     - ESP_ERR_INVALID_ARG: bad arguments
 */
esp_err_t esp_video_uvc_h264_xu_probe(uvc_host_stream_hdl_t stream_hdl, esp_video_uvc_h264_xu_t *xu);

/**
 * @brief Configure the camera's encoder through the PROBE/COMMIT handshake
 *
 * This is the pre-stream negotiation of the H.264 payload specification and belongs between
 * uvc_host_stream_format_select() and uvc_host_stream_start(). For changes while the stream is
 * running, use the dedicated runtime controls below - many cameras accept a mid-stream
 * PROBE/COMMIT and then quietly keep what they committed at stream start.
 *
 * The resolution and frame interval asserted are the ones the VideoStreaming interface already
 * negotiated; the payload specification requires the two to agree. Alongside the caller's
 * request the transaction also pins the stream shape a passthrough path depends on: Annex B
 * byte stream, not muxed, no frame reordering, realtime usage.
 *
 * @param[in]  stream_hdl Open UVC stream, already committed to a format
 * @param[in]  xu         From esp_video_uvc_h264_xu_probe()
 * @param[in]  cfg        What to assert; zero fields are left at the camera's value
 * @param[out] committed  If non-NULL, what the camera agreed to. A report, not a guarantee
 * @return
 *     - ESP_OK: the camera committed the configuration
 *     - ESP_ERR_NOT_SUPPORTED: no H.264 extension unit on this camera
 *     - Others: the camera rejected the transaction
 */
esp_err_t esp_video_uvc_h264_xu_configure(uvc_host_stream_hdl_t stream_hdl,
                                          const esp_video_uvc_h264_xu_t *xu,
                                          const esp_video_uvc_h264_config_t *cfg,
                                          esp_video_uvc_h264_committed_t *committed);

/**
 * @brief Change the bitrate while the stream is running
 *
 * UVCX_BITRATE_LAYERS, the control the payload specification provides for exactly this. Unlike
 * a mid-stream PROBE/COMMIT it is defined to take effect immediately.
 *
 * @param[in]  stream_hdl    Open, running UVC stream
 * @param[in]  xu            From esp_video_uvc_h264_xu_probe()
 * @param[in]  bitrate_bps   Target (average) bitrate in bps
 * @param[in]  peak_bps      Peak bitrate in bps, or 0 to use the target
 * @param[out] committed_bps If non-NULL, the average bitrate read back afterwards
 * @return
 *     - ESP_OK: written and read back
 *     - ESP_ERR_NOT_SUPPORTED: the camera does not implement UVCX_BITRATE_LAYERS
 *     - Others: the camera rejected the request
 */
esp_err_t esp_video_uvc_h264_xu_set_bitrate(uvc_host_stream_hdl_t stream_hdl,
                                            const esp_video_uvc_h264_xu_t *xu,
                                            uint32_t bitrate_bps, uint32_t peak_bps,
                                            uint32_t *committed_bps);

/**
 * @brief Change the rate-control mode while the stream is running
 *
 * UVCX_RATE_CONTROL_MODE. The mode and the fixed-frame-rate flag travel together in one byte,
 * so both are written at once.
 *
 * @param[in] stream_hdl       Open, running UVC stream
 * @param[in] xu               From esp_video_uvc_h264_xu_probe()
 * @param[in] mode             Mode to assert; ESP_VIDEO_UVC_H264_RC_KEEP keeps the camera's
 * @param[in] fixed_frame_rate true forbids frame dropping to stay inside the bitrate
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_NOT_SUPPORTED: the camera does not implement UVCX_RATE_CONTROL_MODE
 *     - Others: the camera rejected the request
 */
esp_err_t esp_video_uvc_h264_xu_set_rc_mode(uvc_host_stream_hdl_t stream_hdl,
                                            const esp_video_uvc_h264_xu_t *xu,
                                            esp_video_uvc_h264_rc_mode_t mode, bool fixed_frame_rate);

/**
 * @brief Set the QP floor and ceiling while the stream is running
 *
 * UVCX_QP_STEPS_LAYERS. Without it, "quality absorbs the bitrate shortfall" is unbounded.
 *
 * @param[in] stream_hdl Open, running UVC stream
 * @param[in] xu         From esp_video_uvc_h264_xu_probe()
 * @param[in] min_qp     Lowest QP the encoder may use
 * @param[in] max_qp     Highest QP the encoder may use
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_NOT_SUPPORTED: the camera does not implement UVCX_QP_STEPS_LAYERS
 *     - Others: the camera rejected the request
 */
esp_err_t esp_video_uvc_h264_xu_set_qp(uvc_host_stream_hdl_t stream_hdl,
                                       const esp_video_uvc_h264_xu_t *xu,
                                       uint8_t min_qp, uint8_t max_qp);

/**
 * @brief Ask the camera for an IDR carrying fresh parameter sets
 *
 * UVCX_PICTURE_TYPE_CONTROL. This is the extension unit's own key-frame request, and the
 * fallback for a camera that implements the H.264 XU but not the VideoStreaming Generate Key
 * Frame control that uvc_host_stream_request_key_frame() uses.
 *
 * @param[in] stream_hdl Open, running UVC stream
 * @param[in] xu         From esp_video_uvc_h264_xu_probe()
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_NOT_SUPPORTED: the camera does not implement UVCX_PICTURE_TYPE_CONTROL
 *     - Others: the camera rejected the request
 */
esp_err_t esp_video_uvc_h264_xu_request_idr(uvc_host_stream_hdl_t stream_hdl,
                                            const esp_video_uvc_h264_xu_t *xu);

#ifdef __cplusplus
}
#endif
