/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"

#include "esp_private/esp_ldo.h"
#include "driver/esp_isp.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"

#include "esp_video.h"
#include "esp_color_formats.h"
#include "mipi_csi_video.h"

#define CSI_NAME                    "MIPI-CSI"
#define CSI_INIT_FORMAT             PIXFORMAT_RGB565
#define CSI_INIT_FORMAT_BPP         16

#define CSI_LDO_UNIT_ID             3
#define CSI_LDO_CFG_VOL_MV          2500

#define CSI_CTRL_ID                 0
#define CSI_CLK_SRC                 MIPI_CSI_PHY_CLK_SRC_DEFAULT
#define CSI_CLK_FREQ_HZ             (200 * 1000 * 1000)
#define CSI_BYTE_SWAP_EN            false
#define CSI_QUEUE_ITEMS             1

#define CSI_DMA_ALIGN_BYTES         64
#define CSI_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM)

#define ISP_CLK_SRC                 ISP_CLK_SRC_DEFAULT
#define ISP_CLK_FREQ_HZ             (80 * 1000 * 1000)
#define ISP_INPUT_DATA_SRC          ISP_INPUT_DATA_SOURCE_CSI
#define ISP_HAS_LINE_START_PACKET   false
#define ISP_HAS_LINE_END_PACKET     false

struct cam_ctrl {
    esp_video_cam_intf_t intf;

    esp_cam_ctlr_handle_t cam_ctrl_handle;
    esp_ldo_unit_handle_t ldo_handle;

    esp_isp_processor_t isp_processor;
};

static const char *TAG = "cam_ctrl";

