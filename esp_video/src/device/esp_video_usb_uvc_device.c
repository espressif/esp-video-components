/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

// #undef LOG_LOCAL_LEVEL
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "usb/usb_host.h"
#include "usb/uvc_host.h"
#include "esp_private/uvc_esp_video.h"

#include "esp_video.h"
#include "esp_video_device.h"
#include "esp_video_device_internal.h"
#include "esp_video_ioctl.h"
#include "esp_video_device_common.h"
#include "esp_video_uvc_controls.h"

#define UVC_NAME_PREFIX    "USB-UVC"

#if CONFIG_SPIRAM
#define FRAME_MEM_CAPS                  MALLOC_CAP_SPIRAM
#else
#define FRAME_MEM_CAPS                  MALLOC_CAP_DEFAULT
#endif

#define UVC_DEVICE_INIT_TASK_STACK_SIZE (3 * 1024)
#define UVC_DEVICE_INIT_TASK_PRIORITY   5

#define UVC_DEVICE_URB_SIZE             CONFIG_USB_UVC_VIDEO_DEVICE_URB_SIZE

#define UVC_INIT_TIMEOUT_MS             CONFIG_USB_UVC_INIT_TIMEOUT_MS
#define UVC_INIT_MAX_PHYSICAL_DEVICES   6
#define UVC_INIT_MAX_STREAMS_PER_DEVICE 6

#define UVC_INTERVAL_DENOMINATOR        (10 * 1000 * 1000)

/* Camera-side encoder controls, as a V4L2 application sets them.
 *
 * Every field is a request rather than a live value, because a UVC camera only accepts most of
 * them at particular moments: some during the pre-stream handshake, some only while running,
 * and a camera that has just been unplugged accepts none. Keeping the request means a control
 * set before STREAMON is still applied at STREAMON, and survives the stop/start in between. */
struct uvc_h264_ctrls {
    esp_video_uvc_h264_xu_t xu;     /*!< What this camera's extension unit can do; unit_id is 0 until a stream is opened */

    uint32_t bitrate;               /*!< V4L2_CID_MPEG_VIDEO_BITRATE, 0 leaves the camera's own */
    uint32_t peak_bitrate;          /*!< V4L2_CID_MPEG_VIDEO_BITRATE_PEAK, 0 means "same as bitrate" */
    uint32_t gop_size;              /*!< V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, in frames */
    uint8_t min_qp;                 /*!< V4L2_CID_MPEG_VIDEO_H264_MIN_QP */
    uint8_t max_qp;                 /*!< V4L2_CID_MPEG_VIDEO_H264_MAX_QP */
    bool qp_set;                    /*!< Whether min_qp/max_qp carry a request at all */
    bool fixed_frame_rate;          /*!< V4L2_CID_MPEG_VIDEO_FRAME_SKIP_MODE == DISABLED */
    esp_video_uvc_h264_rc_mode_t rc_mode;   /*!< V4L2_CID_MPEG_VIDEO_BITRATE_MODE */

    uint32_t committed_bitrate;     /*!< What the camera agreed to, and what G_EXT_CTRLS reports */
};

struct uvc_video {
    uvc_host_stream_hdl_t stream_hdl;

    uint8_t dev_addr;
    uint8_t stream_index;
    uint32_t frame_info_num;
    uvc_host_frame_info_t *frame_info;
    uint8_t *frame_info_fmt_index;

    /* Current Configuration */
    enum uvc_host_stream_format uvc_stream_format;
    uint32_t interval;

    /* Camera-side controls. Guarded by ctrl_mutex together with stream_hdl and streaming,
     * because a control transfer cannot be issued under the core spinlock and must not race
     * with the STREAMOFF that closes the stream out from under it. */
    SemaphoreHandle_t ctrl_mutex;
    bool streaming;
    int8_t ae_priority;             /*!< V4L2_CID_EXPOSURE_AUTO_PRIORITY, -1 leaves the camera's own */
    struct uvc_h264_ctrls h264;

    SemaphoreHandle_t ready_sem;
};

struct uvc_video_core {
    portMUX_TYPE lock;

    uint8_t uvc_video_num;
    struct uvc_video uvc_video[0];
};

struct uvc_device_init_task_args {
    const uvc_host_driver_event_data_t event;
    struct uvc_video_core *core;
};

static const char *TAG = "usb_uvc_device";
static struct uvc_video_core *s_uvc_video_core = NULL;

static uint32_t uvc_to_v4l2_format(enum uvc_host_stream_format uvc_format, uint8_t *bpp_ptr)
{
    uint32_t bpp = 0;
    uint32_t v4l2_format = 0;

    switch (uvc_format) {
    case UVC_VS_FORMAT_YUY2:
        bpp = 2;
        v4l2_format = V4L2_PIX_FMT_YUYV;
        break;
    case UVC_VS_FORMAT_MJPEG:
        bpp = 1;
        v4l2_format = V4L2_PIX_FMT_JPEG;
        break;
    case UVC_VS_FORMAT_H264:
        bpp = 1;
        v4l2_format = V4L2_PIX_FMT_H264;
        break;
    case UVC_VS_FORMAT_H265:
        bpp = 1;
        v4l2_format = V4L2_PIX_FMT_HEVC;
        break;
    default:
        ESP_LOGE(TAG, "Unsupported pixel format %d", uvc_format);
        return 0; // Invalid format
    }

    if (bpp_ptr) {
        *bpp_ptr = bpp;
    }

    return v4l2_format;
}

static esp_err_t v4l2_to_uvc_format(uint32_t v4l2_format, enum uvc_host_stream_format *uvc_format)
{
    switch (v4l2_format) {
    case V4L2_PIX_FMT_YUYV:
        *uvc_format = UVC_VS_FORMAT_YUY2;
        return ESP_OK;
    case V4L2_PIX_FMT_JPEG:
        *uvc_format = UVC_VS_FORMAT_MJPEG;
        return ESP_OK;
    case V4L2_PIX_FMT_H264:
        *uvc_format = UVC_VS_FORMAT_H264;
        return ESP_OK;
    case V4L2_PIX_FMT_HEVC:
        *uvc_format = UVC_VS_FORMAT_H265;
        return ESP_OK;
    default:
        ESP_LOGE(TAG, "Unsupported pixel format %" PRIu32, v4l2_format);
        return ESP_ERR_INVALID_ARG;
    }
}

static bool uvc_frame_callback(const uvc_host_frame_t *frame, void *user_ctx)
{
    struct esp_video *video = (struct esp_video *)user_ctx;
    struct esp_video_buffer *buffer = CAPTURE_VIDEO_BUF(video);

    CAPTURE_VIDEO_DONE_BUF(video, frame->data, frame->data_len);

    struct esp_video_buffer_element *element = esp_video_buffer_get_element_by_buffer(buffer, frame->data);
    element->priv_data = (void *)frame;

    return false;
}

