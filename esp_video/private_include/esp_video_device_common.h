/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_video.h"
#include "esp_video_cam.h"
#include "esp_video_internal.h"
#include "esp_cam_ctlr.h"
#include "hal/cam_ctlr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameters filled by start_init_config for common to configure the video buffer.
 */
typedef struct esp_video_device_common_init_data {
    uint32_t v4l2_format;   /*!< Output pixel format. 0 = common converts from sensor format */
    uint32_t sizeimage;     /*!< Buffer size hint.    0 = auto-calculate from w*h*bpp */
} esp_video_device_common_init_data_t;

/**
 * @brief Common video device structure
 */
struct esp_video_device_common;

/**
 * @brief Device-specific interface hooks.
 */
typedef struct esp_video_device_intf {
    esp_err_t (*init)(struct esp_video_device_common *common);

    /**
     * Device inspects sensor_fmt, sets device-specific state, and
     * returns config params (esp_video_device_common_init_data_t) for common.
     */
    esp_err_t (*start_init_config)(struct esp_video_device_common *common, esp_video_device_common_init_data_t *config);

    esp_err_t (*deinit)(struct esp_video_device_common *common);
    esp_err_t (*start)(struct esp_video_device_common *common, esp_cam_ctlr_handle_t *cam_ctrl_handle_ret);
    esp_err_t (*stop)(struct esp_video_device_common *common);

    esp_err_t (*enum_format)(struct esp_video_device_common *common, uint32_t index, uint32_t *pixel_format);

    /**
     * Called by common_video_set_format AFTER resolution check passes.
     * Device validates pixelformat and does extra config (e.g. ISP, sizeimage).
     * Return ESP_OK to accept, error to reject.
     * NULL → common uses default pixelformat check.
     */
    esp_err_t (*check_set_format)(struct esp_video_device_common *common, const struct v4l2_format *format);

    esp_err_t (*reprocess)(struct esp_video_device_common *common, uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size, size_t *dst_out_size);

    /**
     * Called by common on_get_new_trans BEFORE fetching the next queued buffer.
     * Device can perform per-frame preparation (e.g. swap_byte_start).
     * NULL → no preparation. Must be IRAM-safe.
     */
    void (*prepare_on_get_new_trans)(struct esp_video_device_common *common);

    esp_err_t (*set_selection)(struct esp_video_device_common *common, struct v4l2_selection *selection);

    /**
     * Called by common_video_enum_framesizes AFTER index==0 check.
     * Device validates pixel_format (e.g. CSI checks supported output formats).
     * NULL → common checks pixel_format == current format.
     */
    esp_err_t (*check_enum_framesizes)(struct esp_video_device_common *common, struct v4l2_frmsizeenum *frmsize);
} esp_video_device_intf_t;

/**
 * @brief Configuration structure for esp_video_device_common_create.
 */
typedef struct esp_video_device_common_config {
    const char *name;                               /*!< Device name */
    uint8_t id;                                     /*!< Device ID */
    void *priv;                                     /*!< Private data pointer to device-specific structure, device-specific structure will be allocated by the caller and will be passed to esp_video_device_common_create and esp_video_device_common_free */
    const esp_video_device_intf_t *intf;            /*!< Device interface pointer */
    esp_video_cam_t cam;                            /*!< Video camera device */
    uint32_t mem_caps;                              /*!< Memory capabilities */
    bool use_backup_element;                        /*!< true: when no queued buffer, reuse last element instead of failing */
} esp_video_device_common_config_t;

/**
 * @brief Common device structure — MUST be first member of every device struct.
 */
typedef struct esp_video_device_common {
    const esp_video_device_intf_t *intf;            /*!< Device interface pointer */
    esp_video_cam_t cam;                            /*!< Video camera device */
    uint32_t mem_caps;                              /*!< Memory capabilities */
    size_t buf_alignment;                           /*!< Buffer alignment */
    const esp_cam_sensor_format_t *sensor_format;   /*!< Sensor format */
    cam_ctlr_color_t in_color;                      /*!< Input color */

    bool use_backup_element;                        /*!< true: when no queued buffer, reuse last element instead of failing */
    struct esp_video_buffer_element *backup_element;    /*!< Runtime: current backup element (managed by common callbacks) */

    esp_cam_ctlr_handle_t cam_ctrl_handle;          /*!< Camera controller handle */

    struct esp_video *video;                        /*!< Pointer to the esp_video instance */
    void *priv;                                     /*!< Private data pointer to device-specific structure, device-specific structure will be allocated by the caller and will be passed to esp_video_device_common_create and esp_video_device_common_free */
} esp_video_device_common_t;

#define VIDEO_DEVICE_COMMON(video)                      VIDEO_PRIV_DATA(esp_video_device_common_t *, video)
#define VIDEO_DEVICE_CAM(video)                         (&VIDEO_DEVICE_COMMON(video)->cam)

/**
 * @brief Create a common video device
 *
 * @param config Configuration structure for esp_video_device_common_create
 * @param common_ret Pointer to the common video device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_device_common_create(const esp_video_device_common_config_t *config, esp_video_device_common_t **common_ret);

/**
 * @brief Free a common video device
 *
 * @param common Pointer to the common video device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_device_common_free(esp_video_device_common_t *common);


/**
 * @brief Get the private data pointer from the common video device
 *
 * @param name Device name
 * @param priv Pointer to the private data pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_device_common_get_priv(const char *name, void **priv);

/**
 * @brief Get the camera sensor from the common video device
 *
 * @param name Device name
 * @param sensor Pointer to the video camera device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_device_common_get_video_cam(const char *name, esp_video_cam_t *sensor);

/**
 * @brief Callback function for the transport finished event
 *
 * @param handle Handle to the transport
 * @param trans Transport structure
 * @param user_data User data
 *
 * @return
 *      - true if the transport is finished
 *      - false if the transport is not finished
 */
bool esp_video_device_common_on_trans_finished(esp_cam_ctlr_handle_t handle,
        esp_cam_ctlr_trans_t *trans, void *user_data);

/**
 * @brief Callback function for the new transport event
 *
 * @param handle Handle to the transport
 * @param trans Transport structure
 * @param user_data User data
 *
 * @return
 *      - true if the new transport is queued
 *      - false if the new transport is not queued
 */
bool esp_video_device_common_on_get_new_trans(esp_cam_ctlr_handle_t handle,
        esp_cam_ctlr_trans_t *trans, void *user_data);

/**
 * Query extended control information for video device
 *
 * This function implements the VIDIOC_QUERY_EXT_CTRL ioctl handler for V4L2.
 * It supports both direct ID lookup and NEXT_CTRL iteration.
 *
 * @param qctrl_table Pointer to v4l2_query_ext_ctrl table
 * @param qctrl_table_size Size of v4l2_query_ext_ctrl table
 * @param qctrl Pointer to v4l2_query_ext_ctrl structure (in/out parameter)
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t esp_video_device_common_query_ext_ctrl(const struct v4l2_query_ext_ctrl *qctrl_table, int qctrl_table_size, struct v4l2_query_ext_ctrl *qctrl);

#ifdef __cplusplus
}
#endif
