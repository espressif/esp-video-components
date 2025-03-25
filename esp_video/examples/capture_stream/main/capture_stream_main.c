/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "linux/videodev2.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#include "driver/i2c_master.h"
#endif

#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
#include "esp_heap_caps.h"

#define MEMORY_TYPE V4L2_MEMORY_USERPTR
#define MEMORY_ALIGN 64
#else
#define MEMORY_TYPE V4L2_MEMORY_MMAP
#endif

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#define CAM_DEV_PATH ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#elif CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define CAM_DEV_PATH ESP_VIDEO_DVP_DEVICE_NAME
#endif

#define BUFFER_COUNT 2
#define CAPTURE_SECONDS 3

static const char *TAG = "example";

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
/**
 * @brief i2c master initialization
 * The Camera device uses the I2C bus as the control bus for the camera sensor.
 * Explicitly initializing the I2C bus in the application will allow you to use this I2C master in multiple tasks.
 *
 * @param[out] bus_handle Pointer to store the initialized I2C bus handle
 * @return None
 */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, uint8_t port, uint8_t scl_pin, uint8_t sda_pin)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = port,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
}
#endif

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
static const esp_video_init_csi_config_t csi_config[] = {
    {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
                .scl_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
                .sda_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
            },
            .freq = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
    },
};
#else
static esp_video_init_csi_config_t csi_config[] = {
    {
        .sccb_config = {
            .init_sccb = false,
            .freq = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
    },
};
#endif // CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#endif // CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR

#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
static const esp_video_init_dvp_config_t dvp_config[] = {
    {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port      = CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT,
                .scl_pin   = CONFIG_EXAMPLE_DVP_SCCB_I2C_SCL_PIN,
                .sda_pin   = CONFIG_EXAMPLE_DVP_SCCB_I2C_SDA_PIN,
            },
            .freq      = CONFIG_EXAMPLE_DVP_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN,
        .dvp_pin = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                CONFIG_EXAMPLE_DVP_D0_PIN, CONFIG_EXAMPLE_DVP_D1_PIN, CONFIG_EXAMPLE_DVP_D2_PIN, CONFIG_EXAMPLE_DVP_D3_PIN,
                CONFIG_EXAMPLE_DVP_D4_PIN, CONFIG_EXAMPLE_DVP_D5_PIN, CONFIG_EXAMPLE_DVP_D6_PIN, CONFIG_EXAMPLE_DVP_D7_PIN,
            },
            .vsync_io = CONFIG_EXAMPLE_DVP_VSYNC_PIN,
            .de_io = CONFIG_EXAMPLE_DVP_DE_PIN,
            .pclk_io = CONFIG_EXAMPLE_DVP_PCLK_PIN,
            .xclk_io = CONFIG_EXAMPLE_DVP_XCLK_PIN,
        },
        .xclk_freq = CONFIG_EXAMPLE_DVP_XCLK_FREQ,
    }
};
#else
static esp_video_init_dvp_config_t dvp_config[] = {
    {
        .sccb_config = {
            .init_sccb = false,
            .freq      = CONFIG_EXAMPLE_DVP_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN,
        .dvp_pin = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                CONFIG_EXAMPLE_DVP_D0_PIN, CONFIG_EXAMPLE_DVP_D1_PIN, CONFIG_EXAMPLE_DVP_D2_PIN, CONFIG_EXAMPLE_DVP_D3_PIN,
                CONFIG_EXAMPLE_DVP_D4_PIN, CONFIG_EXAMPLE_DVP_D5_PIN, CONFIG_EXAMPLE_DVP_D6_PIN, CONFIG_EXAMPLE_DVP_D7_PIN,
            },
            .vsync_io = CONFIG_EXAMPLE_DVP_VSYNC_PIN,
            .de_io = CONFIG_EXAMPLE_DVP_DE_PIN,
            .pclk_io = CONFIG_EXAMPLE_DVP_PCLK_PIN,
            .xclk_io = CONFIG_EXAMPLE_DVP_XCLK_PIN,
        },
        .xclk_freq = CONFIG_EXAMPLE_DVP_XCLK_FREQ,
    },
};
#endif
#endif

static const esp_video_init_config_t cam_config = {
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
    .csi      = csi_config,
#endif
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
    .dvp      = dvp_config,
#endif
};

static esp_err_t camera_capture_stream(void)
{
    int fd;
    esp_err_t ret;
    int fmt_index = 0;
    uint32_t frame_size;
    uint32_t frame_count;
    struct v4l2_buffer buf;
    uint8_t *buffer[BUFFER_COUNT];
#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
    uint32_t buffer_size[BUFFER_COUNT];
#endif
    struct v4l2_format init_format;
    struct v4l2_requestbuffers req;
    struct v4l2_capability capability;
#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP || CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
#endif
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fd = open(CAM_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device");
        return ESP_FAIL;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        ret = ESP_FAIL;
        goto exit_0;
    }

    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);
    ESP_LOGI(TAG, "capabilities:");
    if (capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
    }
    if (capability.capabilities & V4L2_CAP_READWRITE) {
        ESP_LOGI(TAG, "\tREADWRITE");
    }
    if (capability.capabilities & V4L2_CAP_ASYNCIO) {
        ESP_LOGI(TAG, "\tASYNCIO");
    }
    if (capability.capabilities & V4L2_CAP_STREAMING) {
        ESP_LOGI(TAG, "\tSTREAMING");
    }
    if (capability.capabilities & V4L2_CAP_META_OUTPUT) {
        ESP_LOGI(TAG, "\tMETA_OUTPUT");
    }
    if (capability.capabilities & V4L2_CAP_DEVICE_CAPS) {
        ESP_LOGI(TAG, "device capabilities:");
        if (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
            ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
        }
        if (capability.device_caps & V4L2_CAP_READWRITE) {
            ESP_LOGI(TAG, "\tREADWRITE");
        }
        if (capability.device_caps & V4L2_CAP_ASYNCIO) {
            ESP_LOGI(TAG, "\tASYNCIO");
        }
        if (capability.device_caps & V4L2_CAP_STREAMING) {
            ESP_LOGI(TAG, "\tSTREAMING");
        }
        if (capability.device_caps & V4L2_CAP_META_OUTPUT) {
            ESP_LOGI(TAG, "\tMETA_OUTPUT");
        }
    }

    memset(&init_format, 0, sizeof(struct v4l2_format));
    init_format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &init_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        ret = ESP_FAIL;
        goto exit_0;
    }

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_VFLIP;
    control[0].value    = 1;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to mirror the frame horizontally and skip this step");
    }