static esp_err_t csi_get_input_frame_type(uint32_t sensor_fmt, mipi_csi_color_t *csi_fmt)
{
    esp_err_t ret = ESP_OK;

    switch (sensor_fmt) {
    case CAM_SENSOR_PIXFORMAT_RAW8:
        *csi_fmt = MIPI_CSI_COLOR_RAW8;
        break;
    case CAM_SENSOR_PIXFORMAT_RAW10:
        *csi_fmt = MIPI_CSI_COLOR_RAW10;
        break;
    case CAM_SENSOR_PIXFORMAT_RAW12:
        *csi_fmt = MIPI_CSI_COLOR_RAW12;
        break;
    case CAM_SENSOR_PIXFORMAT_RGB565:
        *csi_fmt = MIPI_CSI_COLOR_RGB565;
        break;
    case CAM_SENSOR_PIXFORMAT_RGB888:
        *csi_fmt = MIPI_CSI_COLOR_RGB888;
        break;
    case CAM_SENSOR_PIXFORMAT_YUV420:
        *csi_fmt = MIPI_CSI_COLOR_YUV420;
        break;
    case CAM_SENSOR_PIXFORMAT_YUV422:
        *csi_fmt = MIPI_CSI_COLOR_YUV422;
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}

static esp_err_t csi_get_output_frame_type(uint32_t output_fmt, mipi_csi_color_t *csi_fmt)
{
    esp_err_t ret = ESP_OK;

    switch (output_fmt) {
    case PIXFORMAT_RAW8:
        *csi_fmt = MIPI_CSI_COLOR_RAW8;
        break;
    case PIXFORMAT_RAW10:
        *csi_fmt = MIPI_CSI_COLOR_RAW10;
        break;
    case PIXFORMAT_RAW12:
        *csi_fmt = MIPI_CSI_COLOR_RAW12;
        break;
    case PIXFORMAT_RGB565:
        *csi_fmt = MIPI_CSI_COLOR_RGB565;
        break;
    case PIXFORMAT_RGB888:
        *csi_fmt = MIPI_CSI_COLOR_RGB888;
        break;
    case PIXFORMAT_YUV420:
        *csi_fmt = MIPI_CSI_COLOR_YUV420;
        break;
    case PIXFORMAT_YUV422:
        *csi_fmt = MIPI_CSI_COLOR_YUV422;
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}

static esp_err_t isp_get_input_frame_type(uint32_t sensor_fmt, isp_color_t *isp_fmt)
{
    esp_err_t ret = ESP_OK;

    switch (sensor_fmt) {
    case CAM_SENSOR_PIXFORMAT_RAW8:
        *isp_fmt = ISP_COLOR_RAW8;
        break;
    case CAM_SENSOR_PIXFORMAT_RAW10:
        *isp_fmt = ISP_COLOR_RAW10;
        break;
    case CAM_SENSOR_PIXFORMAT_RAW12:
        *isp_fmt = ISP_COLOR_RAW12;
        break;
    case CAM_SENSOR_PIXFORMAT_RGB565:
        *isp_fmt = ISP_COLOR_RGB565;
        break;
    case CAM_SENSOR_PIXFORMAT_RGB888:
        *isp_fmt = ISP_COLOR_RGB888;
        break;
    case CAM_SENSOR_PIXFORMAT_YUV420:
        *isp_fmt = ISP_COLOR_YUV420;
        break;
    case CAM_SENSOR_PIXFORMAT_YUV422:
        *isp_fmt = ISP_COLOR_YUV422;
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}

static esp_err_t isp_get_output_frame_type(uint32_t output_fmt, isp_color_t *isp_fmt)
{
    esp_err_t ret = ESP_OK;

    switch (output_fmt) {
    case PIXFORMAT_RAW8:
        *isp_fmt = ISP_COLOR_RAW8;
        break;
    case PIXFORMAT_RAW10:
        *isp_fmt = ISP_COLOR_RAW10;
        break;
    case PIXFORMAT_RAW12:
        *isp_fmt = ISP_COLOR_RAW12;
        break;
    case PIXFORMAT_RGB565:
        *isp_fmt = ISP_COLOR_RGB565;
        break;
    case PIXFORMAT_RGB888:
        *isp_fmt = ISP_COLOR_RGB888;
        break;
    case PIXFORMAT_YUV420:
        *isp_fmt = ISP_COLOR_YUV420;
        break;
    case PIXFORMAT_YUV422:
        *isp_fmt = ISP_COLOR_YUV422;
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}

static esp_err_t csi_get_data_lane(sensor_port_t port, uint8_t *data_lane_num)
{
    esp_err_t ret = ESP_OK;

    switch (port) {
    case MIPI_CSI_OUTPUT_LANE1:
        *data_lane_num = 1;
        break;
    case MIPI_CSI_OUTPUT_LANE2:
        *data_lane_num = 2;
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}

static bool IRAM_ATTR cam_ctrl_on_trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    struct esp_video *video = (struct esp_video *)user_data;

    ESP_LOGD(TAG, "size=%zu", trans->received_size);

    CAPTURE_VIDEO_DONE_BUF(video, trans->buffer, trans->received_size);

    return true;
}

static bool IRAM_ATTR cam_ctrl_on_get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    struct esp_video_buffer_element *element;
    struct esp_video *video = (struct esp_video *)user_data;

    element = CAPTURE_VIDEO_GET_QUEUED_ELEMENT(video);
    if (!element) {
        return false;
    }

    trans->buffer = element->buffer;
    trans->buflen = ELEMENT_SIZE(element);

    return true;
}

static esp_err_t cam_ctrl_csi_init(struct esp_video *video)
{
    esp_err_t ret;
    uint32_t buf_size;
    sensor_format_t sensor_format;
    esp_cam_ctlr_evt_cbs_t cam_ctrl_cbs = { 0 };
    esp_ldo_unit_init_cfg_t ldo_cfg = { 0 };
    esp_isp_processor_cfg_t isp_config = { 0 };
    esp_cam_ctlr_csi_config_t csi_config = { 0 };
    struct cam_ctrl *cam_ctrl = VIDEO_PRIV_DATA(struct cam_ctrl *, video);

    ret = esp_camera_get_format(video->cam_dev, &sensor_format);
    if (ret != ESP_OK) {
        return ret;
    }

    CAPTURE_VIDEO_SET_FORMAT(video,
                             sensor_format.fps,
                             sensor_format.width,
                             sensor_format.height,
                             CSI_INIT_FORMAT,
                             CSI_INIT_FORMAT_BPP);

    ret = csi_get_data_lane(sensor_format.port, &csi_config.data_lane_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get CSI data lane number");
        return ret;
    }

    ret = csi_get_input_frame_type(sensor_format.format, &csi_config.input_data_color_type);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get CSI input frame type");
        return ret;
    }
    ret = csi_get_output_frame_type(CSI_INIT_FORMAT, &csi_config.output_data_color_type);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get CSI output frame type");
        return ret;
    }

    ret = isp_get_input_frame_type(sensor_format.format, &isp_config.input_data_color_type);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get ISP input frame type");
        return ret;
    }
    ret = isp_get_output_frame_type(CSI_INIT_FORMAT, &isp_config.output_data_color_type);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get ISP output frame type");
        return ret;
    }

    ldo_cfg.unit_id = CSI_LDO_UNIT_ID;
    ldo_cfg.cfg.voltage_mv = CSI_LDO_CFG_VOL_MV;
    ldo_cfg.flags.enable_unit = true;
    ldo_cfg.flags.shared_ldo = true;
    ret = esp_ldo_init_unit(&ldo_cfg, &cam_ctrl->ldo_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to init LDO");
        return ESP_FAIL;
    }

    csi_config.ctlr_id = CSI_CTRL_ID;
    csi_config.clk_src = CSI_CLK_SRC;
    csi_config.byte_swap_en = CSI_BYTE_SWAP_EN;
    csi_config.queue_items = CSI_QUEUE_ITEMS;
    csi_config.h_res = sensor_format.width;
    csi_config.v_res = sensor_format.height;
    if (sensor_format.mipi_info.mipi_clk) {
        csi_config.clk_freq_hz = sensor_format.mipi_info.mipi_clk;
    } else {
        csi_config.clk_freq_hz = CSI_CLK_FREQ_HZ;
    }
    ret = esp_cam_new_csi_ctlr(&csi_config, &cam_ctrl->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create CSI");
        goto errout_new_csi;
    }

    cam_ctrl_cbs.on_get_new_trans = cam_ctrl_on_get_new_trans;
    cam_ctrl_cbs.on_trans_finished = cam_ctrl_on_trans_finished;
    ret = esp_cam_ctlr_register_event_callbacks(cam_ctrl->cam_ctrl_handle, &cam_ctrl_cbs, video);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register CAM ctlr event callback");
        goto errout_register_cam_ctrl_cb;
    }

    ret = esp_cam_ctlr_enable(cam_ctrl->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable CAM ctlr");
        goto errout_register_cam_ctrl_cb;
    }

    buf_size = sensor_format.width * sensor_format.height * CSI_INIT_FORMAT_BPP / 8;
    ESP_LOGI(TAG, "buffer size=%" PRIu32, buf_size);

    CAPTURE_VIDEO_SET_BUF_INFO(video, buf_size, CSI_DMA_ALIGN_BYTES, CSI_MEM_CAPS);

    isp_config.clk_src = ISP_CLK_SRC;
    isp_config.input_data_source = ISP_INPUT_DATA_SRC;
    isp_config.has_line_start_packet = ISP_HAS_LINE_START_PACKET;
    isp_config.has_line_end_packet = ISP_HAS_LINE_END_PACKET;
    isp_config.h_res = sensor_format.width;
    isp_config.v_res = sensor_format.height;
    isp_config.clk_hz = ISP_CLK_FREQ_HZ;
    ret = esp_isp_new_processor(&isp_config, &cam_ctrl->isp_processor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create ISP");
        goto errout_new_isp;
    }

    return ESP_OK;

