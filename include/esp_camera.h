/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_attr.h"

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

    /*Direction bits*/
#define SENSOR_IOC_GET	0U
#define SENSOR_IOC_SET	1U

#define _SENSOR_IOC_TYPESHIFT (16)
#define _SENSOR_IOC_IDSHIFT   (8)
#define _SENSOR_IOC_DIRSHIFT  (1)

/*
 * Used to create numbers.
 *
 * NOTE: _IOW means changing sensor's para. _SENSOR_IOR
 * means get info from sensor.
 */
#define _SENSOR_IOW(class,id) \
	(((class)  << _SENSOR_IOC_TYPESHIFT) | \
     ((id)  << _SENSOR_IOC_IDSHIFT) | \
	 SENSOR_IOC_SET)

#define _SENSOR_IOR(class,id) \
	(((class)  << _SENSOR_IOC_TYPESHIFT) | \
     ((id)  << _SENSOR_IOC_IDSHIFT) | \
	 SENSOR_IOC_GET)

#define    CAM_SENSOR_OPS_CLASS_DEFAULT  0X00
#define    CAM_SENSOR_OPS_CLASS_3A       0X01
#define    CAM_SENSOR_OPS_CLASS_LENS     0X02
#define    CAM_SENSOR_OPS_CLASS_LED      0X03

/* Set ops */
#define	   CAM_SENSOR_S_HW_RESET         _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x01)  // Hardware reset
#define    CAM_SENSOR_S_SF_RESET         _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x02)  // Software reset
#define	   CAM_SENSOR_S_POWER            _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x03)  // Power on/off
#define	   CAM_SENSOR_S_XCLK             _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x04)  // For sensors that require a clock provided by the base board
#define	   CAM_SENSOR_S_SENSOR_MODE      _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x05)
#define	   CAM_SENSOR_S_REG              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x06)
#define	   CAM_SENSOR_S_STREAM           _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x07)  // Start/Stop stream, can be achieved through the bypass on/off or sleep en/dis of the sensor.
#define	   CAM_SENSOR_S_SUSPEND          _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x08)
#define    CAM_SENSOR_S_LONG_EXP         _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x09)
#define	   CAM_SENSOR_S_FPS              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x0a)
#define	   CAM_SENSOR_S_HDR_RADIO        _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x0b)
#define    CAM_SENSOR_S_BRIGHTNESS       _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x0c)
#define    CAM_SENSOR_S_CONTRAST         _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x0d)
#define    CAM_SENSOR_S_SATURATION       _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x0e)
#define    CAM_SENSOR_S_HUE              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x0f)
#define    CAM_SENSOR_S_GAMMA            _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x10)
#define    CAM_SENSOR_S_HMIRROR          _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x11)
#define    CAM_SENSOR_S_VFLIP            _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x12)
#define    CAM_SENSOR_S_SHARPNESS        _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x13)
#define    CAM_SENSOR_S_DENOISE          _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x14)   // Denoise
#define    CAM_SENSOR_S_DPC              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x15)
#define    CAM_SENSOR_S_JPEG_QUALITY     _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x16)
#define	   CAM_SENSOR_S_BLC              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x17)
#define    CAM_SENSOR_S_SPECIAL_EFFECT   _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x18)
#define    CAM_SENSOR_S_LENC             _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x19)
#define    CAM_SENSOR_S_TEST_PATTERN     _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x1a)
#define    CAM_SENSOR_S_FORMAT           _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_DEFAULT,0x1b)
#define    CAM_SENSOR_S_AWB              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x01)       // Auto white balance
#define    CAM_SENSOR_S_EXPOSURE         _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x02)
#define	   CAM_SENSOR_S_EXP              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x03)
#define	   CAM_SENSOR_S_VSEXP            _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x04)
#define	   CAM_SENSOR_S_LONG_GAIN        _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x05)
#define    CAM_SENSOR_S_DGAIN            _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x06)       // Dight gain
#define    CAM_SENSOR_S_ANGAIN           _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x07)       // Analog gain
#define	   CAM_SENSOR_S_VSGAIN           _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x08)
#define    CAM_SENSOR_S_AEC              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x09)
#define    CAM_SENSOR_S_AGC              _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x0a)
#define    CAM_SENSOR_S_AF_AUTO          _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x0b)       // Auto Focus
#define    CAM_SENSOR_S_AF_INIT          _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x0c)
#define	   CAM_SENSOR_S_AF_RELEASE       _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x0d)
#define	   CAM_SENSOR_S_AF_START         _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x0e)
#define	   CAM_SENSOR_S_AF_STOP          _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x0f)
#define    CAM_SENSOR_S_WB               _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x10)      // White balance mode
#define    CAM_SENSOR_S_3A_LOCK          _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_3A,0x11)
#define    CAM_SENSOR_S_FLASH_LED        _SENSOR_IOW(CAM_SENSOR_OPS_CLASS_LED,0x12)

