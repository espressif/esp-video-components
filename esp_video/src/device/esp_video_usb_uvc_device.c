/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "esp_log.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_video.h"
#include "esp_video_device.h"
#include "esp_video_device_usb_uvc.h"
#include "usb/uvc_host.h"
#include "esp_private/uvc_esp_video.h"

static const char *TAG = "esp_video_usb_uvc";
static const char *UVC_NAME = "USB-UVC";

#if CONFIG_SPIRAM
#define FRAME_MEM_CAPS MALLOC_CAP_SPIRAM
#else
#define FRAME_MEM_CAPS MALLOC_CAP_DEFAULT
#endif

#define UVC_VIDEO_MAX_FORMATS 3 // USB cameras have typically 2. Allowing 3 for future expansion

/**
 * @brief esp_video device of type USB-UVC
 *
 * All data to map esp_video device to USB-UVC device
 */
struct uvc_video {
    uvc_host_stream_hdl_t uvc_dev; // Handle to usb_host_uvc device
    enum uvc_host_stream_format format[UVC_VIDEO_MAX_FORMATS];
};

static uint32_t uvc_to_v4l2_format(enum uvc_host_stream_format uvc_format)
{
    switch (uvc_format) {
    case UVC_VS_FORMAT_YUY2:
        return V4L2_PIX_FMT_YUYV;
    case UVC_VS_FORMAT_MJPEG:
        return V4L2_PIX_FMT_JPEG;
    case UVC_VS_FORMAT_H264:
        return V4L2_PIX_FMT_H264;
    case UVC_VS_FORMAT_H265:
        return V4L2_PIX_FMT_HEVC;
    default:
        ESP_LOGE(TAG, "Unsupported pixel format %d", uvc_format);
        return 0; // Invalid format
    }
}

static esp_err_t uvc_video_init(struct esp_video *video)
{
    ESP_LOGD(TAG, "%s called", __func__);

    struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);
    uvc_host_stream_format_t uvc_format;
    uvc_host_buf_info_t uvc_buf_info;

    // Get format and buffer information from the UVC device
    esp_err_t ret = uvc_host_stream_format_get(uvc_video->uvc_dev, &uvc_format);
    ret          |= uvc_host_buf_info_get(uvc_video->uvc_dev, &uvc_buf_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get UVC format");
        return ESP_FAIL;
    }

    // Convert UVC format to V4L2 format
    uint32_t v4l2_format = uvc_to_v4l2_format(uvc_format.format);

    // Save the UVC format and buffer information in the esp_video object
    CAPTURE_VIDEO_SET_FORMAT(video, uvc_format.h_res, uvc_format.v_res, v4l2_format);
    CAPTURE_VIDEO_SET_BUF_INFO(video, uvc_buf_info.dwMaxVideoFrameSize, 4, FRAME_MEM_CAPS);

    return ESP_OK;
}

static esp_err_t uvc_video_start(struct esp_video *video, uint32_t type)
{
    ESP_LOGD(TAG, "%s called", __func__);

    struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);
    return uvc_host_stream_start(uvc_video->uvc_dev);
}

static esp_err_t uvc_video_stop(struct esp_video *video, uint32_t type)
{
    ESP_LOGD(TAG, "%s called", __func__);
    struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);
    return uvc_host_stream_stop(uvc_video->uvc_dev);
}

static esp_err_t uvc_video_deinit(struct esp_video *video)
{
    ESP_LOGD(TAG, "%s called", __func__);
    return ESP_OK;
}

static esp_err_t uvc_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format)
{
    ESP_LOGD(TAG, "%s called, index %d", __func__, index);

    if (index >= UVC_VIDEO_MAX_FORMATS) {
        return ESP_ERR_INVALID_ARG;
    }

    struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);
    enum uvc_host_stream_format uvc_stream_format = uvc_video->format[index];
    if (uvc_stream_format == UVC_VS_FORMAT_DEFAULT) {
        return ESP_ERR_INVALID_ARG;
    }

    *pixel_format = uvc_to_v4l2_format(uvc_video->format[index]);

    return ESP_OK;
}

