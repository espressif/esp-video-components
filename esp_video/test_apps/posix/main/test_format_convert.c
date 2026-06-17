/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_video_csi_format.h"
#include "esp_video_caps.h"

static const char *TAG = "format_convert";

/* Sensor to V4L2 format mapping */
static uint32_t sensor_to_v4l2_format(esp_cam_sensor_output_format_t sensor_fmt)
{
    switch (sensor_fmt) {
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE:
        return V4L2_PIX_FMT_RGB565;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565_BE:
        return V4L2_PIX_FMT_RGB565X;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY:
        return V4L2_PIX_FMT_UYVY;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV:
        return V4L2_PIX_FMT_YUYV;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV420:
        return V4L2_PIX_FMT_YUV420;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB888:
        return V4L2_PIX_FMT_RGB24;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        return V4L2_PIX_FMT_SBGGR8;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        return V4L2_PIX_FMT_SBGGR10;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        return V4L2_PIX_FMT_SBGGR12;
    case ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE:
        return V4L2_PIX_FMT_GREY;
    default:
        return 0;
    }
}

/* Helper function to convert V4L2 format to string */
static const char *v4l2_format_to_string(uint32_t fmt)
{
    switch (fmt) {
    case V4L2_PIX_FMT_SBGGR8:    return "SBGGR8";
    case V4L2_PIX_FMT_SBGGR10:   return "SBGGR10";
    case V4L2_PIX_FMT_SBGGR12:   return "SBGGR12";
    case V4L2_PIX_FMT_RGB565:    return "RGB565";
    case V4L2_PIX_FMT_RGB565X:   return "RGB565X";
    case V4L2_PIX_FMT_RGB24:     return "RGB24";
    case V4L2_PIX_FMT_YUV420:    return "YUV420";
    case V4L2_PIX_FMT_UYVY:      return "UYVY";
    case V4L2_PIX_FMT_VYUY:      return "VYUY";
    case V4L2_PIX_FMT_YUYV:      return "YUYV";
    case V4L2_PIX_FMT_YVYU:      return "YVYU";
    case V4L2_PIX_FMT_JPEG:      return "JPEG";
    case V4L2_PIX_FMT_H264:      return "H264";
    case V4L2_PIX_FMT_GREY:      return "GREY";
    default:                     return NULL;
    }
}

/* Helper function to convert ISP color to string */
static const char *isp_color_to_string(isp_color_t color)
{
    switch (color) {
    case ISP_COLOR_RAW8:   return "RAW8";
    case ISP_COLOR_RAW10:  return "RAW10";
    case ISP_COLOR_RAW12:  return "RAW12";
    case ISP_COLOR_RGB888: return "RGB888";
    case ISP_COLOR_RGB565: return "RGB565";
    case ISP_COLOR_YUV422: return "YUV422";
    case ISP_COLOR_YUV420: return "YUV420";
    default:               return "Unknown";
    }
}

/* Helper function to convert CSI color to string */
static const char *csi_color_to_string(cam_ctlr_color_t color)
{
    switch (color) {
    case CAM_CTLR_COLOR_RAW8:   return "RAW8";
    case CAM_CTLR_COLOR_RAW10:  return "RAW10";
    case CAM_CTLR_COLOR_RAW12:  return "RAW12";
    case CAM_CTLR_COLOR_RGB565: return "RGB565";
    case CAM_CTLR_COLOR_RGB888: return "RGB888";
    case CAM_CTLR_COLOR_YUV420: return "YUV420";
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
    case CAM_CTLR_COLOR_YUV422_YVYU: return "YUV422(YVYU)";
    case CAM_CTLR_COLOR_YUV422_YUYV: return "YUV422(YUYV)";
    case CAM_CTLR_COLOR_YUV422_UYVY: return "YUV422(UYVY)";
    case CAM_CTLR_COLOR_YUV422_VYUY: return "YUV422(VYUY)";
#else /* ESP_VIDEO_CSI_DEVICE_CONV_FORMAT */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    case CAM_CTLR_COLOR_YUV422_UYVY: return "YUV422(UYVY)";
#else /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */
    case CAM_CTLR_COLOR_YUV422: return "YUV422(UYVY)";
#endif /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */
#endif /* ESP_VIDEO_CSI_DEVICE_CONV_FORMAT */
    case CAM_CTLR_COLOR_GRAY8:  return "GRAY8";
    default:                    return "Unknown";
    }
}