/* Get ops*/
#define    CAM_SENSOR_G_POWER           _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x01)
#define    CAM_SENSOR_G_XCLK            _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x02)
#define    CAM_SENSOR_G_SENSOR_MODE     _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x03)
#define    CAM_SENSOR_G_REG             _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x04)
#define    CAM_SENSOR_G_NAME            _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x05)
#define    CAM_SENSOR_G_FORMAT_ARRAY    _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x06)
#define    CAM_SENSOR_G_FORMAT          _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x07)
#define	   CAM_SENSOR_G_CHIP_ID         _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x08)
#define    CAM_SENSOR_G_FPS             _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x09)
#define    CAM_SENSOR_G_CAP             _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_DEFAULT,0x0a)
#define    CAM_SENSOR_G_AF_STATUS       _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_3A,0x01)
#define    CAM_SENSOR_G_WB              _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_3A,0x02)
#define    CAM_SENSOR_G_INT_TIME        _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_3A,0x03)         // Integral time
#define    CAM_SENSOR_G_FLASH_LED       _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_LED,0x01)
#define	   CAM_SENSOR_G_LENS            _SENSOR_IOR(CAM_SENSOR_OPS_CLASS_LENS,0x01)

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

/* Enumeration for get_supported_value() */
typedef enum {
    CAM_SENSOR_CTRL_TYPE_INTEGER = 1,
    CAM_SENSOR_CTRL_TYPE_BOOLEAN = 2,
    CAM_SENSOR_CTRL_TYPE_INTEGER64 = 5,
    CAM_SENSOR_CTRL_TYPE_BITMASK = 8,
    CAM_SENSOR_CTRL_TYPE_INTEGER_MENU = 9,
    CAM_SENSOR_CTRL_TYPE_U8FIXEDPOINT_Q7 = 10,
    CAM_SENSOR_CTRL_TYPE_U16FIXEDPOINT_Q8 = 11,
    CAM_SENSOR_CTRL_TYPE_INTEGER_TIMES_3 = 12,
    CAM_SENSOR_CTRL_TYPE_U8 = 0x0100,
    CAM_SENSOR_CTRL_TYPE_U16 = 0x0101,
    CAM_SENSOR_CTRL_TYPE_U32 = 0x0102,
} sensor_para_ctrl_type_t;

/* Structure for sensor parameter. Used for get_supported_value() */
typedef struct {
    int32_t  minimum;
    int32_t  maximum;
    uint32_t step;
    int32_t  default_value;
} sensor_para_capability_range_t;

typedef struct {
    int8_t  nr_values;
    const int32_t *values;
    int32_t default_value;
} sensor_para_capability_discrete_t;

typedef struct {
    uint32_t nr_elems;
    int32_t  minimum;
    int32_t  maximum;
    int32_t step;
} sensor_para_capability_elems_t;

