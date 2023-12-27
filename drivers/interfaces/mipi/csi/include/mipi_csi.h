/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#include "mipi_csi_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIPI_CSI_PORT_NUM_DEFAULT  (0)

#define ESP_ERR_CAMERA_BASE 0x20000
#define ESP_ERR_CAMERA_NOT_DETECTED             (ESP_ERR_CAMERA_BASE + 1)
#define ESP_ERR_CAMERA_FAILED_TO_SET_FRAME_SIZE (ESP_ERR_CAMERA_BASE + 2)
#define ESP_ERR_CAMERA_FAILED_TO_SET_OUT_FORMAT (ESP_ERR_CAMERA_BASE + 3)
#define ESP_ERR_CAMERA_NOT_SUPPORTED            (ESP_ERR_CAMERA_BASE + 4)
#define ESP_ERR_CAMERA_RESET_FAIL               (ESP_ERR_CAMERA_BASE + 5)
#define ESP_ERR_CAMERA_FAILED_TO_START_FRAME    (ESP_ERR_CAMERA_BASE + 6)

typedef enum {
    // MIPI-CSI Port
    MIPI_CSI_PORT0,
    MIPI_CSI_PORT_MAX,
} mipi_csi_interface_t;

/**
 * @brief Data structure of camera frame buffer
 */
typedef struct {
    uint8_t *buf;               /*!< Pointer to the pixel data */
    size_t len;                 /*!< Length of the buffer in bytes */
    size_t width;               /*!< Width of the buffer in pixels */
    size_t height;              /*!< Height of the buffer in pixels */
    pixformat_t format;         /*!< Format of the pixel data */
    struct timeval timestamp;   /*!< Timestamp since boot of the first DMA buffer of the frame */
#if DEBUG_FB_SEQ
    size_t frame_trans_cnt;     /*!< Record the sequence number in which the frame was acquired */
#endif
} camera_fb_t;

/**
* @brief Configuration structure for camera initialization
*/
typedef struct {
    mipi_csi_lane_num_t lane_num;       /*!< Lane num, 1~(LANE_NUM_MAX - 1)*/
    int mipi_clk;                       /*!< Frequency of MIPI CSI data clk, in Hz. CSI-Host <-> ISP <-> CSI-Bridge*/
    size_t frame_width;
    size_t frame_height;
    uint32_t in_bits_per_pixel;
    uint32_t out_bits_per_pixel;
    size_t vc_channel_num;
    uint32_t dma_req_interval;
    void *reserverd;
} mipi_csi_config_t;

/**
 * @brief mipi csi driver config instance
 *
 */
typedef struct {
    uint16_t width;                   /*!< Frame width, in bytes */
    uint16_t height;                  /*!< Frame height, in bytes */
    uint32_t fb_size;                 /*!< Frame buffer size, in bytes */

    pixformat_t in_type;              /*!< Format of esp32 received data from the sensor */
    uint32_t in_type_bits_per_pixel;
    pixformat_t out_type;             /*!< Format of esp32 bridge output data */
    uint32_t out_type_bits_per_pixel;
    bool cam_isp_en;
} mipi_csi_driv_config_t;

typedef struct esp_mipi_csi_obj *esp_mipi_csi_handle_t;

/**
 * @brief Install mipi csi driver with the given configuration.
 *
 * @param interface The mipi csi interface be used.
 * @param config Pointer to the mipi csi port configuration.
 * @param intr_alloc_flags Flags used to allocate the interrupt. One or multiple (ORred) ESP_INTR_FLAG_* values.
 *                         See esp_intr_alloc.h for more info.
 * @param ret_handle mipi csi driver controller handle.
 *
 * @return
 *      - ESP_ERR_INVALID_ARG   The input parameter is invalid
 *      - ESP_ERR_INVALID_STATE The driver has been initialized already
 *      - ESP_ERR_NO_MEM        No memory for the driver resources
 *      - ESP_OK                Allocate the new driver controller success
 */
esp_err_t esp_mipi_csi_driver_install(mipi_csi_interface_t interface, mipi_csi_port_config_t *config, int intr_alloc_flags, esp_mipi_csi_handle_t *ret_handle);

/**
 * @brief Delete mipi csi driver
 *
 * @note This function does not guarantee thread safety.
 *       Please make sure that no thread will continuously hold semaphores before calling the delete function.
 *
 * @param handle mipi csi controller handle.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t esp_mipi_csi_driver_delete(esp_mipi_csi_handle_t handle);

/**
 * @brief Starts the mipi csi controller's DMA to receive data from the device(sensor).
 *
 * @param handle mipi csi controller handle.
 *
 * @return
 *      - ESP_ERR_INVALID_STATE NULL pointer or invalid configuration
 *      - ESP_OK Success
 */
esp_err_t esp_mipi_csi_start(esp_mipi_csi_handle_t handle);

/**
 * @brief Stop receiving data from the device(sensor).
 * @param handle mipi csi controller handle.
 *
 * @note Calling this function does not prevent the camera sensor from outputting data. If you need to stop the sensor from outputting data,
 * please refer to the technical description of the sensor for implementation.
 * @note Calling this function will not stop the process of receiving image data immediately,
 * it will stop when it is ready to receive next time.
 *
 * @return
 *      - ESP_ERR_INVALID_STATE NULL pointer or the dirver work on the invalid status
 *      - ESP_OK Success
 */
esp_err_t esp_mipi_csi_stop(esp_mipi_csi_handle_t handle);

/*The callback function when allocating buffer and when receiving data.*/
typedef struct {
    uint8_t *(*alloc_buffer)(uint32_t len); // Callback function when allocating buffer
    esp_err_t(*recved_data)(uint8_t *buffer, uint32_t offset, uint32_t len); // The callback function when filling the buffer with image data.
} esp_mipi_csi_ops_t;

/**
 * @brief Notify there is a new buffer is currently available for use..
 * @param handle mipi csi controller handle.
 *
 * @return
 *      - ESP_ERR_INVALID_STATE NULL pointer or the dirver work on the invalid status
 *      - ESP_OK Success
 */
esp_err_t esp_mipi_csi_new_buffer_available(esp_mipi_csi_handle_t handle);

/**
 * @brief Register cb for allocating buffer and buffer is filled.
 * @param handle mipi csi controller handle.
 *
 * @return
 *      - ESP_ERR_INVALID_STATE NULL pointer or the dirver work on the invalid status
 *      - ESP_OK Success
 */
esp_err_t esp_mipi_csi_ops_regist(esp_mipi_csi_handle_t handle, esp_mipi_csi_ops_t *ops);

/**
 * @brief Get mipi csi output frame buffer information
 *
 * @param handle        mipi csi controller handle.
 * @param fb_size       frame buffer size pointer
 * @param fb_align_size frame buffer adress align size pointer
 * @param fb_caps       frame buffer capbility pointer
 *
 * @note  Call this function after esp_mipi_csi_driver_install()
 *
 * @return
 *     - 0 if parameter error
 *     - Others Current framebuffer size
 */
esp_err_t esp_mipi_csi_get_fb_info(esp_mipi_csi_handle_t handle, uint32_t *fb_size, uint32_t *fb_align_size, uint32_t *fb_caps);

#ifdef __cplusplus
}
#endif
