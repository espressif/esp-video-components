/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/param.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_attr.h"
#include "linux/ioctl.h"
#include "linux/v4l2-controls.h"
#include "linux/videodev2.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#define ESP_CAM_SENSOR_LOGE(_fmt, ...)   ESP_LOGE(TAG, "%s(%d): "_fmt, __func__, __LINE__, ##__VA_ARGS__)
#define ESP_CAM_SENSOR_NULL_POINTER_CHECK(tag, p)   ESP_RETURN_ON_FALSE((p), ESP_ERR_INVALID_ARG, tag, "input parameter '"#p"' is NULL")
#define ESP_CAM_SENSOR_BASE 0x30000
#define ESP_CAM_SENSOR_NOT_DETECTED             (ESP_CAM_SENSOR_BASE + 1)
#define ESP_CAM_SENSOR_FAILED_TO_S_FORMAT       (ESP_CAM_SENSOR_BASE + 2)
#define ESP_CAM_SENSOR_FAILED_TO_S_REG_VAULE    (ESP_CAM_SENSOR_BASE + 3)
#define ESP_CAM_SENSOR_FAILED_TO_G_REG_VAULE    (ESP_CAM_SENSOR_BASE + 3)
#define ESP_CAM_SENSOR_NOT_SUPPORTED            (ESP_CAM_SENSOR_BASE + 4)
#define ESP_CAM_SENSOR_RESET_FAIL               (ESP_CAM_SENSOR_BASE + 5)

#define SENSOR_NAME_MAX_LEN (32)
#define SENSOR_ISP_INFO_VERSION_DEFAULT (1)

/*!< Class and ID are 8-bit */
#define _SENSOR_CLASS_SHIFT 8
#define _SENSOR_ID_SHIFT    0

#define V4L2_CTRL_CLASS_PRIV            0x00a60000  /*!< V4L2 Control classes, please refer to "v4l2-controls.h" */

#define _SENSOR_CLASS_ID(class, id) \
    (((id) << _SENSOR_ID_SHIFT) | \
     ((class) << _SENSOR_CLASS_SHIFT) | \
     V4L2_CTRL_CLASS_PRIV)

#define GET_SENSOR_CLASS(val) \
    (((val) >> _SENSOR_CLASS_SHIFT) & 0xffff)

#define GET_SENSOR_ID(val) \
    (((val) >> _SENSOR_ID_SHIFT) & 0xffff)

/**
 * @brief Camera sensor control ID class
 */
#define CAM_SENSOR_CID_CLASS_DEFAULT    0X00        /*!< Sensor default control ID class */
#define CAM_SENSOR_CID_CLASS_3A         0X01        /*!< Sensor 3A control ID class */
#define CAM_SENSOR_CID_CLASS_LENS       0X02        /*!< Sensor lens control ID class */
#define CAM_SENSOR_CID_CLASS_LED        0X03        /*!< Sensor flash LED control ID class */

/**
 * @brief Camera sensor control ID which is used set/get/query
 */
#define CAM_SENSOR_POWER                _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x01)
#define CAM_SENSOR_XCLK                 _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x02)    // For sensors that require a clock provided by the base board
#define CAM_SENSOR_SENSOR_MODE          _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x03)
#define CAM_SENSOR_LONG_EXP             _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x04)
#define CAM_SENSOR_FPS                  _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x05)
#define CAM_SENSOR_HDR_RADIO            _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x06)
#define CAM_SENSOR_BRIGHTNESS           _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x07)
#define CAM_SENSOR_CONTRAST             _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x08)
#define CAM_SENSOR_SATURATION           _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x09)
#define CAM_SENSOR_HUE                  _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x0a)
#define CAM_SENSOR_GAMMA                _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x0b)
#define CAM_SENSOR_HMIRROR              _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x0c)
#define CAM_SENSOR_VFLIP                _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x0d)
#define CAM_SENSOR_SHARPNESS            _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x0e)
#define CAM_SENSOR_DENOISE              _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x0f)    // Denoise
#define CAM_SENSOR_DPC                  _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x10)
#define CAM_SENSOR_JPEG_QUALITY         _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x11)
#define CAM_SENSOR_BLC                  _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x12)
#define CAM_SENSOR_SPECIAL_EFFECT       _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x13)
#define CAM_SENSOR_LENC                 _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_DEFAULT, 0x14)

