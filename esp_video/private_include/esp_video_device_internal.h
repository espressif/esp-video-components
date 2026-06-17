/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_cam_sensor_types.h"
#include "esp_cam_motor_types.h"
#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE
#include "driver/jpeg_encode.h"
#endif
#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_DEC_VIDEO_DEVICE
#include "driver/jpeg_decode.h"
#endif
#if CONFIG_ESP_VIDEO_ENABLE_ISP
#include "driver/isp.h"
#endif
#include "esp_video_device.h"
#include "hal/cam_ctlr_types.h"
#include "esp_cam_ctlr_spi.h"
#include "esp_video_csi_format.h"
#include "esp_video_device.h"
#include "esp_video_caps.h"
#include "linux/videodev2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device name for common video device
 */
#define CSI_NAME                    "MIPI-CSI"
#define DVP_NAME                    "DVP"
#define SPI_NAME                    "SPI"

#ifdef CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
/**
 * @brief SPI video device configuration
 */
typedef struct esp_video_spi_device_config {
    esp_cam_ctlr_spi_cam_intf_t intf;       /*!< SPI CAM interface type */
    esp_cam_ctlr_spi_cam_io_mode_t io_mode; /*!< SPI CAM data I/O mode */
    spi_host_device_t spi_port;             /*!< SPI port */
    gpio_num_t spi_cs_pin;                  /*!< SPI CS pin */
    gpio_num_t spi_sclk_pin;                /*!< SPI SCLK pin */
    gpio_num_t spi_data0_io_pin;            /*!< SPI data0 I/O pin */
    gpio_num_t spi_data1_io_pin;            /*!< SPI data1 I/O pin */
    gpio_num_t spi_data2_io_pin;            /*!< SPI data2 I/O pin */
    gpio_num_t spi_data3_io_pin;            /*!< SPI data3 I/O pin */
} esp_video_spi_device_config_t;
#endif

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
/**
 * @brief USB UVC video device configuration
 */
typedef struct esp_video_usb_uvc_device_config {
    uint32_t uvc_dev_num;                   /*!< USB UVC devices number */

    uint32_t task_stack;                    /*!< USB UVC video device task stack size */
    uint8_t task_priority;                  /*!< USB UVC video device task priority */
    int8_t task_affinity;                   /*!< USB UVC video device task affinity, -1 means no affinity */
} esp_video_usb_uvc_device_config_t;
#endif

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
/**
 * @brief MIPI CSI video device configuration
 */
typedef struct esp_video_csi_device_config {
    bool dont_init_ldo;                     /*!< If true, MIPI-CSI video device will not initialize the LDO; otherwise, MIPI-CSI video device will initialize the LDO */
} esp_video_csi_device_config_t;

/**
 * @brief Create MIPI CSI video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t *cam_dev, const esp_video_csi_device_config_t *config);

/**
 * @brief Destroy MIPI-CSI video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_csi_video_device(void);

/**
 * @brief Add camera motor device to MIPI-CSI video device
 *
 * @param motor_dev camera motor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_csi_video_device_add_motor(esp_cam_motor_device_t *motor_dev);
#endif

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
/**
 * @brief Create DVP video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_dvp_video_device(esp_cam_sensor_device_t *cam_dev);

/**
 * @brief Destroy DVP video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_dvp_video_device(void);
#endif

#ifdef CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
/**
 * @brief Create H.264 video device
 *
 * @param hw_codec true: hardware H.264, false: software H.264(has not supported)
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_h264_video_device(bool hw_codec);

/**
 * @brief Destroy H.264 video device
 *
 * @param hw_codec true: hardware H.264, false: software H.264(has not supported)
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_h264_video_device(bool hw_codec);
#endif

#ifdef CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE
/**
 * @brief Create JPEG encoder video device
 *
 * @param enc_handle JPEG encoder driver handle,
 *      - NULL, JPEG video device will create JPEG encoder driver handle by itself
 *      - Not null, JPEG video device will use this handle instead of creating JPEG encoder driver handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */

esp_err_t esp_video_create_jpeg_enc_video_device(jpeg_encoder_handle_t enc_handle);

/**
 * @brief Destroy JPEG encoder video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_jpeg_enc_video_device(void);
#endif

#ifdef CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_DEC_VIDEO_DEVICE
/**
 * @brief Create JPEG decode video device
 *
 * @param dec_handle JPEG decoder driver handle,
 *      - NULL, JPEG decode video device will create JPEG decoder driver handle by itself
 *      - Not null, JPEG decode video device will use this handle instead of creating JPEG decoder driver handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_jpeg_dec_video_device(jpeg_decoder_handle_t dec_handle);

/**
 * @brief Destroy JPEG decode video device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_jpeg_dec_video_device(void);
#endif

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE

/**
 * @brief Add ISP processor to video device
 *
 * @param isp_proc       ISP processor handle
 * @param width          Image width
 * @param height         Image height
 * @param crop_required  Whether cropping is required
 * @param crop_rect      Crop rectangle (can be NULL if crop_required is false)
 * @param in_out_format  ISP input/output format configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_video_device_add_isp_proc(isp_proc_handle_t isp_proc, uint32_t width, uint32_t height,
        bool crop_required, const struct v4l2_rect *crop_rect, const esp_video_csi_isp_in_out_format_t *in_out_format);

/**
 * @brief Remove ISP processor from video device
 *
 * @param isp_proc       ISP processor handle to remove
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_video_device_remove_isp_proc(isp_proc_handle_t isp_proc);

/**
 * @brief Create ISP video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_isp_video_device(void);

/**
 * @brief Destroy ISP video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_isp_video_device(void);
#endif

#ifdef CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
/**
 * @brief Create SPI video device
 *
 * @param cam_dev       Camera sensor device
 * @param config        SPI video device configuration
 * @param index         SPI video device index
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_spi_video_device(esp_cam_sensor_device_t *cam_dev, const esp_video_spi_device_config_t *config, uint8_t index);

/**
 * @brief Destroy SPI video device
 *
 * @param index         SPI video device index
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_destroy_spi_video_device(uint8_t index);
#endif

#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
/**
 * @brief Install USB UVC video device driver
 *
 * @note USB Host Library must be initialized before calling this function.
 * @note This function will create a task to handle USB UVC video device.
 *
 * @param[in] cfg USB UVC video device configuration
 * @return esp_err_t
 */
esp_err_t esp_video_install_usb_uvc_driver(const esp_video_usb_uvc_device_config_t *cfg);

/**
 * @brief Uninstall USB UVC video device driver
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_uninstall_usb_uvc_driver(void);
#endif // CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE

#ifdef __cplusplus
}
#endif