#endif

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_HFLIP;
    control[0].value    = 1;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to mirror the frame horizontally and skip this step");
    }
#endif

    while (1) {
        struct v4l2_fmtdesc fmtdesc = {
            .index = fmt_index++,
            .type = type,
        };

        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
            break;
        }

        struct v4l2_format format = {
            .type = type,
            .fmt.pix.width = init_format.fmt.pix.width,
            .fmt.pix.height = init_format.fmt.pix.height,
            .fmt.pix.pixelformat = fmtdesc.pixelformat,
        };

        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            if (errno == ESRCH) {
                continue;
            } else {
                ESP_LOGE(TAG, "failed to set format");
                ret = ESP_FAIL;
                goto exit_0;
            }
        }

        ESP_LOGI(TAG, "Capture %s format frames for %d seconds:", (char *)fmtdesc.description, CAPTURE_SECONDS);

        memset(&req, 0, sizeof(req));
        req.count  = BUFFER_COUNT;
        req.type   = type;
        req.memory = MEMORY_TYPE;
        if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
            ESP_LOGE(TAG, "failed to require buffer");
            ret = ESP_FAIL;
            goto exit_0;
        }

        for (int i = 0; i < BUFFER_COUNT; i++) {
            struct v4l2_buffer buf;

            memset(&buf, 0, sizeof(buf));
            buf.type        = type;
            buf.memory      = MEMORY_TYPE;
            buf.index       = i;
            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to query buffer");
                ret = ESP_FAIL;
                goto exit_0;
            }

#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
            buffer[i] = heap_caps_aligned_alloc(MEMORY_ALIGN, buf.length, MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED);
#else
            buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, buf.m.offset);
#endif
            if (!buffer[i]) {
                ESP_LOGE(TAG, "failed to map buffer");
                ret = ESP_FAIL;
                goto exit_0;
            }
#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
            else {
                buf.m.userptr = (unsigned long)buffer[i];
                buffer_size[i] = buf.length;
            }
#endif

            if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to queue video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }
        }

        if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
            ESP_LOGE(TAG, "failed to start stream");
            ret = ESP_FAIL;
            goto exit_0;
        }

        frame_count = 0;
        frame_size = 0;
        int64_t start_time_us = esp_timer_get_time();
        while (esp_timer_get_time() - start_time_us < (CAPTURE_SECONDS * 1000 * 1000)) {
            memset(&buf, 0, sizeof(buf));
            buf.type   = type;
            buf.memory = MEMORY_TYPE;
            if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to receive video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }

            frame_size += buf.bytesused;

#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
            buf.m.userptr = (unsigned long)buffer[buf.index];
            buf.length = buffer_size[buf.index];
#endif
            if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to queue video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }

            frame_count++;
        }

        if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "failed to stop stream");
            ret = ESP_FAIL;
            goto exit_0;
        }

#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
        for (int i = 0; i < BUFFER_COUNT; i++) {
            heap_caps_free(buffer[i]);
        }
#endif

        ESP_LOGI(TAG, "\twidth:  %" PRIu32, format.fmt.pix.width);
        ESP_LOGI(TAG, "\theight: %" PRIu32, format.fmt.pix.height);
        ESP_LOGI(TAG, "\tsize:   %" PRIu32, frame_size / frame_count);
        ESP_LOGI(TAG, "\tFPS:    %" PRIu32, frame_count / CAPTURE_SECONDS);
    }

    ret = ESP_OK;

exit_0:
    close(fd);
    return ret;
}

void app_main(void)
{
    esp_err_t ret = ESP_OK;
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
    i2c_master_init(&i2c_bus_handle, CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT, CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN, CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN);
    csi_config->sccb_config.i2c_handle = i2c_bus_handle;
#elif CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
    i2c_master_init(&i2c_bus_handle, CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT, CONFIG_EXAMPLE_DVP_SCCB_I2C_SCL_PIN, CONFIG_EXAMPLE_DVP_SCCB_I2C_SDA_PIN);
    dvp_config->sccb_config.i2c_handle = i2c_bus_handle;
#endif
#endif
    ret = esp_video_init(&cam_config);
    ESP_GOTO_ON_ERROR(ret, clean1, TAG, "Camera init failed");

    ret = camera_capture_stream();
    ESP_GOTO_ON_ERROR(ret, clean0, TAG, "Camera capture stream failed");

clean0:
    ESP_ERROR_CHECK(esp_video_deinit());
clean1:
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
    /* Todo, Add esp_video_deinit */
    ESP_ERROR_CHECK(i2c_del_master_bus(i2c_bus_handle));
#endif
    return;
}