#define CAM_SENSOR_AWB                  _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x01)   // Auto white balance
#define CAM_SENSOR_EXPOSURE             _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x02)
#define CAM_SENSOR_EXP                  _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x03)
#define CAM_SENSOR_VSEXP                _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x04)
#define CAM_SENSOR_LONG_GAIN            _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x05)
#define CAM_SENSOR_DGAIN                _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x06)   // Dight gain
#define CAM_SENSOR_ANGAIN               _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x07)   // Analog gain
#define CAM_SENSOR_VSGAIN               _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x08)
#define CAM_SENSOR_AE_CONTROL           _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x09)
#define CAM_SENSOR_AGC                  _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x0a)
#define CAM_SENSOR_AF_AUTO              _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x0b)   // Auto Focus
#define CAM_SENSOR_AF_INIT              _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x0c)
#define CAM_SENSOR_AF_RELEASE           _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x0d)
#define CAM_SENSOR_AF_START             _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x0e)
#define CAM_SENSOR_AF_STOP              _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x0f)
#define CAM_SENSOR_WB                   _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x10)  // White balance mode
#define CAM_SENSOR_3A_LOCK              _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x11)
#define CAM_SENSOR_AF_STATUS            _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x12)
#define CAM_SENSOR_INT_TIME             _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x13)         // Integral time
#define CAM_SENSOR_AE_LEVEL             _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_3A, 0x14)

#define CAM_SENSOR_LENS                 _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_LENS, 0x01)

#define CAM_SENSOR_FLASH_LED            _SENSOR_CLASS_ID(CAM_SENSOR_CID_CLASS_LED, 0x01)

/*!< Camera simulation ioctl command */
#define CAM_SIM_IOC_BASE                0x1
#define CAM_SIM_IOC_S_RXCB              _IOW(CAM_SIM_IOC_BASE, 0x01, sizeof(struct sim_cam_rx)) /*!< Set receive callback function */

/*!< Camera sensor ioctl command */
#define CAM_SENSOR_IOC_BASE             0x2
#define CAM_SENSOR_IOC_HW_RESET         _IOW(CAM_SENSOR_IOC_BASE, 0x01, 0)
#define CAM_SENSOR_IOC_SW_RESET         _IOW(CAM_SENSOR_IOC_BASE, 0x02, 0)
#define CAM_SENSOR_IOC_S_TEST_PATTERN   _IOW(CAM_SENSOR_IOC_BASE, 0x03, sizeof(int))
#define CAM_SENSOR_IOC_S_STREAM         _IOW(CAM_SENSOR_IOC_BASE, 0x04, sizeof(int))
#define CAM_SENSOR_IOC_S_SUSPEND        _IOW(CAM_SENSOR_IOC_BASE, 0x05, sizeof(int))
#define CAM_SENSOR_IOC_G_CHIP_ID        _IOR(CAM_SENSOR_IOC_BASE, 0x06, sizeof(struct sensor_chip_id))
#define CAM_SENSOR_IOC_S_REG            _IOW(CAM_SENSOR_IOC_BASE, 0x07, sizeof(struct sensor_reg_val))
#define CAM_SENSOR_IOC_G_REG            _IOR(CAM_SENSOR_IOC_BASE, 0x08, sizeof(struct sensor_reg_val))

/*!< Simulation camera receive callback parameters */
struct sim_cam_rx {
    void (*cb)(void *priv, const uint8_t *buffer, size_t n);    /*!< Callback function */
    void *priv;                                                 /*!< Callback private data */
};

/*!< Sensor chip ID */
struct sensor_chip_id {
    uint32_t id;    /*!< Chip ID value */
};

/*!< Sensor set/get register value parameters */
struct sensor_reg_val {
    uint32_t regaddr;   /*!< Register address */
    uint32_t value;     /*!< Register value */
};

typedef enum {
    GAINCEILING_2X,
    GAINCEILING_4X,
    GAINCEILING_8X,
    GAINCEILING_16X,
    GAINCEILING_32X,
    GAINCEILING_64X,
    GAINCEILING_128X,
} sensor_gainceiling_t;

typedef struct {
    uint8_t MIDH;
    uint8_t MIDL;
    uint16_t PID;
    uint8_t VER;
} sensor_id_t;

typedef enum {
    CAM_SENSOR_BAYER_RGGB = 0,
    CAM_SENSOR_BAYER_GRBG = 1,
    CAM_SENSOR_BAYER_GBRG = 2,
    CAM_SENSOR_BAYER_BGGR = 3,
    CAM_SENSOR_BAYER_BUTT     // No bayer pattern, just for MONO(Support Only Y output) sensor
} sensor_bayer_pattern_t;