static void uvc_event_callback(const uvc_host_stream_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
        ESP_LOGD(TAG, "USB error has occurred, err_no = %i", event->transfer_error.error);
        break;
    case UVC_HOST_DEVICE_DISCONNECTED: {
        struct esp_video *video = (struct esp_video *)user_ctx;
        struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);

        ESP_LOGD(TAG, "Device disconnected, dev_addr = %d, stream_index = %d", device->dev_addr, device->stream_index);

        device->dev_addr = 0;
        device->stream_index = 0;
        device->frame_info_num = 0;
        xSemaphoreTake(device->ready_sem, 0);
        break;
    }
    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
        ESP_LOGD(TAG, "Frame buffer overflow");
        break;
    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
        ESP_LOGD(TAG, "Frame buffer underflow");
        break;
    default:
        abort();
        break;
    }
}

static void uvc_host_driver_event_callback(const uvc_host_driver_event_data_t *event, void *user_ctx)
{
    struct uvc_video_core *core = (struct uvc_video_core *)user_ctx;

    switch (event->type) {
    case UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED: {
        struct uvc_video *found_device = NULL;

        ESP_LOGD(TAG, "Device connected");

        /**
         * Active the video device with given stream parameters
         */

        portENTER_CRITICAL(&core->lock);
        for (int i = 0; i < core->uvc_video_num; i++) {
            struct uvc_video *device = &core->uvc_video[i];

            if (device->dev_addr == 0) {
                device->dev_addr = event->device_connected.dev_addr;
                device->stream_index = event->device_connected.uvc_stream_index;
                device->frame_info_num = event->device_connected.frame_info_num;
                found_device = device;
                break;
            }
        }
        portEXIT_CRITICAL(&core->lock);

        if (!found_device) {
            ESP_LOGD(TAG, "No free UVC device found");
            break;
        } else {
            xSemaphoreGive(found_device->ready_sem);
        }

        ESP_LOGD(TAG, "UVC device found, dev_addr = %d, stream_index = %d",
                 found_device->dev_addr, found_device->stream_index);
        break;
    }
    default:
        ESP_LOGD(TAG, "Unknown UVC host driver event type: %d", event->type);
        break;
    }
}

static esp_err_t uvc_video_init(struct esp_video *video)
{
    esp_err_t ret;
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);
    struct uvc_video_core *core = s_uvc_video_core;

    /* Check if UVC devices are already enumerated before waiting on semaphore */
    uint8_t dev_addr_list[UVC_INIT_MAX_PHYSICAL_DEVICES] = {0};
    int num_of_devices = 0;
    int detected_uvc_count = 0;

    usb_host_device_addr_list_fill(sizeof(dev_addr_list), dev_addr_list, &num_of_devices);

    /* Check each USB device to see if it's a UVC device */
    for (int i = 0; i < num_of_devices; i++) {
        for (int stream_idx = 0; stream_idx < UVC_INIT_MAX_STREAMS_PER_DEVICE; stream_idx++) {
            size_t frame_list_size = 0;
            /* Try to get frame list - if successful, it's a UVC device with this stream */
            if (uvc_host_get_frame_list(dev_addr_list[i], stream_idx, NULL, &frame_list_size) == ESP_OK) {
                if (frame_list_size > 0) {
                    detected_uvc_count++;
                    /* Try to assign this device to an available slot */
                    portENTER_CRITICAL(&core->lock);
                    for (int j = 0; j < core->uvc_video_num; j++) {
                        struct uvc_video *uvc_dev = &core->uvc_video[j];
                        if (uvc_dev->dev_addr == 0) {
                            uvc_dev->dev_addr = dev_addr_list[i];
                            uvc_dev->stream_index = stream_idx;
                            uvc_dev->frame_info_num = frame_list_size;
                            break;
                        }
                    }
                    portEXIT_CRITICAL(&core->lock);
                }
                /* Found a valid stream for this device, move to next device
                 * Note: currently only support one stream per device */
                break;
            }
        }
    }

    /* If all expected UVC devices are already detected, no need to wait */
    if (detected_uvc_count >= core->uvc_video_num) {
        ESP_LOGI(TAG, "All UVC devices already enumerated");
    } else {
        ESP_LOGI(TAG, "Waiting for UVC device to be enumerated...");
        ESP_RETURN_ON_FALSE((xSemaphoreTake(device->ready_sem, UVC_INIT_TIMEOUT_MS / portTICK_PERIOD_MS) == pdPASS), ESP_ERR_NOT_FOUND,
                            TAG, "Failed to take UVC device ready semaphore");
    }

    ESP_GOTO_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, fail0, TAG, "UVC device=%p is not connected", device);

    device->frame_info = malloc((sizeof(uvc_host_frame_info_t) + sizeof(uint8_t)) * device->frame_info_num);
    ESP_GOTO_ON_FALSE(device->frame_info, ESP_ERR_NO_MEM, fail0, TAG, "Failed to allocate memory for frame info");

    device->frame_info_fmt_index = (uint8_t *)device->frame_info + sizeof(uvc_host_frame_info_t) * device->frame_info_num;
    memset(device->frame_info_fmt_index, -1, sizeof(uint8_t) * device->frame_info_num);

    size_t list_size = device->frame_info_num;
    ESP_GOTO_ON_ERROR(uvc_host_get_frame_list(device->dev_addr, device->stream_index, (uvc_host_frame_info_t (*)[1])device->frame_info, &list_size),
                      fail1, TAG, "Failed to get frame info");

#if 0
    for (int i = 0; i < device->frame_info_num; i++) {
        ESP_LOGI(TAG, "Frame info %d: format=%d, h_res=%d, v_res=%d, default_interval=%d, interval_type=%d", i,
                 device->frame_info[i].format, device->frame_info[i].h_res, device->frame_info[i].v_res, device->frame_info[i].default_interval,
                 device->frame_info[i].interval_type);
        if (device->frame_info[i].interval_type == 0) {
            ESP_LOGI(TAG, "\tinterval_min = %d, interval_max = %d, interval_step = %d",
                     device->frame_info[i].interval_min, device->frame_info[i].interval_max, device->frame_info[i].interval_step);
        } else {
            int num = MIN(device->frame_info[i].interval_type, CONFIG_UVC_INTERVAL_ARRAY_SIZE);
            for (int j = 0; j < num; j++) {
                ESP_LOGI(TAG, "\tinterval[%d] = %d", j, device->frame_info[i].interval[j]);
            }
        }
    }
