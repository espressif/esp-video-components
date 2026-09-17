/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The camera's own H.264 encoder, reached through the UVC H.264 payload specification's
 * Extension Unit.
 *
 * The one piece of policy is the split between PROBE/COMMIT and the runtime controls.
 * PROBE/COMMIT is the payload specification's pre-stream negotiation, and a camera may
 * accept a mid-stream PROBE/COMMIT and then keep what it committed at stream start.
 * UVCX_BITRATE_LAYERS, UVCX_RATE_CONTROL_MODE and UVCX_QP_STEPS_LAYERS exist precisely
 * so a rate controller does not have to restart the stream to be heard.
 */

#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_check.h"

#include "usb/uvc_host.h"
#include "esp_video_uvc_controls.h"

static const char *TAG = "uvc_h264_xu";

/* UVC H.264 Extension Unit control selectors.
 *
 * A unit declares which of these it implements in the bmControls of its descriptor, one bit
 * per selector starting at D0 for selector 1 - hence XU_CONTROL_BIT() below. Asking the
 * descriptor is better than probing: a camera that does not implement a selector answers a
 * request with a STALL, which the USB host library logs at ERROR on every boot. */
#define UVCX_VIDEO_CONFIG_PROBE     0x01
#define UVCX_VIDEO_CONFIG_COMMIT    0x02
#define UVCX_RATE_CONTROL_MODE      0x03
#define UVCX_TEMPORAL_SCALE_MODE    0x04
#define UVCX_SPATIAL_SCALE_MODE     0x05
#define UVCX_SNR_SCALE_MODE         0x06
#define UVCX_LTR_BUFFER_SIZE        0x07
#define UVCX_LTR_PICTURE            0x08
#define UVCX_PICTURE_TYPE_CONTROL   0x09
#define UVCX_VERSION                0x0a
#define UVCX_ENCODER_RESET          0x0b
#define UVCX_FRAMERATE_CONFIG       0x0c
#define UVCX_VIDEO_ADVANCE_CONFIG   0x0d
#define UVCX_BITRATE_LAYERS         0x0e
#define UVCX_QP_STEPS_LAYERS        0x0f

#define XU_CONTROL_BIT(_selector)   ((uint8_t)((_selector) - 1))

/* bmHints tells the camera which fields the host is asserting and it must therefore keep
 * fixed; every field not hinted is one it is free to trade away to meet the rest.
 *
 * These values are not sequential bit positions - they are the specific assignments the USB
 * Video Class H.264 payload spec makes - and getting one wrong is silent, because a wrong bit
 * is still a valid hint for some other field. */
#define UVCX_HINT_RESOLUTION      0x0001
#define UVCX_HINT_PROFILE         0x0002
#define UVCX_HINT_RATECONTROL     0x0004
#define UVCX_HINT_USAGE           0x0008
#define UVCX_HINT_SLICEMODE       0x0010
#define UVCX_HINT_SLICEUNITS      0x0020
#define UVCX_HINT_MVCVIEW         0x0040
#define UVCX_HINT_TEMPORAL        0x0080
#define UVCX_HINT_SNR             0x0100
#define UVCX_HINT_SPATIAL         0x0200
#define UVCX_HINT_SPATIAL_RATIO   0x0400
#define UVCX_HINT_FRAME_INTERVAL  0x0800
#define UVCX_HINT_LEAKY_BKT_SIZE  0x1000
#define UVCX_HINT_BITRATE         0x2000
#define UVCX_HINT_ENTROPY         0x4000
#define UVCX_HINT_IFRAME_PERIOD   0x8000

/* bRateControlMode carries a mode in the low nibble plus a flag: FIXED_FRAME_RATE means "do
 * not drop frames to stay inside the bitrate". That is the second, independent half of pinning
 * the frame rate - hinting dwFrameInterval says "do not change this number", this says "do not
 * miss it" - and a camera may honour either one alone. */
#define UVCX_RC_MODE_MASK         0x0f
#define UVCX_RC_FIXED_FRAME_RATE  0x10