typedef enum {
    DVP_OUTPUT_8BITS,
    MIPI_CSI_OUTPUT_LANE1, // MIPI-CSI 1 data lane
    MIPI_CSI_OUTPUT_LANE2, // MIPI-CSI 2 data lanes
} sensor_port_t;

typedef enum {
    CAM_SENSOR_FULL_WINDOWING,
    CAM_SENSOR_CLIP,
    CAM_SENSOR_BINNING,
    CAM_SENSOR_SUBSAMPLE,
    CAM_SENSOR_SCALE
} sensor_windowing_t;

/**
 * @brief Enumerated list of sensor output pixel data formats
 */
typedef enum {
    CAM_SENSOR_PIXFORMAT_RGB565,    // 2BPP/RGB565, 1Bytes Per Pixel
    CAM_SENSOR_PIXFORMAT_YUV422,    // 2BPP/YUV422
    CAM_SENSOR_PIXFORMAT_YUV420,    // 1.5BPP/YUV420
    CAM_SENSOR_PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
    CAM_SENSOR_PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    CAM_SENSOR_PIXFORMAT_RGB888,    // 3BPP/RGB888
    CAM_SENSOR_PIXFORMAT_RAW8,      // 1BPP/RAW8
    CAM_SENSOR_PIXFORMAT_RAW10,     // 5BP4P/RAW8
    CAM_SENSOR_PIXFORMAT_RAW12,     // 3BP2P/RAW8
    CAM_SENSOR_PIXFORMAT_RGB444,    // 3BP2P/RGB444
    CAM_SENSOR_PIXFORMAT_RGB555,    // 3BP2P/RGB555
    CAM_SENSOR_PIXFORMAT_BGR565,    // 2BPP/RGB565
    CAM_SENSOR_PIXFORMAT_BGR888,    // 3BPP/RGB888
    CAM_SENSOR_PIXFORMAT_INVALID,
} sensor_pixformat_t;

/**
 * @brief Enumerated list of camera sensor parameter IDs, Mainly for 3A.
 */
enum {
    CAM_SENSOR_PARA_AEC,
    CAM_SENSOR_PARA_AGC,
    CAM_SENSOR_PARA_AF,
    CAM_SENSOR_PARA_AWB,
    CAM_SENSOR_PARA_MAX,
};

enum {
    CAM_SENSOR_TYPE_V1,
    CAM_SENSOR_TYPE_MAX,
};

enum sensor_hdr_mode_e {
    CAM_SENSOR_HDR_LINEAR,
    CAM_SENSOR_HDR_STITCH,
    CAM_SENSOR_HDR_NATIVE,
};

enum sensor_lens_type_id_e {
    CAM_SENSOR_LENS_FIXED,
    CAM_SENSOR_LENS_AUTO,
    CAM_SENSOR_LENS_FISHEYE,
    CAM_SENSOR_LENS_TWO_PASS_FILTER,
};

typedef struct _sensor_lens {
    uint32_t id;
    char name[16];
} sensor_lens_t;

typedef struct _sensor_mipi_info {
    uint32_t mipi_clk;
} sensor_mipi_info_t;

/* For sensors that output RAW format, it is used to provide the ISP with the required information. */
typedef struct _sensor_isp_v1_info {
    const uint32_t version;
    int pclk;
    union {
        int hts;               // HTS = H_Size + H_Blank
        int hamx;
    };
    union {
        int vts;               // VTS = V_Size + V_Blank
        int vmax;
    };
} sensor_isp_info_v1_t;

typedef union _sensor_isp_info {
    sensor_isp_info_v1_t isp_v1_info;
} sensor_isp_info_t;

typedef struct _sensor_format_struct {
    int index;
    const char *name;
    sensor_pixformat_t format;
    sensor_port_t port;
    sensor_bayer_pattern_t bayer_type;
    uint32_t hdr_mode;
    int xclk;                // Sensor input clock
    int start_pos_x;         // Output windows start point x, set -1 if no used
    int start_pos_y;         // Output windows start point y, set -1 if no used
    uint16_t width;          // Output windows width
    uint16_t height;         // Output windows height

    const void *regs;        // Regs to enable this format
    int regs_size;
    uint8_t bpp;             // Bytes per pixel
    uint8_t fps;             // Bytes per pixel
    /* for sensor without internal ISP */
    const sensor_isp_info_t *isp_info; // Set NULL if the sensor itself has the ISP processor.
    /* for mipi sensor */
    sensor_mipi_info_t mipi_info;
    void *reserved;          // can be used to provide AE\AF\AWB info or Parameters of some related accessories（VCM、LED、IR）
} sensor_format_t;