#endif

    /*
     * Build a mapping table from UVC format index to V4L2 format index
     */
    int min_uvc_fmt_index = UVC_VS_FORMAT_MJPEG;
    int max_uvc_fmt_index = UVC_VS_FORMAT_H265;
    int v4l2_fmt_index = 0;
    for (int index = min_uvc_fmt_index; index <= max_uvc_fmt_index; index++) {
        int j;
        bool found = false;

        for (j = 0; j < device->frame_info_num; j++) {
            if (device->frame_info[j].format == index) {
                found = true;
                break;
            }
        }

        if (found) {
            for (; j < device->frame_info_num; j++) {
                if (device->frame_info[j].format == index) {
                    device->frame_info_fmt_index[j] = v4l2_fmt_index;
                }
            }
        }

        v4l2_fmt_index++;
    }

    uint8_t bpp = 0;
    uvc_host_frame_info_t *frame_info = &device->frame_info[0];

    struct v4l2_format format = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .fmt.pix = {
            .width = frame_info->h_res,
            .height = frame_info->v_res,
            .pixelformat = uvc_to_v4l2_format(frame_info->format, &bpp),
        },
    };
    ESP_GOTO_ON_ERROR(esp_video_config_buffer(video, &format, FRAME_MEM_CAPS), fail1, TAG, "failed to configure stream buffer");

    device->uvc_stream_format = frame_info->format;
    device->interval = frame_info->default_interval;

    return ESP_OK;

fail1:
    free(device->frame_info);
    device->frame_info = NULL;
fail0:
    xSemaphoreGive(device->ready_sem);
    return ret;
}

/* Mapping between the V4L2 controls this device publishes and the camera's own encoder.
 *
 * The ranges are what a generic camera can be asked for; the H.264 extension unit reports the
 * range this particular camera accepts and uvc_video_query_ext_ctrl() substitutes it once a
 * stream has been opened. */
#define UVC_H264_MIN_BITRATE        64000
#define UVC_H264_MAX_BITRATE        20000000
#define UVC_H264_BITRATE_STEP       1000
#define UVC_H264_MIN_GOP            1
#define UVC_H264_MAX_GOP            300
#define UVC_H264_MIN_QP             0
#define UVC_H264_MAX_QP             51

static const struct v4l2_query_ext_ctrl s_uvc_qctrl[] = {
    {
        .id = V4L2_CID_MPEG_VIDEO_BITRATE,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .minimum = UVC_H264_MIN_BITRATE,
        .maximum = UVC_H264_MAX_BITRATE,
        .step = UVC_H264_BITRATE_STEP,
        .default_value = CONFIG_ESP_VIDEO_UVC_H264_BITRATE_BPS,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "Video Bitrate",
    },
    {
        .id = V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .minimum = UVC_H264_MIN_BITRATE,
        .maximum = UVC_H264_MAX_BITRATE,
        .step = UVC_H264_BITRATE_STEP,
        .default_value = CONFIG_ESP_VIDEO_UVC_H264_BITRATE_BPS,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "Video Peak Bitrate",
    },
    {
        .id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
        .type = V4L2_CTRL_TYPE_MENU,
        .minimum = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
        .maximum = V4L2_MPEG_VIDEO_BITRATE_MODE_CQ,
        .step = 1,
        .default_value = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "Video Bitrate Mode",
    },
    {
        .id = V4L2_CID_MPEG_VIDEO_FRAME_SKIP_MODE,
        .type = V4L2_CTRL_TYPE_MENU,
        .minimum = V4L2_MPEG_VIDEO_FRAME_SKIP_MODE_DISABLED,
        .maximum = V4L2_MPEG_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT,
        .step = 1,
        .default_value = V4L2_MPEG_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "Frame Skip Mode",
    },
    {
        .id = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .minimum = UVC_H264_MIN_GOP,
        .maximum = UVC_H264_MAX_GOP,
        .step = 1,
        .default_value = CONFIG_ESP_VIDEO_UVC_H264_GOP_SIZE,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "H264 I-Frame Period",
    },
    {
        .id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .minimum = UVC_H264_MIN_QP,
        .maximum = UVC_H264_MAX_QP,
        .step = 1,
        .default_value = UVC_H264_MIN_QP,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "H264 Minimum QP Value",
    },
    {
        .id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .minimum = UVC_H264_MIN_QP,
        .maximum = UVC_H264_MAX_QP,
        .step = 1,
        .default_value = UVC_H264_MAX_QP,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "H264 Maximum QP Value",
    },
    {
        .id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
        .type = V4L2_CTRL_TYPE_BUTTON,
        .minimum = 0,
        .maximum = 0,
        .step = 0,
        .default_value = 0,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "Force Key Frame",
    },
    {
        .id = V4L2_CID_EXPOSURE_AUTO_PRIORITY,
        .type = V4L2_CTRL_TYPE_BOOLEAN,
        .minimum = 0,
        .maximum = 1,
        .step = 1,
        .default_value = 1,
        .elems = 1,
        .nr_of_dims = 0,
        .name = "Auto Exposure, Priority",
    },
};

/* The payload spec's wIFramePeriod is a duration; V4L2_CID_MPEG_VIDEO_H264_I_PERIOD is a frame
 * count, the same as on the on-chip encoder device. The negotiated frame interval is what
 * converts between them, which is also why the conversion happens at stream start rather than
 * when the control is set. */
static uint16_t uvc_gop_frames_to_ms(const struct uvc_video *device, uint32_t gop_frames)
{
    if (!gop_frames || !device->interval) {
        return 0;
    }

    /* Rounded, not truncated. 30 frames at a 333333 (100 ns) interval is 999.999 ms, and
     * truncation asks the camera for 999 - measured being refused outright by a camera that
     * accepts 1000, which then rejects the whole probe and leaves the bitrate, the resolution
     * and the GOP all unapplied. */
    uint64_t ms = ((uint64_t)gop_frames * device->interval + 5000ULL) / 10000ULL;

    if (ms == 0) {
        ms = 1;
    }
    return (ms > UINT16_MAX) ? UINT16_MAX : (uint16_t)ms;
}

/* The bitrate the camera committed is what a rate controller has to work down from, so it is
 * recorded whichever route the change took. */
static esp_err_t uvc_h264_push_bitrate(struct uvc_video *device)
{
    struct uvc_h264_ctrls *h264 = &device->h264;
    uint32_t committed = 0;

    if (!h264->xu.unit_id) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!h264->xu.can_set_bitrate_live) {
        ESP_LOGW(TAG, "camera has no UVCX_BITRATE_LAYERS - %" PRIu32 " bps will take effect at "
                 "the next stream start", h264->bitrate);
        return ESP_OK;
    }

    esp_err_t ret = esp_video_uvc_h264_xu_set_bitrate(device->stream_hdl, &h264->xu, h264->bitrate,
                                                      h264->peak_bitrate, &committed);
    if (ret == ESP_OK) {
        h264->committed_bitrate = committed;
    }
    return ret;
}