typedef struct {
    sensor_para_ctrl_type_t type;   /* Control type */
    union {
        /* Use 'range' member in the following types cases.
            *   cam_sensor_CTRL_TYPE_INTEGER
            *   cam_sensor_CTRL_TYPE_BOOLEAN
            *   cam_sensor_CTRL_TYPE_INTEGER64
            *   cam_sensor_CTRL_TYPE_BITMASK
            *   cam_sensor_CTRL_TYPE_U8FIXEDPOINT_Q7
            *   cam_sensor_CTRL_TYPE_U16FIXEDPOINT_Q8
            *   cam_sensor_CTRL_TYPE_INTEGER_TIMES_3
            */

        sensor_para_capability_range_t    range;

        /* Use 'discrete' member in the following type case.
            *   sensor_para_CTRL_TYPE_INTEGER_MENU
            */

        sensor_para_capability_discrete_t discrete;

        /* Use 'elems' member in the following types cases.
            *   sensor_para_CTRL_TYPE_U8
            *   sensor_para_CTRL_TYPE_U16
            *   sensor_para_CTRL_TYPE_U32
            */

        sensor_para_capability_elems_t    elems;
    } u;
} sensor_para_supported_value_t;

typedef union sensor_para_value_u {
    int32_t  value32;
    uint8_t *p_u8;
    uint16_t *p_u16;
    uint32_t *p_u32;
} sensor_para_value_t;

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
    uint8_t sccb_port;
    int8_t  xclk_pin;
    int8_t  reset_pin;
    int8_t  pwdn_pin;
    sensor_format_t *cur_format;                                                                // current format
    sensor_id_t id;                                                                             // Sensor ID.
    uint8_t stream_status;
    // struct mutex lock;                                                                       // io mutex lock
    esp_camera_ops_t *ops;
    void *priv
} esp_camera_device_t;

typedef struct _esp_camera_ops {
    /* ISP */
    int (*get_supported_para_value) (esp_camera_device_t *dev, uint32_t para_id, sensor_para_supported_value_t *value);
    int (*get_para_value)           (esp_camera_device_t *dev, uint32_t para_id, uint32_t size, sensor_para_value_t *value);
    int (*set_para_value)           (esp_camera_device_t *dev, uint32_t para_id, uint32_t size, sensor_para_value_t value);
    /* Common */
    int (*query_support_formats)    (esp_camera_device_t *dev, void *parry);
    int (*query_support_capability) (esp_camera_device_t *dev, void *arg);
    int (*set_format)               (esp_camera_device_t *dev, void *format);
    int (*get_format)               (esp_camera_device_t *dev, void *ret_format);
    int (*priv_ioctl)               (esp_camera_device_t *dev, unsigned int cmd, void *arg);
    int (*get_name)                 (esp_camera_device_t *dev, void *name, size_t *size);
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

esp_err_t esp_camera_ioctl(esp_camera_device_t *dev, uint32_t cmd, void *value, size_t *size);

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
} esp_camera_sim_config_t;

typedef struct {
    uint8_t  i2c_or_i3c;        /*!< Use I2C or I3C for SCCB, 0: I2C, 1: I3C, don't use other values. For I3C, it's only applied to the chips with I3C. */
    int8_t   scl_pin;           /*!< Specify the SCCB SCL pin number, use -1 when the SCCB port is initialized in application level. */
    int8_t   sda_pin;           /*!< Specify the SCCB SDA pin number, use -1 when the SCCB port is initialized in application level. */
    uint8_t  i2c_port;          /*!< Specify the I2C port number of SCCB */
    uint32_t i2c_freq;          /*!< Specify the I2C frequency of SCCB, setting to 0 will use the default freq defined in SCCB driver. */
} esp_camera_sccb_config_t;

typedef struct {
    uint8_t sccb_num;               /*!< Specify the numbers of SCCB used by cameras, if there are two same type cameras connected, it should be 2, otherwise, set it to 1. */
    esp_camera_sccb_config_t *sccb;
    esp_camera_csi_config_t *csi;
    uint8_t dvp_num;                /*!< Specify the numbers of dvp cameras */
    esp_camera_dvp_config_t *dvp;
    uint8_t sim_num;                /*!< Specify the numbers of simulated cameras */
    esp_camera_sim_config_t *sim;
} esp_camera_config_t;

esp_err_t esp_camera_init(const esp_camera_config_t *config);

#ifdef __cplusplus
}
#endif