/**
 * @brief Camera sensor capability object.
 */
typedef struct _sensor_capability {

    /* Data format field */
    uint32_t fmt_raw : 1;
    uint32_t fmt_rgb565 : 1;
    uint32_t fmt_yuv : 1;
    uint32_t fmt_jpeg : 1;
} sensor_capability_t;

typedef struct sensor_format_info_array_s {
    uint32_t count;
    const sensor_format_t *format_array;
} sensor_format_array_info_t;

typedef struct _esp_camera_ops esp_camera_ops_t;

typedef struct {
    char *name;
    uint8_t sccb_port;
    int8_t  xclk_pin;
    int8_t  reset_pin;
    int8_t  pwdn_pin;
    const sensor_format_t *cur_format;                                                                // current format
    sensor_id_t id;                                                                             // Sensor ID.
    uint8_t stream_status;
    // struct mutex lock;                                                                       // io mutex lock
    esp_camera_ops_t *ops;
    void *priv;
} esp_camera_device_t;

typedef struct _esp_camera_ops {
    /* ISP */
    int (*query_para_desc)          (esp_camera_device_t *dev, struct v4l2_query_ext_ctrl *qctrl);
    int (*get_para_value)           (esp_camera_device_t *dev, struct v4l2_ext_control *ctrl);
    int (*set_para_value)           (esp_camera_device_t *dev, const struct v4l2_ext_control *ctrl);

    /* Common */
    int (*query_support_formats)    (esp_camera_device_t *dev, sensor_format_array_info_t *parry);
    int (*query_support_capability) (esp_camera_device_t *dev, sensor_capability_t *arg);
    int (*set_format)               (esp_camera_device_t *dev, const sensor_format_t *format);
    int (*get_format)               (esp_camera_device_t *dev, sensor_format_t *format);
    int (*priv_ioctl)               (esp_camera_device_t *dev, unsigned int cmd, void *arg);
} esp_camera_ops_t;

#if 0
#define CAM_SENSOR_INIT_COMMON(SENSOR_NAME)  \
    do { \
        sensor->sensor_common.name                      = SENSOR_NAME;               \
        sensor->sensor_common.slv_addr                  = SENSOR_SCCB_ADDR;          \
        sensor->sensor_common.sccb_port                 = sccb_port;                 \
        sensor->sensor_common.query_support_formats     = query_support_formats;     \
        sensor->sensor_common.query_support_capability  = query_support_capability;  \
        sensor->sensor_common.set_format                = set_format;                \
        sensor->sensor_common.get_format                = get_format;                \
        sensor->sensor_common.priv_ioctl                = priv_ioctl;                \
    } while (0)

#define PRINT_CAM_SENSOR_INFO(sensor)  \
    do { \
        printf("type_v=%d, name=%s, slave_addr=%u, sccb_port=%d,\n", \
        sensor->sensor_type,\
        sensor->sensor_common.name,\
        sensor->sensor_common.slv_addr,\
        sensor->sensor_common.sccb_port); \
    } while(0);


#else
#define PRINT_CAM_SENSOR_FORMAT_INFO(p_format)  \
    do { \
        printf("format[%d]: name=%s\n  {w=%u, h=%u, regs=%p, fps=%d, mipi_clk=%u}\n", \
        ((sensor_format_t*)p_format)->index,\
        ((sensor_format_t*)p_format)->name,\
        ((sensor_format_t*)p_format)->width,\
        ((sensor_format_t*)p_format)->height,\
        ((sensor_format_t*)p_format)->regs,\
        ((sensor_format_t*)p_format)->fps,\
        ((sensor_format_t*)p_format)->mipi_info.mipi_clk); \
    } while(0);
#endif

typedef enum {
    CAMERA_INTF_CSI,
    CAMERA_INTF_DVP,
    CAMERA_INTF_SIM
} camera_intf_t;

/**
 * Internal structure describing ESP_SYSTEM_INIT_FN startup functions
 */
typedef struct {
    esp_camera_device_t *(*fn)(void *);   /*!< Pointer to the detect function */
    camera_intf_t intf;                   /*!< Interface of the camera */
} esp_camera_detect_fn_t;