static esp_err_t uvc_h264_push_rc_mode(struct uvc_video *device)
{
    struct uvc_h264_ctrls *h264 = &device->h264;

    if (!h264->xu.unit_id) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!h264->xu.can_set_rc_mode_live) {
        ESP_LOGW(TAG, "camera has no UVCX_RATE_CONTROL_MODE - the rate-control mode will take "
                 "effect at the next stream start");
        return ESP_OK;
    }
    return esp_video_uvc_h264_xu_set_rc_mode(device->stream_hdl, &h264->xu, h264->rc_mode,
                                             h264->fixed_frame_rate);
}

static esp_err_t uvc_h264_push_qp(struct uvc_video *device)
{
    struct uvc_h264_ctrls *h264 = &device->h264;

    if (!h264->xu.unit_id) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!h264->xu.can_set_qp_live) {
        ESP_LOGW(TAG, "camera has no UVCX_QP_STEPS_LAYERS - the QP limits will take effect at "
                 "the next stream start");
        return ESP_OK;
    }
    return esp_video_uvc_h264_xu_set_qp(device->stream_hdl, &h264->xu, h264->min_qp, h264->max_qp);
}

/* Configure the camera's encoder for the stream about to start.
 *
 * Only ever called on an H.264 stream. Running the extension-unit handshake while the camera
 * is streaming MJPEG would reconfigure an encoder nothing is reading, on a control whose
 * resolution and frame interval have to match the format that was actually committed.
 *
 * Never fatal: a camera with no extension unit, or one that refuses the transaction, still
 * streams - it just streams at whatever rate it chose for itself. */
static void uvc_h264_configure_at_start(struct uvc_video *device)
{
    struct uvc_h264_ctrls *h264 = &device->h264;

    h264->committed_bitrate = 0;

    if (esp_video_uvc_h264_xu_probe(device->stream_hdl, &h264->xu) != ESP_OK) {
        ESP_LOGD(TAG, "camera has no H.264 extension unit - its encoder is not configurable");
        return;
    }

    const esp_video_uvc_h264_config_t cfg = {
        .bitrate_bps      = h264->bitrate,
        .peak_bitrate_bps = h264->peak_bitrate,
        .iframe_period_ms = uvc_gop_frames_to_ms(device, h264->gop_size),
        .fixed_frame_rate = h264->fixed_frame_rate,
        .rc_mode          = h264->rc_mode,
    };
    esp_video_uvc_h264_committed_t committed = {0};

    if (esp_video_uvc_h264_xu_configure(device->stream_hdl, &h264->xu, &cfg, &committed) != ESP_OK) {
        ESP_LOGW(TAG, "camera refused the H.264 configuration and kept its own");
        return;
    }
    h264->committed_bitrate = committed.bitrate_bps;

    /* QP limits are not part of the probe/commit structure, so they are a separate transaction
     * and only exist on cameras that declare the control. */
    if (h264->qp_set && h264->xu.can_set_qp_live) {
        (void)esp_video_uvc_h264_xu_set_qp(device->stream_hdl, &h264->xu, h264->min_qp, h264->max_qp);
    }
}

/* The encoder controls only mean something on an H.264 stream. Refusing them outright while a
 * different format is streaming is the honest answer: recording a bitrate the running stream
 * will never use looks like success and is not. Before STREAMON there is nothing to refuse -
 * the value is a request for whatever format is selected next. */
static bool uvc_encoder_ctrl_usable(const struct uvc_video *device, uint32_t id)
{
    if (device->streaming && device->uvc_stream_format != UVC_VS_FORMAT_H264) {
        ESP_LOGE(TAG, "id=%" PRIx32 " is an H.264 encoder control and the running stream is not H.264", id);
        return false;
    }
    return true;
}