/* bUsageType. Realtime is the low-latency reference pattern: no B frames, no long-term
 * reference tricks, every frame decodable as it arrives. */
#define UVCX_USAGE_REALTIME       0x01

/* bStreamFormat: the payload has to arrive as something a passthrough consumer can parse.
 * Annex B start codes, not NAL length prefixes. */
#define UVCX_STREAM_FORMAT_ANNEXB 0x00

/* bStreamMuxOption: bit 0 enables muxing at all, so zero means "H.264 elementary stream only".
 * A camera left to itself may hand back an MJPEG+H.264 mux the consumer never asked for. */
#define UVCX_STREAM_MUX_NONE      0x00

/* wLayerID for the base layer. The runtime controls are all per-layer, and a stream that uses
 * no temporal, spatial or SNR scalability has exactly one. */
#define UVCX_LAYER_BASE           0x0000

/* wPicType of UVCX_PICTURE_TYPE_CONTROL. An IDR alone resets the reference chain; an IDR that
 * repeats the parameter sets is what lets a viewer joining mid-stream start decoding. */
#define UVCX_PICTYPE_IDR_WITH_SPS_PPS 0x0002

/* dwFrameInterval is in 100 ns units, so this is the numerator of "interval for N fps". */
#define UVCX_INTERVAL_UNITS_PER_S 10000000.0f

typedef struct __attribute__((packed)) {
    uint32_t dwFrameInterval;
    uint32_t dwBitRate;
    uint16_t bmHints;
    uint16_t wConfigurationIndex;
    uint16_t wWidth;
    uint16_t wHeight;
    uint16_t wSliceUnits;
    uint16_t wSliceMode;
    uint16_t wProfile;
    uint16_t wIFramePeriod;
    uint16_t wEstimatedVideoDelay;
    uint16_t wEstimatedMaxConfigDelay;
    uint8_t  bUsageType;
    uint8_t  bRateControlMode;
    uint8_t  bTemporalScaleMode;
    uint8_t  bSpatialScaleMode;
    uint8_t  bSNRScaleMode;
    uint8_t  bStreamMuxOption;
    uint8_t  bStreamFormat;
    uint8_t  bEntropyCABAC;
    uint8_t  bTimestamp;
    uint8_t  bNumOfReorderFrames;
    uint8_t  bPreviewFlipped;
    uint8_t  bView;
    uint8_t  bReserved1;
    uint8_t  bReserved2;
    uint8_t  bStreamID;
    uint8_t  bSpatialLayerRatio;
    uint16_t wLeakyBucketSize;
} uvcx_video_config_t;

_Static_assert(sizeof(uvcx_video_config_t) == 46, "UVCX_VIDEO_CONFIG must be 46 bytes");

typedef struct __attribute__((packed)) {
    uint16_t wLayerID;
    uint8_t  bRateControlMode;
} uvcx_rate_control_mode_t;

typedef struct __attribute__((packed)) {
    uint16_t wLayerID;
    uint32_t dwPeakBitrate;
    uint32_t dwAverageBitrate;
} uvcx_bitrate_layers_t;

typedef struct __attribute__((packed)) {
    uint16_t wLayerID;
    uint8_t  bFrameType;
    uint8_t  bMinQp;
    uint8_t  bMaxQp;
} uvcx_qp_steps_layers_t;

typedef struct __attribute__((packed)) {
    uint16_t wLayerID;
    uint16_t wPicType;
} uvcx_picture_type_t;

/* GUID of the H.264 extension unit, little-endian as it appears in the descriptor.
 * A29E7641-DE04-47E3-8B2B-F4341AFF003B */
static const uint8_t uvcx_h264_guid[16] = {
    0x41, 0x76, 0x9e, 0xa2, 0x04, 0xde, 0xe3, 0x47,
    0x8b, 0x2b, 0xf4, 0x34, 0x1a, 0xff, 0x00, 0x3b
};

/* Reading the descriptor rather than probing the camera, so an unsupported selector costs
 * nothing and logs nothing. A descriptor that does not declare bmControls at all is read as
 * "does not implement it", which is the safe direction: the caller falls back. */