/**
 * @brief Define a camera detect function which will be executed when esp_camera_init() is called.
 *
 * @param f  function name (identifier)
 * @param i  interface which is used to communicate with the camera
 * @param (varargs)  optional, additional attributes for the function declaration (such as IRAM_ATTR)
 *
 * The function defined using this macro must return esp_camera_device_t on success. Any other value will be
 * logged and the esp_camera_init process will abort.
 *
 * There should be at lease one undefined symble to be added in the camera driver in order to avoid
 * the optimization of the linker. Because otherwise the linker will ignore camera driver as it has
 * no other files depending on any symbols in it.
 *
 * Some thing like this should be added in the CMakeLists.txt of the camera driver:
 *  target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov2640_detect")
 */
#define ESP_CAMERA_DETECT_FN(f, i, ...) \
    static esp_camera_device_t * __VA_ARGS__ __esp_camera_detect_fn_##f(void *config); \
    static __attribute__((used)) _SECTION_ATTR_IMPL(".esp_camera_detect_fn", __COUNTER__) \
        esp_camera_detect_fn_t esp_camera_detect_fn_##f = { .fn = ( __esp_camera_detect_fn_##f), .intf = (i) }; \
    static esp_camera_device_t *__esp_camera_detect_fn_##f(void *config)

typedef struct {
    uint8_t sccb_port;                  /*!< Specify I2C/I3C port used for SCCB */
    int8_t  xclk_pin;
    int8_t  reset_pin;
    int8_t  pwdn_pin;
    int32_t xclk_freq_hz;
} esp_camera_driver_config_t;

typedef struct {
    uint8_t sccb_config_index;          /*!< Specify the index number of esp_camera_sccb_config_t */
    int8_t  xclk_pin;
    int8_t  reset_pin;
    int8_t  pwdn_pin;
    int32_t xclk_freq_hz;
} esp_camera_csi_config_t;

typedef struct {
    uint8_t sccb_config_index;          /*!< Specify the index number of esp_camera_sccb_config_t */
    int8_t  xclk_pin;
    int8_t  reset_pin;
    int8_t  pwdn_pin;
    int32_t xclk_freq_hz;
} esp_camera_dvp_config_t;

typedef struct {
    int id;
} esp_camera_sim_config_t;

typedef struct {
    uint8_t  i2c_or_i3c;        /*!< Use I2C or I3C for SCCB, 0: I2C, 1: I3C, don't use other values. For I3C, it's only applied to the chips with I3C. */
    int8_t   scl_pin;           /*!< Specify the I2C/I3C SCL pin number, use -1 when the SCCB port is initialized in application level. */
    int8_t   sda_pin;           /*!< Specify the I2C/I3C SDA pin number, use -1 when the SCCB port is initialized in application level. */
    uint8_t  port;              /*!< Specify the I2C/I3C port number of SCCB */
    uint32_t freq;              /*!< Specify the I2C/I3C frequency of SCCB, setting to 0 will use the default freq defined in SCCB driver. */
} esp_camera_sccb_config_t;

typedef struct {
    uint8_t sccb_num;               /*!< Specify the numbers of SCCB used by cameras, if there are two same type cameras connected, it should be 2, otherwise, set it to 1. */
    const esp_camera_sccb_config_t *sccb;
    const esp_camera_csi_config_t *csi;
    uint8_t dvp_num;                /*!< Specify the numbers of dvp cameras */
    const esp_camera_dvp_config_t *dvp;
    uint8_t sim_num;                /*!< Specify the numbers of simulated cameras */
    const esp_camera_sim_config_t *sim;
} esp_camera_config_t;

esp_err_t esp_camera_query_para_desc(esp_camera_device_t *dev, struct v4l2_query_ext_ctrl *qctrl);

esp_err_t esp_camera_get_para_value(esp_camera_device_t *dev, struct v4l2_ext_control *ctrl);

esp_err_t esp_camera_set_para_value(esp_camera_device_t *dev, const struct v4l2_ext_control *ctrl);

esp_err_t esp_camera_get_capability(esp_camera_device_t *dev, sensor_capability_t *caps);

esp_err_t esp_camera_query_format(esp_camera_device_t *dev, sensor_format_array_info_t *format_arry);

esp_err_t esp_camera_set_format(esp_camera_device_t *dev, const sensor_format_t *format);

esp_err_t esp_camera_get_format(esp_camera_device_t *dev, sensor_format_t *format);

esp_err_t esp_camera_ioctl(esp_camera_device_t *dev, uint32_t cmd, void *arg);

const char *esp_camera_get_name(esp_camera_device_t *dev);

esp_err_t esp_camera_init(const esp_camera_config_t *config);

#ifdef __cplusplus
}
#endif