static esp_err_t uvc_video_set_ext_ctrl(struct esp_video *video, const struct v4l2_ext_controls *ctrls)
{
    esp_err_t ret = ESP_OK;
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);
    struct uvc_h264_ctrls *h264 = &device->h264;

    ESP_RETURN_ON_FALSE(device->ctrl_mutex, ESP_ERR_INVALID_STATE, TAG, "UVC device is not installed");

    /* Held across the whole loop, and taken by uvc_video_start()/uvc_video_stop() too: every
     * branch below touches device->stream_hdl, and a concurrent STREAMOFF would otherwise be
     * free to close it between the streaming check and the control transfer. */
    xSemaphoreTake(device->ctrl_mutex, portMAX_DELAY);

    const bool live = device->streaming;

    for (int i = 0; i < ctrls->count; i++) {
        struct v4l2_ext_control *ctrl = &ctrls->controls[i];

        switch (ctrl->id) {
        case V4L2_CID_MPEG_VIDEO_BITRATE:
            if (!uvc_encoder_ctrl_usable(device, ctrl->id)) {
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
            if (ctrl->value < UVC_H264_MIN_BITRATE || ctrl->value > UVC_H264_MAX_BITRATE) {
                ESP_LOGE(TAG, "bitrate value is out of range");
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            h264->bitrate = ctrl->value;
            if (live) {
                ret = uvc_h264_push_bitrate(device);
            }
            break;
        case V4L2_CID_MPEG_VIDEO_BITRATE_PEAK:
            if (!uvc_encoder_ctrl_usable(device, ctrl->id)) {
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
            if (ctrl->value < UVC_H264_MIN_BITRATE || ctrl->value > UVC_H264_MAX_BITRATE) {
                ESP_LOGE(TAG, "peak bitrate value is out of range");
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            h264->peak_bitrate = ctrl->value;
            if (live && h264->bitrate) {
                ret = uvc_h264_push_bitrate(device);
            }
            break;
        case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
            if (!uvc_encoder_ctrl_usable(device, ctrl->id)) {
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
            switch (ctrl->value) {
            case V4L2_MPEG_VIDEO_BITRATE_MODE_VBR:
                h264->rc_mode = ESP_VIDEO_UVC_H264_RC_VBR;
                break;
            case V4L2_MPEG_VIDEO_BITRATE_MODE_CBR:
                h264->rc_mode = ESP_VIDEO_UVC_H264_RC_CBR;
                break;
            case V4L2_MPEG_VIDEO_BITRATE_MODE_CQ:
                h264->rc_mode = ESP_VIDEO_UVC_H264_RC_CONST_QP;
                break;
            default:
                ESP_LOGE(TAG, "bitrate mode %" PRId32 " is not supported", ctrl->value);
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            if (ret == ESP_OK && live) {
                ret = uvc_h264_push_rc_mode(device);
            }
            break;
        case V4L2_CID_MPEG_VIDEO_FRAME_SKIP_MODE:
            if (!uvc_encoder_ctrl_usable(device, ctrl->id)) {
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
            if (ctrl->value < V4L2_MPEG_VIDEO_FRAME_SKIP_MODE_DISABLED ||
                    ctrl->value > V4L2_MPEG_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT) {
                ESP_LOGE(TAG, "frame skip mode %" PRId32 " is not supported", ctrl->value);
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            /* Forbidding frame skipping is what makes quality, rather than frame rate, absorb a
             * bitrate the picture does not fit into. The payload spec has one flag for it, so
             * the two "limit" modes are the same answer: skipping is allowed. */
            h264->fixed_frame_rate = (ctrl->value == V4L2_MPEG_VIDEO_FRAME_SKIP_MODE_DISABLED);
            if (live) {
                ret = uvc_h264_push_rc_mode(device);
            }
            break;
        case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
            if (!uvc_encoder_ctrl_usable(device, ctrl->id)) {
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
            if (ctrl->value < UVC_H264_MIN_GOP || ctrl->value > UVC_H264_MAX_GOP) {
                ESP_LOGE(TAG, "GOP value is out of range");
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            h264->gop_size = ctrl->value;
            if (live) {
                /* wIFramePeriod lives only in the probe/commit structure, which is the
                 * pre-stream negotiation. Ask for a key frame now if one is wanted sooner. */
                ESP_LOGW(TAG, "the key-frame period takes effect at the next stream start");
            }
            break;
        case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
            if (!uvc_encoder_ctrl_usable(device, ctrl->id)) {
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
            if (ctrl->value < UVC_H264_MIN_QP || ctrl->value > UVC_H264_MAX_QP) {
                ESP_LOGE(TAG, "min QP value is out of range");
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            if (h264->qp_set && ctrl->value > h264->max_qp) {
                ESP_LOGE(TAG, "min QP %" PRId32 " exceeds max QP %u", ctrl->value, h264->max_qp);
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            h264->min_qp = (uint8_t)ctrl->value;
            if (!h264->qp_set) {
                h264->max_qp = UVC_H264_MAX_QP;
                h264->qp_set = true;
            }
            if (live) {
                ret = uvc_h264_push_qp(device);
            }
            break;
        case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
            if (!uvc_encoder_ctrl_usable(device, ctrl->id)) {
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
            if (ctrl->value < UVC_H264_MIN_QP || ctrl->value > UVC_H264_MAX_QP) {
                ESP_LOGE(TAG, "max QP value is out of range");
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            if (h264->qp_set && ctrl->value < h264->min_qp) {
                ESP_LOGE(TAG, "max QP %" PRId32 " is below min QP %u", ctrl->value, h264->min_qp);
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            h264->max_qp = (uint8_t)ctrl->value;
            if (!h264->qp_set) {
                h264->min_qp = UVC_H264_MIN_QP;
                h264->qp_set = true;
            }
            if (live) {
                ret = uvc_h264_push_qp(device);
            }
            break;
        case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
            if (!live) {
                ESP_LOGE(TAG, "no stream is running");
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
            ret = uvc_host_stream_request_key_frame(device->stream_hdl);
            if (ret == ESP_ERR_NOT_SUPPORTED && h264->xu.can_set_picture_type) {
                /* The camera does not implement the VideoStreaming control but does implement
                 * the extension unit's own, which is the same request by another route. */
                ret = esp_video_uvc_h264_xu_request_idr(device->stream_hdl, &h264->xu);
            }
            break;
        case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
            if (ctrl->value < 0 || ctrl->value > 1) {
                ESP_LOGE(TAG, "AE priority must be 0 or 1");
                ret = ESP_ERR_INVALID_ARG;
                break;
            }
            /* Recorded as well as applied: the camera resets it when the stream is closed, so
             * uvc_video_start() has to re-assert it on every start. */
            device->ae_priority = (int8_t)ctrl->value;
            if (live) {
                ret = esp_video_uvc_camera_set_ae_priority(device->stream_hdl, (uint8_t)ctrl->value, NULL);
            }
            break;
        default:
            ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", ctrl->id);
            ret = ESP_ERR_NOT_SUPPORTED;
            break;
        }

        if (ret != ESP_OK) {
            break;
        }
    }

    xSemaphoreGive(device->ctrl_mutex);
    return ret;
}

static esp_err_t uvc_video_get_ext_ctrl(struct esp_video *video, struct v4l2_ext_controls *ctrls)
{
    esp_err_t ret = ESP_OK;
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);
    struct uvc_h264_ctrls *h264 = &device->h264;

    ESP_RETURN_ON_FALSE(device->ctrl_mutex, ESP_ERR_INVALID_STATE, TAG, "UVC device is not installed");

    xSemaphoreTake(device->ctrl_mutex, portMAX_DELAY);

    for (int i = 0; i < ctrls->count; i++) {
        struct v4l2_ext_control *ctrl = &ctrls->controls[i];

        switch (ctrl->id) {
        case V4L2_CID_MPEG_VIDEO_BITRATE:
            /* What the camera committed, not what was asked for. The camera clamps freely, and
             * a rate controller that reads back its own request will keep stepping away from a
             * ceiling it never reached. */
            ctrl->value = h264->committed_bitrate ? h264->committed_bitrate : h264->bitrate;
            break;
        case V4L2_CID_MPEG_VIDEO_BITRATE_PEAK:
            ctrl->value = h264->peak_bitrate ? h264->peak_bitrate : h264->bitrate;
            break;
        case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
            switch (h264->rc_mode) {
            case ESP_VIDEO_UVC_H264_RC_CBR:
                ctrl->value = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
                break;
            case ESP_VIDEO_UVC_H264_RC_CONST_QP:
                ctrl->value = V4L2_MPEG_VIDEO_BITRATE_MODE_CQ;
                break;
            default:
                ctrl->value = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
                break;
            }
            break;
        case V4L2_CID_MPEG_VIDEO_FRAME_SKIP_MODE:
            ctrl->value = h264->fixed_frame_rate ? V4L2_MPEG_VIDEO_FRAME_SKIP_MODE_DISABLED
                          : V4L2_MPEG_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT;
            break;
        case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
            ctrl->value = h264->gop_size;
            break;
        case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
            ctrl->value = h264->qp_set ? h264->min_qp : UVC_H264_MIN_QP;
            break;
        case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
            ctrl->value = h264->qp_set ? h264->max_qp : UVC_H264_MAX_QP;
            break;
        case V4L2_CID_EXPOSURE_AUTO_PRIORITY: {
            uint8_t priority = 0;

            /* Ask the camera when one is streaming: this control is the camera's to refuse,
             * and it is the only one here whose committed value can differ from the request
             * without anything having gone wrong. */
            if (device->streaming &&
                    esp_video_uvc_camera_get_ae_priority(device->stream_hdl, &priority) == ESP_OK) {
                ctrl->value = priority;
            } else if (device->ae_priority >= 0) {
                ctrl->value = device->ae_priority;
            } else {
                ctrl->value = 1;    /* The UVC default: exposure wins, frame rate falls */
            }
            break;
        }
        default:
            ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", ctrl->id);
            ret = ESP_ERR_NOT_SUPPORTED;
            break;
        }

        if (ret != ESP_OK) {
            break;
        }
    }

    xSemaphoreGive(device->ctrl_mutex);
    return ret;
}

static esp_err_t uvc_video_query_ext_ctrl(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);

    ESP_RETURN_ON_FALSE(device->ctrl_mutex, ESP_ERR_INVALID_STATE, TAG, "UVC device is not installed");

    ESP_RETURN_ON_ERROR(esp_video_device_common_query_ext_ctrl(s_uvc_qctrl, ARRAY_SIZE(s_uvc_qctrl), qctrl),
                        TAG, "Failed to query control");

    /* The static table is what a generic camera can be asked for. Once a stream has been opened
     * the extension unit has told us what this camera actually accepts, and that is the range
     * the application needs, not ours. */
    xSemaphoreTake(device->ctrl_mutex, portMAX_DELAY);

    const esp_video_uvc_h264_xu_t *xu = &device->h264.xu;

    if (xu->bitrate_range_known &&
            (qctrl->id == V4L2_CID_MPEG_VIDEO_BITRATE || qctrl->id == V4L2_CID_MPEG_VIDEO_BITRATE_PEAK)) {
        qctrl->minimum = xu->bitrate_min;
        qctrl->maximum = xu->bitrate_max;
        qctrl->default_value = xu->bitrate_def;
    }

    xSemaphoreGive(device->ctrl_mutex);
    return ESP_OK;
}

static esp_err_t uvc_video_start(struct esp_video *video, uint32_t type)
{
    esp_err_t ret = ESP_OK;
    struct esp_video_buffer *buffer = CAPTURE_VIDEO_BUF(video);
    struct esp_video_buffer_info *info = &buffer->info;
    int buffer_count = info->count;
    uint8_t *buffer_array[buffer_count];
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);

    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    for (int i = 0; i < buffer_count; i++) {
        buffer->element[i].priv_data = NULL;
        buffer_array[i] = buffer->element[i].buffer;
    }

    /* The format the stream is opened with is what fixes the isochronous geometry:
     * uvc_host_stream_open() probes the camera, learns dwMaxPayloadTransferSize and picks the
     * alternate setting that matches it, then allocates the ISOC URBs from that endpoint. */
    const uvc_host_stream_format_t vs_format_selected = {
        .h_res = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video),
        .v_res = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video),
        .fps = (float)UVC_INTERVAL_DENOMINATOR / (float)device->interval,
        .format = device->uvc_stream_format,
    };

    uvc_host_stream_config_t stream_config = {
        .event_cb = uvc_event_callback,
        .frame_cb = uvc_frame_callback,
        .user_ctx = video,
        .usb = {
            .dev_addr = device->dev_addr,
            .vid = UVC_HOST_ANY_VID,
            .pid = UVC_HOST_ANY_PID,
            .uvc_stream_index = device->stream_index,
        },
        .vs_format = vs_format_selected,
        .advanced = {
            .number_of_frame_buffers = info->count,
            .frame_size = info->size,
            .frame_heap_caps = info->caps,
            .number_of_urbs = buffer_count,
            .user_frame_buffers = buffer_array,
            .urb_size = UVC_DEVICE_URB_SIZE,
        },
    };

    xSemaphoreTake(device->ctrl_mutex, portMAX_DELAY);

    device->stream_hdl = NULL;
    memset(&device->h264.xu, 0, sizeof(device->h264.xu));
    device->h264.committed_bitrate = 0;

    ESP_GOTO_ON_ERROR(uvc_host_stream_open(&stream_config, 0, &device->stream_hdl), fail0, TAG, "Failed to open UVC stream");

    uvc_host_stream_format_t uvc_format = vs_format_selected;
    ESP_GOTO_ON_ERROR(uvc_host_stream_format_select(device->stream_hdl, &uvc_format), fail1, TAG, "Failed to set UVC format");

    /* Only on an H.264 stream. The extension unit configures the camera's H.264 encoder, and
     * its configuration structure carries the resolution and frame interval the VideoStreaming
     * interface committed to - running the handshake while the camera is sending MJPEG would
     * reconfigure an encoder nothing is reading, against a format it was not asked for. */
    if (device->uvc_stream_format == UVC_VS_FORMAT_H264) {
        uvc_h264_configure_at_start(device);
    }

    ESP_GOTO_ON_ERROR(uvc_host_stream_start(device->stream_hdl), fail1, TAG, "Failed to start UVC stream");

    device->streaming = true;

    /* After the start, because this is a live control rather than part of the setup handshake,
     * and on every start because the camera forgets it when the stream closes. Failure is not
     * fatal: a camera without AE Priority still streams, it just cannot be stopped from trading
     * frame rate for exposure. */
    if (device->ae_priority >= 0) {
        (void)esp_video_uvc_camera_set_ae_priority(device->stream_hdl,
                                                   (uint8_t)device->ae_priority, NULL);
    }

    xSemaphoreGive(device->ctrl_mutex);
    return ESP_OK;

fail1:
    uvc_host_stream_close(device->stream_hdl);
    device->stream_hdl = NULL;
fail0:
    xSemaphoreGive(device->ctrl_mutex);
    return ret;
}

static esp_err_t uvc_video_stop(struct esp_video *video, uint32_t type)
{
    struct esp_video_buffer_element *element;
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);
    struct esp_video_buffer *buffer = CAPTURE_VIDEO_BUF(video);
    int buffer_count = CAPTURE_VIDEO_BUF_COUNT(video);

    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    /* Held across the close, and taken by the control paths too: clearing the flag first is not
     * enough on its own, because a control transfer that already passed the check would
     * otherwise still be in flight on a handle this is about to free. */
    xSemaphoreTake(device->ctrl_mutex, portMAX_DELAY);

    device->streaming = false;

    esp_err_t ret = uvc_host_stream_stop(device->stream_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop UVC stream");
        goto out;
    }

    /**
     * Free all cached frames to avoid UVC stream stop failed
     */
    while ((element = esp_video_get_done_element(video, V4L2_BUF_TYPE_VIDEO_CAPTURE)) != NULL) {
        uvc_host_frame_return(device->stream_hdl, (uvc_host_frame_t *)element->priv_data);
    }

    ret = uvc_host_stream_close(device->stream_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to close UVC stream");
        goto out;
    }
    /**
     * Clear the private data of the buffer elements, so will not be passed to the UVC stream in the function uvc_video_notify.
     */
    for (int i = 0; i < buffer_count; i++) {
        buffer->element[i].priv_data = NULL;
    }

    /* After the elements are cleared, because uvc_video_notify() reaches the handle through
     * them and does not hold this mutex. */
    device->stream_hdl = NULL;

    /* The extension unit was discovered on the stream that has just gone away, and the bitrate
     * belonged to it. Both are re-established at the next start. */
    memset(&device->h264.xu, 0, sizeof(device->h264.xu));
    device->h264.committed_bitrate = 0;

out:
    xSemaphoreGive(device->ctrl_mutex);
    return ret;
}

static esp_err_t uvc_video_deinit(struct esp_video *video)
{
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);

    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    free(device->frame_info);
    device->frame_info = NULL;

    xSemaphoreGive(device->ready_sem);

    return ESP_OK;
}

static esp_err_t uvc_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);

    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    if (index >= device->frame_info_num) {
        ESP_LOGD(TAG, "Index %"PRIu32" is out of range", index);
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < device->frame_info_num; i++) {
        if (device->frame_info_fmt_index[i] == index) {
            *pixel_format = uvc_to_v4l2_format(device->frame_info[i].format, NULL);
            ret = ESP_OK;
            break;
        }
    }

    return ret;
}

static esp_err_t uvc_video_set_format(struct esp_video *video, const struct v4l2_format *format)
{
    enum uvc_host_stream_format uvc_stream_format;
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);

    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    ESP_RETURN_ON_ERROR(v4l2_to_uvc_format(GET_FORMAT_PIXEL_FORMAT(format), &uvc_stream_format), TAG, "Failed to convert V4L2 format to UVC format");

    uint32_t width = GET_FORMAT_WIDTH(format);
    uint32_t height = GET_FORMAT_HEIGHT(format);
    uint32_t interval = 0;
    bool found = false;
    for (int i = 0; i < device->frame_info_num; i++) {
        if (device->frame_info[i].format == uvc_stream_format) {
            if (device->frame_info[i].h_res == width && device->frame_info[i].v_res == height) {
                interval = device->frame_info[i].default_interval;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(esp_video_config_buffer(video, format, FRAME_MEM_CAPS), TAG, "failed to configure stream buffer");

    device->uvc_stream_format = uvc_stream_format;
    device->interval = interval;

    return ESP_OK;
}

static esp_err_t uvc_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);

    if (event == ESP_VIDEO_BUFFER_VALID) {
        struct esp_video_buffer_element *element = CAPTURE_VIDEO_GET_QUEUED_ELEMENT(video);
        if (element) {
            uvc_host_frame_t *frame = (uvc_host_frame_t *)element->priv_data;
            if (frame) {
                ESP_RETURN_ON_ERROR(uvc_host_frame_return(device->stream_hdl, frame), TAG, "Failed to return UVC frame");
            }
        }
    }

    return ESP_OK;
}

static esp_err_t uvc_video_set_parm(struct esp_video *video, struct v4l2_streamparm *stream_parm, struct esp_video_stream *stream)
{
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);
    struct v4l2_fract *time_per_frame = &stream_parm->parm.capture.timeperframe;

    ESP_RETURN_ON_FALSE(time_per_frame->numerator > 0 && time_per_frame->denominator > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid time per frame");
    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    bool found = false;
    uint32_t width = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video);
    uint32_t height = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video);
    uint32_t interval = (float)UVC_INTERVAL_DENOMINATOR / (float)time_per_frame->denominator * time_per_frame->numerator;

    for (int i = 0; i < device->frame_info_num; i++) {
        uvc_host_frame_info_t *frame_info = &device->frame_info[i];

        if ((frame_info->format == device->uvc_stream_format) &&
                (frame_info->h_res == width) &&
                (frame_info->v_res == height)) {
            if (!frame_info->interval_type) {
                if (interval <= frame_info->interval_max && interval >= frame_info->interval_min) {
                    if (!((interval - frame_info->interval_min) % frame_info->interval_step)) {
                        found = true;
                        break;
                    }
                }
            } else {
                for (int j = 0; j < frame_info->interval_type; j++) {
                    if (interval == frame_info->interval[j]) {
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    if (!found) {
        return ESP_ERR_INVALID_ARG;
    }

    device->interval = interval;

    return ESP_OK;
}

static esp_err_t uvc_video_get_parm(struct esp_video *video, struct v4l2_streamparm *stream_parm, struct esp_video_stream *stream)
{
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);
    struct v4l2_fract *time_per_frame = &stream_parm->parm.capture.timeperframe;

    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    time_per_frame->numerator   = device->interval;
    time_per_frame->denominator = UVC_INTERVAL_DENOMINATOR;

    return ESP_OK;
}

static esp_err_t uvc_video_enum_framesizes(struct esp_video *video, struct v4l2_frmsizeenum *frmsize, struct esp_video_stream *stream)
{
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    enum uvc_host_stream_format uvc_format;
    ESP_RETURN_ON_ERROR(v4l2_to_uvc_format(frmsize->pixel_format, &uvc_format), TAG, "Failed to convert V4L2 format to UVC format");

    int uvc_fmt_resolution_index = -1;
    for (int i = 0; i < device->frame_info_num; i++) {
        if (device->frame_info[i].format == uvc_format) {
            uvc_fmt_resolution_index++;

            if (uvc_fmt_resolution_index == frmsize->index) {
                frmsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
                frmsize->discrete.width = device->frame_info[i].h_res;
                frmsize->discrete.height = device->frame_info[i].v_res;
                ret = ESP_OK;
                break;
            }
        }
    }

    return ret;
}

static esp_err_t uvc_video_enum_frameintervals(struct esp_video *video, struct v4l2_frmivalenum *frmival, struct esp_video_stream *stream)
{
    struct uvc_video *device = VIDEO_PRIV_DATA(struct uvc_video *, video);
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    ESP_RETURN_ON_FALSE(device->dev_addr, ESP_ERR_NOT_FOUND, TAG, "UVC device=%p is not connected", device);

    enum uvc_host_stream_format uvc_format;
    ESP_RETURN_ON_ERROR(v4l2_to_uvc_format(frmival->pixel_format, &uvc_format), TAG, "Failed to convert V4L2 format to UVC format");

    for (int i = 0; i < device->frame_info_num; i++) {
        if ((device->frame_info[i].format == uvc_format) &&
                (device->frame_info[i].h_res == frmival->width) &&
                (device->frame_info[i].v_res == frmival->height)) {
            if (!device->frame_info[i].interval_type) {
                if (frmival->index != 0) {
                    return ESP_ERR_INVALID_ARG;
                }

                frmival->type = V4L2_FRMIVAL_TYPE_STEPWISE;
                frmival->stepwise.min.numerator = device->frame_info[i].interval_min;
                frmival->stepwise.min.denominator = UVC_INTERVAL_DENOMINATOR;
                frmival->stepwise.max.numerator = device->frame_info[i].interval_max;
                frmival->stepwise.max.denominator = UVC_INTERVAL_DENOMINATOR;
                frmival->stepwise.step.numerator = device->frame_info[i].interval_step;
                frmival->stepwise.step.denominator = UVC_INTERVAL_DENOMINATOR;
                ret = ESP_OK;
                break;
            } else {
                if (frmival->index >= device->frame_info[i].interval_type) {
                    return ESP_ERR_INVALID_ARG;
                }

                frmival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
                frmival->discrete.numerator = device->frame_info[i].interval[frmival->index];
                frmival->discrete.denominator = UVC_INTERVAL_DENOMINATOR;
                ret = ESP_OK;
                break;
            }
        }
    }
    return ret;
}

static const struct esp_video_ops s_uvc_video_ops = {
    .init          = uvc_video_init,
    .deinit        = uvc_video_deinit,
    .start         = uvc_video_start,
    .stop          = uvc_video_stop,
    .enum_format   = uvc_video_enum_format,
    .set_format    = uvc_video_set_format,
    .notify        = uvc_video_notify,
    .set_parm      = uvc_video_set_parm,
    .get_parm      = uvc_video_get_parm,
    .enum_framesizes = uvc_video_enum_framesizes,
    .enum_frameintervals = uvc_video_enum_frameintervals,
    .set_ext_ctrl   = uvc_video_set_ext_ctrl,
    .get_ext_ctrl   = uvc_video_get_ext_ctrl,
    .query_ext_ctrl = uvc_video_query_ext_ctrl,
};

esp_err_t esp_video_install_usb_uvc_driver(const esp_video_usb_uvc_device_config_t *cfg)
{
    esp_err_t ret;
    struct uvc_video_core *core;
    struct esp_video *video[ESP_VIDEO_USB_UVC_DEVICE_ID_NUM] = {0};
    uint32_t device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_STREAMING | V4L2_CAP_TIMEPERFRAME;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    ESP_RETURN_ON_FALSE(cfg->uvc_dev_num > 0 && cfg->uvc_dev_num <= ESP_VIDEO_USB_UVC_DEVICE_ID_NUM, ESP_ERR_INVALID_ARG, TAG, "uvc_dev_num is out of range");

    core = heap_caps_calloc(1, sizeof(struct uvc_video_core) + sizeof(struct uvc_video) * cfg->uvc_dev_num, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(core, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for uvc_video_core");

    core->uvc_video_num = cfg->uvc_dev_num;

    for (int i = 0; i < cfg->uvc_dev_num; i++) {
        char name[12];

        assert(snprintf(name, sizeof(name), UVC_NAME_PREFIX "%d", i) > 0);

        core->uvc_video[i].ready_sem = xSemaphoreCreateBinary();
        ESP_GOTO_ON_FALSE(core->uvc_video[i].ready_sem, ESP_ERR_NO_MEM, fail0, TAG, "Failed to create UVC device ready semaphore");

        core->uvc_video[i].ctrl_mutex = xSemaphoreCreateMutex();
        ESP_GOTO_ON_FALSE(core->uvc_video[i].ctrl_mutex, ESP_ERR_NO_MEM, fail0, TAG, "Failed to create UVC device control mutex");

        /* Kconfig supplies the starting value of two V4L2 controls, nothing more. Both default
         * to 0, which is "leave the camera's own encoder settings alone" - a camera framework
         * has no business reconfiguring a camera nobody asked it to reconfigure. */
        core->uvc_video[i].h264.bitrate = CONFIG_ESP_VIDEO_UVC_H264_BITRATE_BPS;
        core->uvc_video[i].h264.gop_size = CONFIG_ESP_VIDEO_UVC_H264_GOP_SIZE;
        core->uvc_video[i].ae_priority = -1;

        video[i] = esp_video_create(name, ESP_VIDEO_USB_UVC_DEVICE_ID(i), &s_uvc_video_ops, &core->uvc_video[i], caps, device_caps);
        ESP_GOTO_ON_FALSE(video[i], ESP_ERR_NO_MEM, fail0, TAG, "Failed to create esp_video");
    }

    core->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;

    const uvc_host_driver_config_t driver_config = {
        .driver_task_stack_size = cfg->task_stack,
        .driver_task_priority = cfg->task_priority,
        .xCoreID = cfg->task_affinity >= 0 ? cfg->task_affinity : tskNO_AFFINITY,
        .create_background_task = true,
        .event_cb = uvc_host_driver_event_callback,
        .user_ctx = core,
    };
    ESP_GOTO_ON_ERROR(uvc_host_install(&driver_config), fail0, TAG, "Failed to install UVC host driver");

    s_uvc_video_core = core;

    return ESP_OK;

fail0:
    /* Not gated on video[i]: the device whose creation failed still has the semaphores that
     * were created for it a few lines earlier. */
    for (int i = 0; i < cfg->uvc_dev_num; i++) {
        if (video[i]) {
            esp_video_destroy(video[i]);
        }

        if (core->uvc_video[i].ready_sem) {
            vSemaphoreDelete(core->uvc_video[i].ready_sem);
        }

        if (core->uvc_video[i].ctrl_mutex) {
            vSemaphoreDelete(core->uvc_video[i].ctrl_mutex);
        }
    }
    free(core);
    return ret;
}

esp_err_t esp_video_uninstall_usb_uvc_driver(void)
{
    if (s_uvc_video_core) {
        struct uvc_video_core *core = s_uvc_video_core;

        for (int i = 0; i < core->uvc_video_num; i++) {
            struct esp_video *video;
            char name[12];

            assert(snprintf(name, sizeof(name), UVC_NAME_PREFIX "%d", i) > 0);

            video = esp_video_device_get_object(name);
            if (!video) {
                ESP_LOGW(TAG, "UVC device %s not found", name);
                return ESP_FAIL;
            }

            esp_video_destroy(video);

            if (core->uvc_video[i].ready_sem) {
                vSemaphoreDelete(core->uvc_video[i].ready_sem);
            }

            if (core->uvc_video[i].ctrl_mutex) {
                vSemaphoreDelete(core->uvc_video[i].ctrl_mutex);
            }
        }

        free(s_uvc_video_core);
        s_uvc_video_core = NULL;
    }

    return uvc_host_uninstall();
}