static bool xu_declares(uvc_host_stream_hdl_t stream_hdl, uint8_t unit_id, uint8_t selector)
{
    bool supported = false;

    if (uvc_host_stream_unit_supports_control(stream_hdl, unit_id, XU_CONTROL_BIT(selector),
                                              &supported) != ESP_OK) {
        return false;
    }
    return supported;
}

/* GET_MIN/GET_MAX/GET_DEF on the PROBE control. The point is to learn the camera's real
 * bitrate range so VIDIOC_QUERY_EXT_CTRL can report it, instead of asking for a value, being
 * clamped, and discovering the range from the warning afterwards. */
static void xu_read_bitrate_range(uvc_host_stream_hdl_t stream_hdl, esp_video_uvc_h264_xu_t *xu)
{
    uvcx_video_config_t cfg;
    esp_err_t ret;

    ret = uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_VIDEO_CONFIG_PROBE,
                                    UVC_HOST_REQ_GET_MIN, &cfg, sizeof(cfg));
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "PROBE GET_MIN unavailable (%s), bitrate range unknown", esp_err_to_name(ret));
        return;
    }
    xu->bitrate_min = cfg.dwBitRate;

    ret = uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_VIDEO_CONFIG_PROBE,
                                    UVC_HOST_REQ_GET_MAX, &cfg, sizeof(cfg));
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "PROBE GET_MAX unavailable (%s), bitrate range unknown", esp_err_to_name(ret));
        xu->bitrate_min = 0;
        return;
    }
    xu->bitrate_max = cfg.dwBitRate;

    if (uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_VIDEO_CONFIG_PROBE,
                                  UVC_HOST_REQ_GET_DEF, &cfg, sizeof(cfg)) == ESP_OK) {
        xu->bitrate_def = cfg.dwBitRate;
    }

    /* A camera that answers the requests but reports an empty or inverted range has told us
     * nothing usable, and publishing it as a V4L2 min/max would make every SET fail. */
    if (xu->bitrate_min >= xu->bitrate_max) {
        ESP_LOGD(TAG, "camera reported bitrate range %" PRIu32 "..%" PRIu32 ", ignoring it",
                 xu->bitrate_min, xu->bitrate_max);
        xu->bitrate_min = 0;
        xu->bitrate_max = 0;
        xu->bitrate_def = 0;
        return;
    }

    if (xu->bitrate_def < xu->bitrate_min || xu->bitrate_def > xu->bitrate_max) {
        xu->bitrate_def = xu->bitrate_min;
    }
    xu->bitrate_range_known = true;
}

esp_err_t esp_video_uvc_h264_xu_probe(uvc_host_stream_hdl_t stream_hdl, esp_video_uvc_h264_xu_t *xu)
{
    ESP_RETURN_ON_FALSE(stream_hdl && xu, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    uint8_t unit_id = 0;

    memset(xu, 0, sizeof(*xu));

    esp_err_t ret = uvc_host_stream_find_extension_unit(stream_hdl, uvcx_h264_guid, &unit_id);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "no H.264 extension unit on this camera");
        return ESP_ERR_NOT_FOUND;
    }
    xu->unit_id = unit_id;

    uint16_t version = 0;
    if (uvc_host_stream_unit_ctrl(stream_hdl, unit_id, UVCX_VERSION, UVC_HOST_REQ_GET_CUR,
                                  &version, sizeof(version)) == ESP_OK) {
        xu->version = version;
    }

    xu->can_set_bitrate_live = xu_declares(stream_hdl, unit_id, UVCX_BITRATE_LAYERS);
    xu->can_set_rc_mode_live = xu_declares(stream_hdl, unit_id, UVCX_RATE_CONTROL_MODE);
    xu->can_set_qp_live      = xu_declares(stream_hdl, unit_id, UVCX_QP_STEPS_LAYERS);
    xu->can_set_picture_type = xu_declares(stream_hdl, unit_id, UVCX_PICTURE_TYPE_CONTROL);
    xu->can_reset_encoder    = xu_declares(stream_hdl, unit_id, UVCX_ENCODER_RESET);

    xu_read_bitrate_range(stream_hdl, xu);

    ESP_LOGI(TAG, "H.264 XU: bUnitID %u, version 0x%04x, runtime controls:%s%s%s%s",
             unit_id, xu->version,
             xu->can_set_bitrate_live ? " bitrate" : "",
             xu->can_set_rc_mode_live ? " rc-mode" : "",
             xu->can_set_qp_live      ? " qp"      : "",
             xu->can_set_picture_type ? " idr"     : "");
    if (xu->bitrate_range_known) {
        ESP_LOGI(TAG, "H.264 XU bitrate range: %" PRIu32 "..%" PRIu32 " bps, default %" PRIu32,
                 xu->bitrate_min, xu->bitrate_max, xu->bitrate_def);
    }
    if (!xu->can_set_bitrate_live) {
        ESP_LOGI(TAG, "camera has no UVCX_BITRATE_LAYERS - the bitrate can only be set at "
                 "stream start");
    }

    return ESP_OK;
}

