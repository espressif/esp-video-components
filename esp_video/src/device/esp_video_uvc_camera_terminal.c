/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Camera Terminal controls, reached over the generic unit-control API in usb_host_uvc.
 *
 * These are UVC 1.5 core controls on the sensor side of the pipeline, nothing to do with
 * the H.264 payload specification or with any extension unit. They live here rather than in
 * the class driver because deciding when to move one is application policy: the driver
 * supplies the mechanism (find the terminal, check its capability bits, issue the request)
 * and the caller decides what the camera should be doing.
 */

#include <inttypes.h>

#include "esp_log.h"
#include "esp_check.h"

#include "usb/uvc_host.h"
#include "esp_video_uvc_controls.h"

static const char *TAG = "uvc_camera_ct";

/* Camera Terminal, UVC 1.5 section 4.2.2.1. Auto-Exposure Priority is the only control that
 * decides frame rate versus exposure, and that trade is upstream of the encoder:
 *
 *   0 = constant frame rate. Auto-exposure may not lengthen exposure past the frame interval,
 *       so a dim scene comes back dark and noisy at full rate.
 *   1 = frame rate may vary. Auto-exposure lengthens exposure for a correct picture and the
 *       sensor rate falls to suit - the UVC default, and why a camera indoors after dark
 *       quietly delivers half its nominal fps.
 */
#define UVC_CT_AE_PRIORITY_CONTROL 0x03

/* Resolve the Camera Terminal and confirm it claims AE Priority. Checking the descriptor first
 * matters: a camera that does not implement the control answers with a STALL, and the USB host
 * library logs that at ERROR, so an unconditional write puts an error in the log of every boot
 * on every camera that simply lacks it. */
static esp_err_t ct_find_ae_priority(uvc_host_stream_hdl_t stream_hdl, uint8_t *terminal_id)
{
    bool has_ae_priority = false;

    ESP_RETURN_ON_ERROR(uvc_host_stream_find_terminal(stream_hdl, UVC_HOST_ITT_CAMERA, terminal_id),
                        TAG, "No Camera Terminal on this camera");

    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_supports_control(stream_hdl, *terminal_id,
                                                              ESP_VIDEO_UVC_CT_AE_PRIORITY_BIT,
                                                              &has_ae_priority),
                        TAG, "Could not read Camera Terminal controls");
    if (!has_ae_priority) {
        ESP_LOGD(TAG, "Camera Terminal %u does not implement AE Priority - the sensor frame "
                 "rate is not controllable on this camera", *terminal_id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

esp_err_t esp_video_uvc_camera_set_ae_priority(uvc_host_stream_hdl_t stream_hdl, uint8_t priority,
                                               uint8_t *committed)
{
    ESP_RETURN_ON_FALSE(stream_hdl, ESP_ERR_INVALID_ARG, TAG, "stream_hdl is NULL");
    ESP_RETURN_ON_FALSE(priority <= 1, ESP_ERR_INVALID_ARG, TAG, "priority must be 0 or 1");

    uint8_t terminal_id = 0;

    ESP_RETURN_ON_ERROR(ct_find_ae_priority(stream_hdl, &terminal_id), TAG, "AE Priority unavailable");

    uint8_t value = priority;
    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, terminal_id, UVC_CT_AE_PRIORITY_CONTROL,
                                                  UVC_HOST_REQ_SET_CUR, &value, sizeof(value)),
                        TAG, "AE Priority write rejected");

    /* Read back rather than trust the write */
    value = 0xff;
    ESP_RETURN_ON_ERROR(uvc_host_stream_unit_ctrl(stream_hdl, terminal_id, UVC_CT_AE_PRIORITY_CONTROL,
                                                  UVC_HOST_REQ_GET_CUR, &value, sizeof(value)),
                        TAG, "AE Priority read-back failed");

    ESP_LOGD(TAG, "AE Priority on Camera Terminal %u: asked %u, committed %u (%s)",
             terminal_id, priority, value,
             value == 0 ? "constant frame rate - exposure gives way"
             : "frame rate may vary - exposure wins");
    if (value != priority) {
        ESP_LOGW(TAG, "camera kept AE Priority %u - it refused the request", value);
    }
    if (committed != NULL) {
        *committed = value;
    }
    return ESP_OK;
}

esp_err_t esp_video_uvc_camera_get_ae_priority(uvc_host_stream_hdl_t stream_hdl, uint8_t *priority)
{
    ESP_RETURN_ON_FALSE(stream_hdl && priority, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    uint8_t terminal_id = 0;

    ESP_RETURN_ON_ERROR(ct_find_ae_priority(stream_hdl, &terminal_id), TAG, "AE Priority unavailable");

    return uvc_host_stream_unit_ctrl(stream_hdl, terminal_id, UVC_CT_AE_PRIORITY_CONTROL,
                                     UVC_HOST_REQ_GET_CUR, priority, sizeof(*priority));
}