/**
 * @brief Validate that all specified V4L2 formats are supported and can be enumerated
 *
 * This function checks if all formats in the provided array can be enumerated
 * and configured through the CSI-ISP pipeline for the given sensor format.
 *
 * @param sensor_fmt Camera sensor output format
 * @param expected_formats Array of V4L2 formats expected to be supported
 * @param expected_count Number of formats in the array
 * @return true if all formats are supported, false otherwise
 */
bool validate_supported_formats(esp_cam_sensor_output_format_t sensor_fmt,
                                const uint32_t *expected_formats,
                                int expected_count)
{
    if (!expected_formats || expected_count <= 0) {
        ESP_LOGE(TAG, "Invalid parameters: expected_formats=%p, expected_count=%d",
                 expected_formats, expected_count);
        return false;
    }

    ESP_LOGI(TAG, "Validating %d expected formats for sensor format %d",
             expected_count, sensor_fmt);

    /* Create in/out format structure */
    esp_video_csi_isp_in_out_format_t in_out_format = {0};

    bool all_supported = true;

    /* First, enumerate all supported formats and store them in a set */
    uint32_t enumerated_formats[64]; /* Reasonable maximum */
    int enumerated_count = 0;

    /* Enumerate all supported formats */
    uint32_t pixel_format = 0;
    uint32_t index = 0;

    while (1) {
        esp_err_t ret = esp_video_csi_enum_format(sensor_fmt, index, &pixel_format);
        if (ret != ESP_OK) {
            /* No more formats */
            break;
        }

        /* Check if we have space in the array */
        if (enumerated_count >= (int)(ARRAY_SIZE(enumerated_formats))) {
            ESP_LOGW(TAG, "Too many enumerated formats, truncating at %d", enumerated_count);
            break;
        }

        enumerated_formats[enumerated_count++] = pixel_format;
        index++;
    }

    ESP_LOGI(TAG, "Enumerated %d formats for sensor format %d",
             enumerated_count, sensor_fmt);

    /* Log enumerated formats for debugging */
    for (int i = 0; i < enumerated_count; i++) {
        const char *fmt_name = v4l2_format_to_string(enumerated_formats[i]);
        ESP_LOGD(TAG, "  [%2d] 0x%" PRIx32 " (%s)", i, enumerated_formats[i],
                 fmt_name ? fmt_name : "Unknown");
    }

    /* Check each expected format */
    for (int i = 0; i < expected_count; i++) {
        uint32_t expected_fmt = expected_formats[i];
        const char *fmt_name = v4l2_format_to_string(expected_fmt);

        /* Step 1: Check if format is enumerated */
        bool enumerated = false;
        for (int j = 0; j < enumerated_count; j++) {
            if (enumerated_formats[j] == expected_fmt) {
                enumerated = true;
                break;
            }
        }

        if (!enumerated) {
            ESP_LOGE(TAG, "Format 0x%" PRIx32 " (%s) is NOT enumerated",
                     expected_fmt, fmt_name ? fmt_name : "Unknown");
            all_supported = false;
            continue;
        }

        /* Step 2: Check if format can be configured */
        memset(&in_out_format, 0, sizeof(in_out_format));

        esp_err_t check_ret = esp_video_csi_check_format(sensor_fmt, expected_fmt, &in_out_format);

        if (check_ret != ESP_OK) {
            ESP_LOGE(TAG, "Format 0x%" PRIx32 " (%s) cannot be configured: error %d",
                     expected_fmt, fmt_name ? fmt_name : "Unknown", check_ret);
            all_supported = false;

            /* Log detailed error information */
            if (check_ret == ESP_ERR_NOT_SUPPORTED) {
                uint32_t sensor_v4l2_fmt = sensor_to_v4l2_format(sensor_fmt);
                ESP_LOGD(TAG, "  Sensor format: 0x%" PRIx32 " (%s) -> Requested: 0x%" PRIx32 " (%s)",
                         sensor_v4l2_fmt, v4l2_format_to_string(sensor_v4l2_fmt),
                         expected_fmt, fmt_name ? fmt_name : "Unknown");
            }
        } else {
            ESP_LOGI(TAG, "Format 0x%" PRIx32 " (%s) is supported and configurable",
                     expected_fmt, fmt_name ? fmt_name : "Unknown");

            /* Log the pipeline configuration for debugging */
            ESP_LOGW(TAG, "  Pipeline config:");
            ESP_LOGW(TAG, "    Sensor output format: 0x%" PRIx32 " (%s)", sensor_to_v4l2_format(sensor_fmt),
                     v4l2_format_to_string(sensor_to_v4l2_format(sensor_fmt)));
            ESP_LOGW(TAG, "    ISP bypass: %s", in_out_format.isp_bypass_required ? "Yes" : "No");
            ESP_LOGW(TAG, "    ISP input: 0x%x (%s)",
                     in_out_format.isp_input_fmt, isp_color_to_string(in_out_format.isp_input_fmt));
            ESP_LOGW(TAG, "    ISP output: 0x%x (%s)",
                     in_out_format.isp_output_fmt, isp_color_to_string(in_out_format.isp_output_fmt));
            ESP_LOGW(TAG, "    CSI input: 0x%x (%s)",
                     in_out_format.csi_input_fmt, csi_color_to_string(in_out_format.csi_input_fmt));
            ESP_LOGW(TAG, "    CSI output: 0x%x (%s)",
                     in_out_format.csi_output_fmt, csi_color_to_string(in_out_format.csi_output_fmt));
        }
    }

    /* Check for formats that are enumerated but not in expected list (optional) */
    bool check_for_unexpected = true;
    if (check_for_unexpected) {
        for (int i = 0; i < enumerated_count; i++) {
            uint32_t enumerated_fmt = enumerated_formats[i];
            bool expected = false;

            for (int j = 0; j < expected_count; j++) {
                if (expected_formats[j] == enumerated_fmt) {
                    expected = true;
                    break;
                }
            }

            if (!expected) {
                const char *fmt_name = v4l2_format_to_string(enumerated_fmt);
                ESP_LOGD(TAG, "Format %" PRIx32 " (%s) is enumerated but not in expected list",
                         enumerated_fmt, fmt_name ? fmt_name : "Unknown");
            }
        }
    }

    if (all_supported) {
        ESP_LOGI(TAG, "All %d expected formats are supported for sensor format %d",
                 expected_count, sensor_fmt);
    } else {
        ESP_LOGE(TAG, "Some expected formats are NOT supported for sensor format %d",
                 sensor_fmt);
    }

    return all_supported;
}