esp_err_t esp_video_uvc_h264_xu_configure(uvc_host_stream_hdl_t stream_hdl,
                                          const esp_video_uvc_h264_xu_t *xu,
                                          const esp_video_uvc_h264_config_t *cfg_in,
                                          esp_video_uvc_h264_committed_t *committed)
{
    ESP_RETURN_ON_FALSE(stream_hdl && xu && cfg_in, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(xu->unit_id, ESP_ERR_NOT_SUPPORTED, TAG, "no H.264 extension unit");

    uvcx_video_config_t cfg = {0};
    uint32_t want_interval = cfg_in->frame_interval_100ns;

    /* Read what the camera is doing now, change only what we need */
    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_VIDEO_CONFIG_PROBE,
                                                  UVC_HOST_REQ_GET_CUR, &cfg, sizeof(cfg)),
                        TAG, "H.264 XU not readable (camera may not implement it)");

    ESP_LOGD(TAG, "camera H.264 config: %ux%u, %" PRIu32 " bps, IFramePeriod %u ms, "
             "interval %" PRIu32 ", rc_mode 0x%02x, hints 0x%04x",
             cfg.wWidth, cfg.wHeight, cfg.dwBitRate, cfg.wIFramePeriod,
             cfg.dwFrameInterval, cfg.bRateControlMode, cfg.bmHints);

    cfg.bmHints = 0;

    /* The H.264 config carries its own resolution and frame interval, and per the payload spec
     * both must match what the VideoStreaming interface committed to. A camera that moved the
     * encoder off the negotiated geometry to meet some other constraint would produce payloads
     * the stream cannot carry. */
    uvc_host_stream_format_t stream_fmt = {0};
    if (uvc_host_stream_format_get(stream_hdl, &stream_fmt) == ESP_OK &&
            stream_fmt.h_res && stream_fmt.v_res) {
        cfg.wWidth  = stream_fmt.h_res;
        cfg.wHeight = stream_fmt.v_res;
        cfg.bmHints |= UVCX_HINT_RESOLUTION;
    }
    if (stream_fmt.fps > 0.0f) {
        const uint32_t negotiated = (uint32_t)(UVCX_INTERVAL_UNITS_PER_S / stream_fmt.fps + 0.5f);

        if (negotiated) {
            if (want_interval && want_interval != negotiated) {
                ESP_LOGW(TAG, "requested frame interval %" PRIu32 " does not match the "
                         "negotiated %" PRIu32 " (%.0f fps); the payload spec requires them to "
                         "agree, so the negotiated one is used. Change the frame rate with "
                         "VIDIOC_S_PARM before starting the stream.",
                         want_interval, negotiated, stream_fmt.fps);
            }
            want_interval = negotiated;
        }
    }
    if (want_interval) {
        cfg.dwFrameInterval = want_interval;
        cfg.bmHints |= UVCX_HINT_FRAME_INTERVAL;
    }

    if (cfg_in->bitrate_bps) {
        cfg.dwBitRate = cfg_in->bitrate_bps;
        cfg.bmHints |= UVCX_HINT_BITRATE;
    }
    if (cfg_in->iframe_period_ms) {
        cfg.wIFramePeriod = cfg_in->iframe_period_ms;
        cfg.bmHints |= UVCX_HINT_IFRAME_PERIOD;
    }

    /* The rate-control byte is only worth asserting once a mode has actually been chosen.
     * Hinting it while leaving the low nibble at whatever GET_CUR returned pins the camera to
     * that mode, and if that mode happens to be constant-QP the bitrate hint is inert - the
     * commit succeeds, the read-back matches, and the ceiling is simply not in effect. */
    if (cfg_in->rc_mode != ESP_VIDEO_UVC_H264_RC_KEEP) {
        cfg.bRateControlMode = (uint8_t)(cfg_in->rc_mode & UVCX_RC_MODE_MASK);
        if (cfg_in->fixed_frame_rate) {
            cfg.bRateControlMode |= UVCX_RC_FIXED_FRAME_RATE;
        }
        cfg.bmHints |= UVCX_HINT_RATECONTROL;
    } else if (cfg_in->fixed_frame_rate != (bool)(cfg.bRateControlMode & UVCX_RC_FIXED_FRAME_RATE)) {
        /* No mode preference, but the frame-dropping policy still has to change. Keep the
         * camera's mode and move only the flag. */
        if (cfg_in->fixed_frame_rate) {
            cfg.bRateControlMode |= UVCX_RC_FIXED_FRAME_RATE;
        } else {
            cfg.bRateControlMode &= (uint8_t)~UVCX_RC_FIXED_FRAME_RATE;
        }
        cfg.bmHints |= UVCX_HINT_RATECONTROL;
    }

    /* The shape of the stream a passthrough consumer depends on. None of this is a tuning
     * knob: an application that hands the payload to a depayloader cannot parse a NAL-length
     * prefixed or muxed stream, and reorder frames add latency to a live path for no benefit
     * the consumer can use. Asserting them costs one field each and removes a class of
     * camera-dependent breakage. */
    cfg.bStreamFormat        = UVCX_STREAM_FORMAT_ANNEXB;
    cfg.bStreamMuxOption     = UVCX_STREAM_MUX_NONE;
    cfg.bNumOfReorderFrames  = 0;
    cfg.bUsageType           = UVCX_USAGE_REALTIME;
    cfg.bmHints             |= UVCX_HINT_USAGE;

    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_VIDEO_CONFIG_PROBE,
                                                  UVC_HOST_REQ_SET_CUR, &cfg, sizeof(cfg)),
                        TAG, "H.264 XU probe rejected");
    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_VIDEO_CONFIG_PROBE,
                                                  UVC_HOST_REQ_GET_CUR, &cfg, sizeof(cfg)),
                        TAG, "H.264 XU probe read-back failed");
    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_VIDEO_CONFIG_COMMIT,
                                                  UVC_HOST_REQ_SET_CUR, &cfg, sizeof(cfg)),
                        TAG, "H.264 XU commit rejected");

    ESP_LOGI(TAG, "camera H.264 committed: %ux%u, %" PRIu32 " bps, IFramePeriod %u ms, "
             "interval %" PRIu32 ", rc_mode 0x%02x",
             cfg.wWidth, cfg.wHeight, cfg.dwBitRate, cfg.wIFramePeriod, cfg.dwFrameInterval,
             cfg.bRateControlMode);

    /* The frame interval belongs in this check as much as the bitrate does: a camera that takes
     * the bitrate and quietly returns a different interval has accepted the budget and refused
     * the terms, which reads as a frame-rate fault a long way from here. */
    if ((cfg_in->bitrate_bps && cfg.dwBitRate != cfg_in->bitrate_bps) ||
            (cfg_in->iframe_period_ms && cfg.wIFramePeriod != cfg_in->iframe_period_ms) ||
            (want_interval && cfg.dwFrameInterval != want_interval)) {
        ESP_LOGW(TAG, "camera kept its own values (asked %" PRIu32 " bps / %u ms / interval "
                 "%" PRIu32 ") - it clamped or rejected the request",
                 cfg_in->bitrate_bps, cfg_in->iframe_period_ms, want_interval);
    }
    if (cfg_in->fixed_frame_rate && !(cfg.bRateControlMode & UVCX_RC_FIXED_FRAME_RATE)) {
        ESP_LOGW(TAG, "camera cleared the fixed-frame-rate flag - it may still drop frames to "
                 "stay inside %" PRIu32 " bps", cfg.dwBitRate);
    }
    if (cfg.bStreamFormat != UVCX_STREAM_FORMAT_ANNEXB || cfg.bStreamMuxOption != UVCX_STREAM_MUX_NONE) {
        ESP_LOGW(TAG, "camera kept bStreamFormat %u / bStreamMuxOption 0x%02x - the payload is "
                 "not a plain Annex B elementary stream and a passthrough consumer will need to "
                 "handle that", cfg.bStreamFormat, cfg.bStreamMuxOption);
    }

    if (committed != NULL) {
        committed->bitrate_bps          = cfg.dwBitRate;
        committed->iframe_period_ms     = cfg.wIFramePeriod;
        committed->frame_interval_100ns = cfg.dwFrameInterval;
        committed->fixed_frame_rate     = (cfg.bRateControlMode & UVCX_RC_FIXED_FRAME_RATE) != 0;
        committed->rc_mode              = (esp_video_uvc_h264_rc_mode_t)(cfg.bRateControlMode & UVCX_RC_MODE_MASK);
    }
    return ESP_OK;
}