errout_new_isp:
    esp_cam_ctlr_disable(cam_ctrl->cam_ctrl_handle);
errout_register_cam_ctrl_cb:
    esp_cam_del_ctlr(cam_ctrl->cam_ctrl_handle);
    cam_ctrl->cam_ctrl_handle = NULL;
errout_new_csi:
    esp_ldo_disable_unit(cam_ctrl->ldo_handle);
    esp_ldo_deinit_unit(cam_ctrl->ldo_handle);
    cam_ctrl->ldo_handle = NULL;
    return ret;
}

static esp_err_t cam_ctrl_csi_deinit(struct esp_video *video)
{
    struct cam_ctrl *cam_ctrl = VIDEO_PRIV_DATA(struct cam_ctrl *, video);

    esp_isp_disable(cam_ctrl->isp_processor);
    esp_isp_del_processor(cam_ctrl->isp_processor);
    cam_ctrl->isp_processor = NULL;

    esp_cam_ctlr_stop(cam_ctrl->cam_ctrl_handle);
    esp_cam_ctlr_disable(cam_ctrl->cam_ctrl_handle);
    esp_cam_del_ctlr(cam_ctrl->cam_ctrl_handle);
    cam_ctrl->cam_ctrl_handle = NULL;

    esp_ldo_disable_unit(cam_ctrl->ldo_handle);
    esp_ldo_deinit_unit(cam_ctrl->ldo_handle);
    cam_ctrl->ldo_handle = NULL;

    return ESP_OK;
}