static esp_err_t uvc_video_set_format(struct esp_video *video, const struct v4l2_format *format)
{
    ESP_LOGD(TAG, "%s called", __func__);
    struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);

    enum uvc_host_stream_format uvc_stream_format;
    switch (GET_FORMAT_PIXEL_FORMAT(format)) {
    case V4L2_PIX_FMT_YUYV:
        uvc_stream_format = UVC_VS_FORMAT_YUY2;
        break;
    case V4L2_PIX_FMT_JPEG:
        uvc_stream_format = UVC_VS_FORMAT_MJPEG;
        break;
    case V4L2_PIX_FMT_H264:
        uvc_stream_format = UVC_VS_FORMAT_H264;
        break;
    case V4L2_PIX_FMT_HEVC:
        uvc_stream_format = UVC_VS_FORMAT_H265;
        break;
    default:
        ESP_LOGE(TAG, "Unsupported pixel format %d", GET_FORMAT_PIXEL_FORMAT(format));
        return ESP_ERR_INVALID_ARG;
    }

    // Set the new UVC format and get new buffer information
    uvc_host_buf_info_t uvc_buf_info;
    uvc_host_stream_format_t f = {
        .h_res = GET_FORMAT_WIDTH(format),
        .v_res = GET_FORMAT_HEIGHT(format),
        .fps = 0, // Select default FPS
        .format = uvc_stream_format,
    };
    esp_err_t ret = uvc_host_stream_format_select(uvc_video->uvc_dev, &f);
    ret          |= uvc_host_buf_info_get(uvc_video->uvc_dev, &uvc_buf_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UVC format");
        return ESP_FAIL;
    }

    // Update esp_video format and buffer size
    CAPTURE_VIDEO_SET_FORMAT(video, f.h_res, f.v_res, GET_FORMAT_PIXEL_FORMAT(format));
    CAPTURE_VIDEO_SET_BUF_INFO(video, uvc_buf_info.dwMaxVideoFrameSize, 4, FRAME_MEM_CAPS);
    return ESP_OK;
}

static esp_err_t uvc_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    ESP_LOGD(TAG, "%s called, event %d", __func__, event);
    switch (event) {
    case ESP_VIDEO_BUFFER_VALID:
    case ESP_VIDEO_M2M_TRIGGER:
    case ESP_VIDEO_DATA_PREPROCESSING:
    default:
        break;
    }

    return ESP_OK;
}

static esp_err_t uvc_video_set_parm(struct esp_video *video, struct v4l2_streamparm *stream_parm, struct esp_video_stream *stream)
{
    ESP_LOGD(TAG, "%s called", __func__);
    struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);
    struct v4l2_fract *time_per_frame = &stream_parm->parm.capture.timeperframe;

    float fps = (float)time_per_frame->denominator / (float)time_per_frame->numerator;

    uvc_host_stream_format_t f = {
        .h_res = 0,  // 0 means do not change resolution
        .v_res = 0,
        .fps = fps,
        .format = 0, // 0 means do not change format
    };
    return uvc_host_stream_format_select(uvc_video->uvc_dev, &f);
}