esp_err_t esp_video_uvc_h264_xu_set_bitrate(uvc_host_stream_hdl_t stream_hdl,
                                            const esp_video_uvc_h264_xu_t *xu,
                                            uint32_t bitrate_bps, uint32_t peak_bps,
                                            uint32_t *committed_bps)
{
    ESP_RETURN_ON_FALSE(stream_hdl && xu, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(bitrate_bps, ESP_ERR_INVALID_ARG, TAG, "bitrate must be non-zero");
    ESP_RETURN_ON_FALSE(xu->unit_id && xu->can_set_bitrate_live, ESP_ERR_NOT_SUPPORTED, TAG,
                        "camera has no UVCX_BITRATE_LAYERS");

    uvcx_bitrate_layers_t layers = {
        .wLayerID         = UVCX_LAYER_BASE,
        .dwAverageBitrate = bitrate_bps,
        .dwPeakBitrate    = peak_bps ? peak_bps : bitrate_bps,
    };

    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_BITRATE_LAYERS,
                                                  UVC_HOST_REQ_SET_CUR, &layers, sizeof(layers)),
                        TAG, "UVCX_BITRATE_LAYERS write rejected");

    /* Read back rather than trust the write: the clamp is the camera's to apply, and a rate
     * controller must not go on believing it is sending what it asked for. */
    memset(&layers, 0, sizeof(layers));
    layers.wLayerID = UVCX_LAYER_BASE;
    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_BITRATE_LAYERS,
                                                  UVC_HOST_REQ_GET_CUR, &layers, sizeof(layers)),
                        TAG, "UVCX_BITRATE_LAYERS read-back failed");

    if (layers.dwAverageBitrate != bitrate_bps) {
        ESP_LOGW(TAG, "camera clamped the bitrate: asked %" PRIu32 ", committed %" PRIu32,
                 bitrate_bps, layers.dwAverageBitrate);
    } else {
        ESP_LOGD(TAG, "bitrate now %" PRIu32 " bps (peak %" PRIu32 ")",
                 layers.dwAverageBitrate, layers.dwPeakBitrate);
    }
    if (committed_bps != NULL) {
        *committed_bps = layers.dwAverageBitrate;
    }
    return ESP_OK;
}