static void test_csi_enum_format(esp_cam_sensor_output_format_t sensor_fmt, const uint32_t *expected_formats, int expected_count)
{
    uint32_t fmt;
    esp_err_t ret;
    int index = 0;

    ESP_LOGI(TAG, "Test enumerate formats for sensor format %s", v4l2_format_to_string(sensor_to_v4l2_format(sensor_fmt)));

    // Iterate through valid indices
    for (int i = 0; i < expected_count; i++) {
        ret = esp_video_csi_enum_format(sensor_fmt, i, &fmt);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_NOT_EQUAL(0, fmt); // Format should not be 0
        bool found = false;
        for (int j = 0; j < expected_count; j++) {
            if (expected_formats[j] == fmt) {
                found = true;
                break;
            }
        }
        TEST_ASSERT(found);
        index++;
    }
    // Verify the total number of supported formats is expected_count
    TEST_ASSERT_EQUAL(expected_count, index);

    // Test invalid boundary index
    ret = esp_video_csi_enum_format(sensor_fmt, index, &fmt);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("Test enumerate formats for MIPI-CSI", "[video][csi]")
{
    const uint32_t expected_formats_1[] = {
        V4L2_PIX_FMT_SBGGR8,
        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_YUV420,
        V4L2_PIX_FMT_UYVY,
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_VYUY,
        V4L2_PIX_FMT_YVYU,
#endif
    };
    test_csi_enum_format(ESP_CAM_SENSOR_PIXFORMAT_RAW8, expected_formats_1, ARRAY_SIZE(expected_formats_1));

    const uint32_t expected_formats_2[] = {
        V4L2_PIX_FMT_SBGGR8,
        V4L2_PIX_FMT_SBGGR10,
        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_YUV420,
        V4L2_PIX_FMT_UYVY,
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_VYUY,
        V4L2_PIX_FMT_YVYU,
#endif
    };
    test_csi_enum_format(ESP_CAM_SENSOR_PIXFORMAT_RAW10, expected_formats_2, ARRAY_SIZE(expected_formats_2));

    const uint32_t expected_formats_3[] = {
        V4L2_PIX_FMT_SBGGR12,
    };
    test_csi_enum_format(ESP_CAM_SENSOR_PIXFORMAT_RAW12, expected_formats_3, ARRAY_SIZE(expected_formats_3));

    const uint32_t expected_formats_4[] = {
        V4L2_PIX_FMT_GREY,
    };
    test_csi_enum_format(ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE, expected_formats_4, ARRAY_SIZE(expected_formats_4));

    const esp_cam_sensor_output_format_t input_sensor_formats_5[] = {
        ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE,
        ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV,
        ESP_CAM_SENSOR_PIXFORMAT_YUV420,
    };
    for (int i = 0; i < ARRAY_SIZE(input_sensor_formats_5); i++) {
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
        const uint32_t expected_formats_5[] = {
            V4L2_PIX_FMT_RGB565,
            V4L2_PIX_FMT_UYVY,
            V4L2_PIX_FMT_YUYV,
            V4L2_PIX_FMT_VYUY,
            V4L2_PIX_FMT_YVYU,
            V4L2_PIX_FMT_YUV420
        };
#else
        uint32_t expected_formats_5[1];
        expected_formats_5[0] = sensor_to_v4l2_format(input_sensor_formats_5[i]);
#endif

        test_csi_enum_format(input_sensor_formats_5[i], expected_formats_5, ARRAY_SIZE(expected_formats_5));
    }


    const esp_cam_sensor_output_format_t input_sensor_formats_6[] = {
        ESP_CAM_SENSOR_PIXFORMAT_RGB888
    };
    for (int i = 0; i < ARRAY_SIZE(input_sensor_formats_6); i++) {
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
        const uint32_t expected_formats_6[] = {
            V4L2_PIX_FMT_RGB565,
            V4L2_PIX_FMT_RGB24,
            V4L2_PIX_FMT_UYVY,
            V4L2_PIX_FMT_YUYV,
            V4L2_PIX_FMT_VYUY,
            V4L2_PIX_FMT_YVYU,
            V4L2_PIX_FMT_YUV420,
        };
#else
        uint32_t expected_formats_6[1];
        expected_formats_6[0] = sensor_to_v4l2_format(input_sensor_formats_6[i]);
#endif

        test_csi_enum_format(input_sensor_formats_6[i], expected_formats_6, ARRAY_SIZE(expected_formats_6));
    }
}

TEST_CASE("Test format convert when ISP is in bypass mode", "[video]")
{
    /**
     * Step 1: Test CSI convert format
     */
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
    esp_cam_sensor_output_format_t sensor_fmt_1[] = {
        ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE,
        ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV,
        ESP_CAM_SENSOR_PIXFORMAT_YUV420,
        ESP_CAM_SENSOR_PIXFORMAT_RGB888,
    };
    const uint32_t expected_formats_1[] = {
        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_YUV420,
        V4L2_PIX_FMT_UYVY,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_VYUY,
        V4L2_PIX_FMT_YVYU
    };
    int expected_count_1 = sizeof(expected_formats_1) / sizeof(expected_formats_1[0]);

    for (int i = 0; i < sizeof(sensor_fmt_1) / sizeof(sensor_fmt_1[0]); i++) {
        bool all_supported = validate_supported_formats(sensor_fmt_1[i], expected_formats_1, expected_count_1);
        TEST_ASSERT(all_supported);
    }
#endif

    /**
     * Step 2: Test CSI bypass
     */
    esp_cam_sensor_output_format_t sensor_fmt_2[] = {
        ESP_CAM_SENSOR_PIXFORMAT_RGB888,
        ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,

        ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        ESP_CAM_SENSOR_PIXFORMAT_RAW12,

        ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE,
        ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV,
        ESP_CAM_SENSOR_PIXFORMAT_YUV420,
    };
    const uint32_t expected_formats_2[] = {
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_GREY,

        V4L2_PIX_FMT_SBGGR8,
        V4L2_PIX_FMT_SBGGR10,
        V4L2_PIX_FMT_SBGGR12,

        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_UYVY,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_YUV420,
    };

    for (int i = 0; i < sizeof(sensor_fmt_2) / sizeof(sensor_fmt_2[0]); i++) {
        bool all_supported = validate_supported_formats(sensor_fmt_2[i], &expected_formats_2[i], 1);
        TEST_ASSERT(all_supported);
    }
}

TEST_CASE("Test format convert when ISP is in enable(on-bypass) mode", "[video]")
{
    /**
     * Step 1: Test CSI convert format
     */
    esp_cam_sensor_output_format_t sensor_fmt_1[] = {
        ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        ESP_CAM_SENSOR_PIXFORMAT_RAW10,
    };
    const uint32_t expected_formats_1[] = {
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_YUV420,
        V4L2_PIX_FMT_UYVY,
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_VYUY,
        V4L2_PIX_FMT_YVYU,
#endif
    };
    int expected_count_1 = sizeof(expected_formats_1) / sizeof(expected_formats_1[0]);

    for (int i = 0; i < sizeof(sensor_fmt_1) / sizeof(sensor_fmt_1[0]); i++) {
        bool all_supported = validate_supported_formats(sensor_fmt_1[i], expected_formats_1, expected_count_1);
        TEST_ASSERT(all_supported);
    }

    /**
     * Step 2: Test CSI bypass
     */
    esp_cam_sensor_output_format_t sensor_fmt_2[] = {
        ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        ESP_CAM_SENSOR_PIXFORMAT_RAW10,
    };
    const uint32_t expected_formats_2[] = {
        V4L2_PIX_FMT_SBGGR8,
        V4L2_PIX_FMT_SBGGR10,
    };

    for (int i = 0; i < sizeof(sensor_fmt_2) / sizeof(sensor_fmt_2[0]); i++) {
        bool all_supported = validate_supported_formats(sensor_fmt_2[i], &expected_formats_2[i], 1);
        TEST_ASSERT(all_supported);
    }
}

TEST_CASE("Test ISP bypass mode and requested format is 0", "[video]")
{
    esp_cam_sensor_output_format_t sensor_fmt_1[] = {
        ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        ESP_CAM_SENSOR_PIXFORMAT_RGB565,
        ESP_CAM_SENSOR_PIXFORMAT_RGB888,
        ESP_CAM_SENSOR_PIXFORMAT_YUV420,
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
        ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY,
        ESP_CAM_SENSOR_PIXFORMAT_YUV422_YUYV,
#else
        ESP_CAM_SENSOR_PIXFORMAT_YUV422,
#endif
    };
    const uint32_t expected_formats_1[] = {
        CAM_CTLR_COLOR_RAW8,
        CAM_CTLR_COLOR_RAW10,
        CAM_CTLR_COLOR_RGB565,
        CAM_CTLR_COLOR_RGB888,
        CAM_CTLR_COLOR_YUV420,
#if ESP_VIDEO_CSI_DEVICE_CONV_FORMAT
        CAM_CTLR_COLOR_YUV422_UYVY,
        CAM_CTLR_COLOR_YUV422_YUYV,
#else
        CAM_CTLR_COLOR_YUV422,
#endif
    };

    for (int i = 0; i < sizeof(sensor_fmt_1) / sizeof(sensor_fmt_1[0]); i++) {
        esp_video_csi_isp_in_out_format_t in_out_format = {0};
        esp_err_t ret = esp_video_csi_check_format(sensor_fmt_1[i], 0, &in_out_format);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(true, in_out_format.isp_bypass_required);
        TEST_ASSERT_EQUAL_HEX(expected_formats_1[i], in_out_format.csi_output_fmt);
    }
}