static esp_err_t uvc_video_get_parm(struct esp_video *video, struct v4l2_streamparm *stream_parm, struct esp_video_stream *stream)
{
    ESP_LOGD(TAG, "%s called", __func__);

    struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);
    struct v4l2_fract *time_per_frame = &stream_parm->parm.capture.timeperframe;
    uvc_host_stream_format_t uvc_format;

    // Get format and buffer information from the UVC device
    esp_err_t ret = uvc_host_stream_format_get(uvc_video->uvc_dev, &uvc_format);
    if (ret != ESP_OK || uvc_format.fps <= 0.0) {
        ESP_LOGE(TAG, "Failed to get UVC format");
        return ESP_FAIL;
    }

    /* Build a v4l2_fract for timeperframe when fps is known
     * to be either integer or integer+0.5. (typical for USb UVC devices)
     * compute twice the fps, rounded to nearest integer */
    unsigned int k = (unsigned int)(uvc_format.fps * 2.0 + 0.5);

    if ((k & 1) == 0) {
        /* even → fps = k/2, so tpf = 1/(k/2) */
        time_per_frame->numerator   = 1;
        time_per_frame->denominator = k >> 1;
    } else {
        /* odd → fps = (k)/2 = N+0.5, so tpf = 2/k */
        time_per_frame->numerator   = 2;
        time_per_frame->denominator = k;
    }

    return ESP_OK;
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
    // .set_ext_ctrl  = uvc_video_set_ext_ctrl,
    // .get_ext_ctrl  = uvc_video_get_ext_ctrl,
    // .query_ext_ctrl = uvc_video_query_ext_ctrl,
    // .set_sensor_format = uvc_video_set_sensor_format,
    // .get_sensor_format = uvc_video_get_sensor_format,
    // .query_menu    = uvc_video_query_menu,
    // .set_selection  = uvc_video_set_selection,
    // .set_motor_format = uvc_video_set_motor_format,
    // .get_motor_format = uvc_video_get_motor_format,
};

static bool uvc_frame_callback(const uvc_host_frame_t *frame, void *user_ctx)
{
    assert(frame);
    assert(user_ctx);
    struct esp_video *video = (struct esp_video *)user_ctx;
    struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);
    assert(uvc_video);

    ESP_LOGD(TAG, "Frame callback! data len: %d", frame->data_len);
    struct esp_video_buffer_element *element;

    element = CAPTURE_VIDEO_GET_QUEUED_ELEMENT(video);
    if (!element) {
        ESP_LOGW(TAG, "Could not get free element");
        return true;
    }

    memcpy(ELEMENT_BUFFER(element), frame->data, frame->data_len);

    esp_err_t ret = CAPTURE_VIDEO_DONE_BUF(video, ELEMENT_BUFFER(element), frame->data_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to pass the frame to esp_video: %s", esp_err_to_name(ret));
        return true; // We will not process this frame, return it immediately
    }
    return true; // We memory copied the frame from UVC to esp_video, we can return it
}

static void uvc_stream_callback(const uvc_host_stream_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
        ESP_LOGE(TAG, "USB error has occurred, err_no = %i", event->transfer_error.error);
        break;
    case UVC_HOST_DEVICE_DISCONNECTED: {
        ESP_LOGI(TAG, "Device suddenly disconnected");
        assert(user_ctx);
        struct esp_video *video = (struct esp_video *)user_ctx;
        struct uvc_video *uvc_video = VIDEO_PRIV_DATA(struct uvc_video *, video);
        assert(uvc_video);

        ESP_ERROR_CHECK(uvc_host_stream_close(event->device_disconnected.stream_hdl));
        free(uvc_video);
        esp_video_destroy(video);
        break;
    }
    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
        ESP_LOGW(TAG, "Frame buffer overflow");
        break;
    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
        ESP_LOGW(TAG, "Frame buffer underflow");
        break;
    default:
        abort();
        break;
    }
}