esp_err_t esp_video_uvc_h264_xu_set_rc_mode(uvc_host_stream_hdl_t stream_hdl,
                                            const esp_video_uvc_h264_xu_t *xu,
                                            esp_video_uvc_h264_rc_mode_t mode, bool fixed_frame_rate)
{
    ESP_RETURN_ON_FALSE(stream_hdl && xu, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(xu->unit_id && xu->can_set_rc_mode_live, ESP_ERR_NOT_SUPPORTED, TAG,
                        "camera has no UVCX_RATE_CONTROL_MODE");

    uvcx_rate_control_mode_t rc = { .wLayerID = UVCX_LAYER_BASE };

    if (mode == ESP_VIDEO_UVC_H264_RC_KEEP) {
        /* Only the flag is moving, so the mode has to come from the camera rather than from a
         * guess - writing a zero nibble would assert an undefined mode. */
        ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_RATE_CONTROL_MODE,
                                                      UVC_HOST_REQ_GET_CUR, &rc, sizeof(rc)),
                            TAG, "UVCX_RATE_CONTROL_MODE read failed");
        rc.wLayerID = UVCX_LAYER_BASE;
        rc.bRateControlMode &= UVCX_RC_MODE_MASK;
    } else {
        rc.bRateControlMode = (uint8_t)(mode & UVCX_RC_MODE_MASK);
    }
    if (fixed_frame_rate) {
        rc.bRateControlMode |= UVCX_RC_FIXED_FRAME_RATE;
    }

    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_RATE_CONTROL_MODE,
                                                  UVC_HOST_REQ_SET_CUR, &rc, sizeof(rc)),
                        TAG, "UVCX_RATE_CONTROL_MODE write rejected");

    ESP_LOGD(TAG, "rate control mode now 0x%02x", rc.bRateControlMode);
    return ESP_OK;
}