static esp_err_t cam_ctrl_video_init(struct esp_video *video)
{
    esp_err_t ret = ESP_FAIL;
    struct cam_ctrl *cam_ctrl = VIDEO_PRIV_DATA(struct cam_ctrl *, video);

    if (cam_ctrl->intf == ESP_VIDEO_CAM_INTF_MIPI_CSI) {
        ret = cam_ctrl_csi_init(video);
    }

    return ret;
}

static esp_err_t cam_ctrl_video_deinit(struct esp_video *video)
{
    esp_err_t ret = ESP_FAIL;
    struct cam_ctrl *cam_ctrl = VIDEO_PRIV_DATA(struct cam_ctrl *, video);

    if (cam_ctrl->intf == ESP_VIDEO_CAM_INTF_MIPI_CSI) {
        ret = cam_ctrl_csi_deinit(video);
    }

    return ret;
}

static esp_err_t cam_ctrl_video_start(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    int flags = 1;
    esp_camera_device_t *cam_dev = VIDEO_CAM_DEV(video);
    struct cam_ctrl *cam_ctrl = VIDEO_PRIV_DATA(struct cam_ctrl *, video);

    ret = esp_isp_enable(cam_ctrl->isp_processor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable ISP");
        return ret;
    }

    ret = esp_cam_ctlr_start(cam_ctrl->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start CAM ctlr");
        goto errout_start_cam_ctlr;
    }

    ret = esp_camera_ioctl(cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start sensor");
        goto errout_start_cam;
    }

    return ESP_OK;

errout_start_cam:
    esp_cam_ctlr_stop(cam_ctrl->cam_ctrl_handle);
errout_start_cam_ctlr:
    esp_isp_disable(cam_ctrl->isp_processor);
    return ret;
}

static esp_err_t cam_ctrl_video_stop(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    int flags = 0;
    esp_camera_device_t *cam_dev = VIDEO_CAM_DEV(video);
    struct cam_ctrl *cam_ctrl = VIDEO_PRIV_DATA(struct cam_ctrl *, video);

    ret = esp_camera_ioctl(cam_dev, CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to disable sensor");
        return ret;
    }

    ret = esp_cam_ctlr_stop(cam_ctrl->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to disable CAM ctlr");
        return ret;
    }

    ret = esp_isp_disable(cam_ctrl->isp_processor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to disable ISP");
    }

    return ret;
}

static esp_err_t cam_ctrl_video_set_format(struct esp_video *video, uint32_t type, const struct esp_video_format *format)
{
    return ESP_OK;
}

static esp_err_t cam_ctrl_video_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    if (!capability) {
        ESP_LOGE(TAG, "capability=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(capability, 0, sizeof(struct esp_video_capability));

    return ESP_OK;
}

static esp_err_t cam_ctrl_video_description(struct esp_video *video, char *buffer, uint32_t size)
{
    int ret;

    ret = snprintf(buffer, size, "MIPI-CSI Interface Camera\n");
    if (ret <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t cam_ctrl_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    return ESP_OK;
}

static const struct esp_video_ops s_cam_ctrl_video_ops = {
    .init          = cam_ctrl_video_init,
    .deinit        = cam_ctrl_video_deinit,
    .start         = cam_ctrl_video_start,
    .stop          = cam_ctrl_video_stop,
    .set_format    = cam_ctrl_video_set_format,
    .capability    = cam_ctrl_video_capability,
    .description   = cam_ctrl_video_description,
    .notify        = cam_ctrl_video_notify,
};

/**
 * @brief Create camera controller video device
 *
 * @param cam_dev camera sensor devcie
 * @param intf    camera controller interface
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_cam_device(esp_camera_device_t *cam_dev, esp_video_cam_intf_t intf)
{
    char *name = NULL;
    struct esp_video *video;
    struct cam_ctrl *cam_ctrl;
    uint32_t device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE | V4L2_CAP_EXT_PIX_FORMAT |
                           V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    if (intf != ESP_VIDEO_CAM_INTF_MIPI_CSI) {
        return ESP_ERR_INVALID_ARG;
    }

    cam_ctrl = heap_caps_calloc(1, sizeof(struct cam_ctrl), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!cam_ctrl) {
        return ESP_ERR_NO_MEM;
    }

    cam_ctrl->intf = intf;
    if (intf == ESP_VIDEO_CAM_INTF_MIPI_CSI) {
        name = CSI_NAME;
    }

    video = esp_video_create(name, cam_dev, &s_cam_ctrl_video_ops, cam_ctrl, caps, device_caps);
    if (!video) {
        heap_caps_free(cam_ctrl);
        return ESP_FAIL;
    }

    return ESP_OK;
}