static void uvc_open_and_register_task(void *arg)
{
    const uvc_host_driver_event_data_t *event = (const uvc_host_driver_event_data_t *)arg;
    int number_of_frame_buffers = 2; //@todo make this configurable

    // Allocate everything we need to register the device
    struct uvc_video *uvc_video = heap_caps_calloc(1, sizeof(struct uvc_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    const uint32_t device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_STREAMING | V4L2_CAP_TIMEPERFRAME;
    const uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;
    struct esp_video *video = esp_video_create(UVC_NAME, ESP_VIDEO_USB_UVC_DEVICE_ID, &s_uvc_video_ops, uvc_video, caps, device_caps);

    if (!uvc_video || !video) {
        if (uvc_video) {
            heap_caps_free(uvc_video);
        }
        if (video) {
            esp_video_destroy(video);
        }
        return; // Refactor error handling in this task
    }

    // event->device_connected.frame_info_num;
    uvc_host_stream_hdl_t stream_hdl = NULL;
    const uvc_host_stream_config_t stream_config = {
        .event_cb = uvc_stream_callback,
        .frame_cb = uvc_frame_callback,
        .user_ctx = video,
        .usb = {
            .dev_addr = event->device_connected.dev_addr,
            .vid = UVC_HOST_ANY_VID,
            .pid = UVC_HOST_ANY_PID,
            .uvc_stream_index = event->device_connected.uvc_stream_index,
        },
        .vs_format = {
            .format = UVC_VS_FORMAT_DEFAULT,
        },
        .advanced = {
            .number_of_frame_buffers = number_of_frame_buffers,
            .frame_size = 250 * 1024,
            .frame_heap_caps = FRAME_MEM_CAPS,
            .number_of_urbs = 3,
            .urb_size = 10 * 1024,
        },
    };

    // Get supported formats from the UVC device and save them in the uvc_video structure
    size_t list_size = 0;
    ESP_ERROR_CHECK(uvc_host_get_frame_list(
                        event->device_connected.dev_addr,
                        event->device_connected.uvc_stream_index,
                        NULL,
                        &list_size));
    uvc_host_frame_info_t *frame_list = malloc(list_size * sizeof(uvc_host_frame_info_t));
    if (frame_list != NULL) {
        int format_index = 0;
        enum uvc_host_stream_format previous_format = UVC_VS_FORMAT_DEFAULT;
        ESP_ERROR_CHECK(uvc_host_get_frame_list(
                            event->device_connected.dev_addr,
                            event->device_connected.uvc_stream_index,
                            (uvc_host_frame_info_t (*)[])frame_list,
                            &list_size));

        for (size_t i = 0; i < list_size; i++) {
            if (frame_list[i].format != previous_format) {
                if (format_index >= UVC_VIDEO_MAX_FORMATS) {
                    ESP_LOGW(TAG, "Too many formats, only first 3 will be saved");
                    break; // We only support 3 formats
                }
                uvc_video->format[format_index++] = frame_list[i].format;
                previous_format = frame_list[i].format;
            }
        }
        free(frame_list);
    }

    // Open the UVC stream
    ESP_ERROR_CHECK(uvc_host_stream_open(&stream_config, 0, &stream_hdl));
    ESP_LOGD(TAG, "Device opened");

    uvc_video->uvc_dev = stream_hdl;

    if (!stream_hdl) {
        heap_caps_free(uvc_video);
        esp_video_destroy(video);
        return; //@todo refactor error handling in this task
    }
    ESP_LOGD(TAG, "Device registered");

    free(arg); // Free the event copy
    vTaskDelete(NULL); // Delete the task
}

static void uvc_host_driver_event_callback(const uvc_host_driver_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED:
        ESP_LOGD(TAG, "Device connected");

        // Create a copy of the event data to pass to the task
        uvc_host_driver_event_data_t *event_copy = malloc(sizeof(uvc_host_driver_event_data_t));
        if (!event_copy) {
            break;
        }
        memcpy(event_copy, event, sizeof(uvc_host_driver_event_data_t));

        // Here we create on-demand task that opens the newly connected device
        // and registers it with the esp_video driver
        // We must do it in a separate task because the event callback is called from the USB client task
        // opening a device from here does not work because the USB client task is blocked
        xTaskCreate(uvc_open_and_register_task, "uvc-open", 3 * 1024, (void *)event_copy, 5, NULL);
        break;
    default:
        break;
    }
}

esp_err_t esp_video_install_usb_uvc_driver(size_t task_stack, unsigned task_priority, int task_affinity)
{
    const uvc_host_driver_config_t driver_config = {
        .driver_task_stack_size = task_stack,
        .driver_task_priority = task_priority,
        .xCoreID = task_affinity,
        .create_background_task = true,
        .event_cb = uvc_host_driver_event_callback,
        .user_ctx = NULL, // user_ctx is not used in the driver now
    };
    return uvc_host_install(&driver_config);
}

esp_err_t esp_video_uninstall_usb_uvc_driver(void)
{
    return uvc_host_uninstall();
}