esp_err_t esp_video_uvc_h264_xu_set_qp(uvc_host_stream_hdl_t stream_hdl,
                                       const esp_video_uvc_h264_xu_t *xu,
                                       uint8_t min_qp, uint8_t max_qp)
{
    ESP_RETURN_ON_FALSE(stream_hdl && xu, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(min_qp <= max_qp, ESP_ERR_INVALID_ARG, TAG, "min QP exceeds max QP");
    ESP_RETURN_ON_FALSE(xu->unit_id && xu->can_set_qp_live, ESP_ERR_NOT_SUPPORTED, TAG,
                        "camera has no UVCX_QP_STEPS_LAYERS");

    /* bFrameType 0 applies the limits to every frame type, which is what a single pair of V4L2
     * min/max QP controls can express. */
    uvcx_qp_steps_layers_t qp = {
        .wLayerID   = UVCX_LAYER_BASE,
        .bFrameType = 0,
        .bMinQp     = min_qp,
        .bMaxQp     = max_qp,
    };

    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_QP_STEPS_LAYERS,
                                                  UVC_HOST_REQ_SET_CUR, &qp, sizeof(qp)),
                        TAG, "UVCX_QP_STEPS_LAYERS write rejected");

    ESP_LOGD(TAG, "QP range now %u..%u", min_qp, max_qp);
    return ESP_OK;
}

esp_err_t esp_video_uvc_h264_xu_request_idr(uvc_host_stream_hdl_t stream_hdl,
                                            const esp_video_uvc_h264_xu_t *xu)
{
    ESP_RETURN_ON_FALSE(stream_hdl && xu, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(xu->unit_id && xu->can_set_picture_type, ESP_ERR_NOT_SUPPORTED, TAG,
                        "camera has no UVCX_PICTURE_TYPE_CONTROL");

    uvcx_picture_type_t pic = {
        .wLayerID = UVCX_LAYER_BASE,
        .wPicType = UVCX_PICTYPE_IDR_WITH_SPS_PPS,
    };

    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, xu->unit_id, UVCX_PICTURE_TYPE_CONTROL,
                                                  UVC_HOST_REQ_SET_CUR, &pic, sizeof(pic)),
                        TAG, "UVCX_PICTURE_TYPE_CONTROL write rejected");
    return ESP_OK;
}
