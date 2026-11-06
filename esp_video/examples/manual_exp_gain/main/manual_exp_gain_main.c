/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_board_manager_includes.h"
#include "dev_display_lcd.h"
#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"
#include "linux/v4l2-controls.h"
#include "linux/videodev2.h"

/* PPA scale can take longer than one frame; extra buffers keep CSI fed. */
#define BUFFER_COUNT            4
#define STREAM_TASK_STACK       8192
#define STREAM_TASK_PRIORITY    5
#define DQBUF_TIMEOUT_MS        200
#define STREAM_STOP_WAIT_MS     3000
#define EXPOSURE_V4L2_UNIT_US   100

#define LCD_RGB565_BPP          2
#define PPA_FRAG_MAX            16

static const char *TAG = "manual_exp_gain";

static int s_fd = -1;
static uint32_t s_width;
static uint32_t s_height;
static uint32_t s_pixfmt;
static uint8_t *s_cam_buf[BUFFER_COUNT];
static size_t s_cam_buf_len[BUFFER_COUNT];
static volatile bool s_run;
static TaskHandle_t s_stream_task;
static TaskHandle_t s_main_task;
static esp_lcd_panel_handle_t s_lcd_panel;
static uint16_t s_lcd_w;
static uint16_t s_lcd_h;
static size_t s_lcd_fb_bytes;
static void *s_lcd_fb[2];
static uint8_t s_ppa_fb_idx;
static ppa_client_handle_t s_ppa;

static void print_help(void)
{
    ESP_LOGI(TAG, "commands:");
    ESP_LOGI(TAG, "  show              print AGC status, exposure (us), and gain");
    ESP_LOGI(TAG, "  exp <us>          set exposure in microseconds (V4L2 ABSOLUTE, 100 us/step)");
    ESP_LOGI(TAG, "  gain <x>          set gain multiplier, e.g. 1.0 / 2.5");
    ESP_LOGI(TAG, "  agc off           disable AGC (manual exposure/gain take effect)");
    ESP_LOGI(TAG, "  agc on            enable AGC again");
    ESP_LOGI(TAG, "  help              print this help");
    ESP_LOGI(TAG, "  quit              stop the stream");
}

static void trim_line(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
        line[--len] = '\0';
    }
}

static esp_err_t set_ctrl_int(uint32_t ctrl_class, uint32_t id, int32_t value)
{
    struct v4l2_ext_control ctl = {
        .id = id,
        .value = value,
    };
    struct v4l2_ext_controls ctls = {
        .ctrl_class = ctrl_class,
        .count = 1,
        .controls = &ctl,
    };

    if (ioctl(s_fd, VIDIOC_S_EXT_CTRLS, &ctls) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t get_ctrl_int(uint32_t ctrl_class, uint32_t id, int32_t *out)
{
    struct v4l2_ext_control ctl = {
        .id = id,
    };
    struct v4l2_ext_controls ctls = {
        .ctrl_class = ctrl_class,
        .count = 1,
        .controls = &ctl,
    };

    if (ioctl(s_fd, VIDIOC_G_EXT_CTRLS, &ctls) != 0) {
        return ESP_FAIL;
    }
    *out = ctl.value;
    return ESP_OK;
}

static esp_err_t get_exposure_us(int *exp_us)
{
    int32_t v = 0;

    if (get_ctrl_int(V4L2_CID_CAMERA_CLASS, V4L2_CID_EXPOSURE_ABSOLUTE, &v) != ESP_OK) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    *exp_us = (int)v * EXPOSURE_V4L2_UNIT_US;
    return ESP_OK;
}

static esp_err_t set_exposure_us(int exposure_us)
{
    struct v4l2_query_ext_ctrl qctrl = {
        .id = V4L2_CID_EXPOSURE_ABSOLUTE,
    };
    int32_t value;

    if (exposure_us < EXPOSURE_V4L2_UNIT_US) {
        exposure_us = EXPOSURE_V4L2_UNIT_US;
    }

    if (ioctl(s_fd, VIDIOC_QUERY_EXT_CTRL, &qctrl) != 0) {
        ESP_LOGE(TAG, "EXPOSURE_ABSOLUTE is not supported (errno=%d)", errno);
        return ESP_ERR_NOT_SUPPORTED;
    }

    value = exposure_us / EXPOSURE_V4L2_UNIT_US;
    if (value < (int32_t)qctrl.minimum) {
        value = (int32_t)qctrl.minimum;
    }
    if (value > (int32_t)qctrl.maximum) {
        value = (int32_t)qctrl.maximum;
    }
    if (set_ctrl_int(V4L2_CID_CAMERA_CLASS, V4L2_CID_EXPOSURE_ABSOLUTE, value) != ESP_OK) {
        ESP_LOGE(TAG, "set EXPOSURE_ABSOLUTE failed (errno=%d)", errno);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "set exposure %" PRId32 " us (abs=%" PRId32 ")",
             (int32_t)(value * EXPOSURE_V4L2_UNIT_US), value);
    return ESP_OK;
}

static esp_err_t get_gain_x(float *gain_x)
{
    int32_t index = 0;
    struct v4l2_querymenu qmenu = {
        .id = V4L2_CID_GAIN,
    };

    if (get_ctrl_int(V4L2_CID_USER_CLASS, V4L2_CID_GAIN, &index) != ESP_OK) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    qmenu.index = (uint32_t)index;
    if (ioctl(s_fd, VIDIOC_QUERYMENU, &qmenu) != 0) {
        *gain_x = (float)index;
        ESP_LOGW(TAG, "QUERYMENU gain failed, using index=%" PRId32, index);
        return ESP_OK;
    }
    *gain_x = (float)qmenu.value / 1000.0f;
    return ESP_OK;
}

static esp_err_t set_gain_x(float gain_x)
{
    struct v4l2_query_ext_ctrl qctrl = {
        .id = V4L2_CID_GAIN,
    };
    int32_t gain_value = 0;
    int32_t index = -1;
    int32_t base = 1000;

    if (!(gain_x > 0.0f) || !isfinite(gain_x)) {
        ESP_LOGE(TAG, "gain must be > 0");
        return ESP_ERR_INVALID_ARG;
    }
    if (ioctl(s_fd, VIDIOC_QUERY_EXT_CTRL, &qctrl) != 0) {
        ESP_LOGE(TAG, "QUERY_EXT_CTRL GAIN failed (errno=%d)", errno);
        return ESP_ERR_NOT_SUPPORTED;
    }

    for (int32_t i = (int32_t)qctrl.minimum; i <= (int32_t)qctrl.maximum; i++) {
        struct v4l2_querymenu qmenu = {
            .id = V4L2_CID_GAIN,
            .index = (uint32_t)i,
        };
        if (ioctl(s_fd, VIDIOC_QUERYMENU, &qmenu) != 0) {
            continue;
        }
        if (i == (int32_t)qctrl.minimum) {
            base = (int32_t)qmenu.value;
            if (base <= 0) {
                base = 1000;
            }
            gain_value = (int32_t)lroundf((float)base * gain_x);
        }
    }

    for (int32_t i = (int32_t)qctrl.minimum; i < (int32_t)qctrl.maximum; i++) {
        struct v4l2_querymenu q0 = {
            .id = V4L2_CID_GAIN,
            .index = (uint32_t)i,
        };
        struct v4l2_querymenu q1 = {
            .id = V4L2_CID_GAIN,
            .index = (uint32_t)(i + 1),
        };
        int32_t g0;
        int32_t g1;

        if (ioctl(s_fd, VIDIOC_QUERYMENU, &q0) != 0 || ioctl(s_fd, VIDIOC_QUERYMENU, &q1) != 0) {
            continue;
        }
        g0 = (int32_t)q0.value;
        g1 = (int32_t)q1.value;
        if (gain_value >= g0 && gain_value <= g1) {
            index = ((gain_value - g0) > (g1 - gain_value)) ? (i + 1) : i;
            break;
        }
    }

    if (index < 0) {
        struct v4l2_querymenu qmin = {
            .id = V4L2_CID_GAIN,
            .index = (uint32_t)qctrl.minimum,
        };
        struct v4l2_querymenu qmax = {
            .id = V4L2_CID_GAIN,
            .index = (uint32_t)qctrl.maximum,
        };
        if (ioctl(s_fd, VIDIOC_QUERYMENU, &qmin) == 0 && gain_value <= (int32_t)qmin.value) {
            index = (int32_t)qctrl.minimum;
        } else if (ioctl(s_fd, VIDIOC_QUERYMENU, &qmax) == 0) {
            index = (int32_t)qctrl.maximum;
        } else {
            index = (int32_t)qctrl.minimum;
        }
    }

    if (set_ctrl_int(V4L2_CID_USER_CLASS, V4L2_CID_GAIN, index) != ESP_OK) {
        ESP_LOGE(TAG, "set GAIN index failed (errno=%d)", errno);
        return ESP_FAIL;
    }

    {
        float applied = 0.0f;
        if (get_gain_x(&applied) == ESP_OK) {
            ESP_LOGI(TAG, "set gain %.3fx -> idx=%" PRId32 " (%.3fx)", gain_x, index, applied);
        } else {
            ESP_LOGI(TAG, "set gain %.3fx -> idx=%" PRId32, gain_x, index);
        }
    }
    return ESP_OK;
}

static void print_show(void)
{
    esp_video_isp_pipeline_agc_status_t agc = ESP_VIDEO_ISP_PIPELINE_AGC_DISABLE;
    int exp_us = 0;
    float gain_x = 0.0f;
    esp_err_t ret;

    ret = esp_video_isp_pipeline_get_agc_status(&agc);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "agc=%s", agc == ESP_VIDEO_ISP_PIPELINE_AGC_ENABLE ? "on" : "off");
    } else {
        ESP_LOGW(TAG, "get agc status failed: %s", esp_err_to_name(ret));
    }

    if (get_exposure_us(&exp_us) == ESP_OK) {
        ESP_LOGI(TAG, "exposure=%d us", exp_us);
    } else {
        ESP_LOGW(TAG, "exposure time is not available");
    }

    if (get_gain_x(&gain_x) == ESP_OK) {
        ESP_LOGI(TAG, "gain=%.3fx", gain_x);
    } else {
        ESP_LOGW(TAG, "get gain failed (errno=%d)", errno);
    }
    ESP_LOGI(TAG, "frame=%" PRIu32 "x%" PRIu32 " pixfmt=0x%08" PRIx32, s_width, s_height, s_pixfmt);
}

static void set_agc(bool enable)
{
    esp_err_t ret = esp_video_isp_pipeline_set_agc_status(
                        enable ? ESP_VIDEO_ISP_PIPELINE_AGC_ENABLE : ESP_VIDEO_ISP_PIPELINE_AGC_DISABLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set agc %s failed: %s", enable ? "on" : "off", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "agc %s", enable ? "on" : "off");
}

static void handle_line(char *line)
{
    unsigned exp_us;
    float gain_x;
    bool wrote = false;

    trim_line(line);
    if (line[0] == '\0') {
        return;
    }

    if (strcmp(line, "help") == 0) {
        print_help();
    } else if (strcmp(line, "show") == 0) {
        print_show();
    } else if (strcmp(line, "agc off") == 0) {
        set_agc(false);
        wrote = true;
    } else if (strcmp(line, "agc on") == 0) {
        set_agc(true);
        wrote = true;
    } else if (sscanf(line, "exp %u", &exp_us) == 1) {
        if (set_exposure_us((int)exp_us) == ESP_OK) {
            wrote = true;
        }
    } else if (sscanf(line, "gain %f", &gain_x) == 1) {
        if (set_gain_x(gain_x) == ESP_OK) {
            wrote = true;
        }
    } else if (strcmp(line, "quit") == 0) {
        s_run = false;
    } else {
        ESP_LOGW(TAG, "unknown command: %s", line);
        print_help();
    }

    if (wrote) {
        ESP_LOGI(TAG, "read back after write:");
        print_show();
    }
}

static ppa_srm_color_mode_t cam_ppa_color_mode(uint32_t fourcc)
{
    if (fourcc == V4L2_PIX_FMT_UYVY) {
        return PPA_SRM_COLOR_MODE_YUV422_UYVY;
    }
    return PPA_SRM_COLOR_MODE_YUV420;
}

static esp_err_t init_lcd(void)
{
    esp_err_t ret;
    dev_display_lcd_handles_t *lcd_handles = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;

    /* LDO_MIPI is acquired in app_main before example_video_init(). */
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    ESP_RETURN_ON_ERROR(ret, TAG, "init display LCD");
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD backlight init skipped: %s", esp_err_to_name(ret));
    }

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_handles);
    ESP_RETURN_ON_ERROR(ret, TAG, "get LCD handle");
    ESP_RETURN_ON_FALSE(lcd_handles && lcd_handles->panel_handle, ESP_ERR_INVALID_STATE, TAG, "LCD panel null");
    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_cfg);
    ESP_RETURN_ON_ERROR(ret, TAG, "get LCD config");
    ESP_RETURN_ON_FALSE(lcd_cfg && lcd_cfg->lcd_width > 0 && lcd_cfg->lcd_height > 0,
                        ESP_ERR_INVALID_STATE, TAG, "LCD size invalid");

    s_lcd_panel = lcd_handles->panel_handle;
    s_lcd_w = lcd_cfg->lcd_width;
    s_lcd_h = lcd_cfg->lcd_height;
    s_lcd_fb_bytes = (size_t)s_lcd_w * s_lcd_h * LCD_RGB565_BPP;
    s_lcd_fb[0] = heap_caps_aligned_alloc(64, s_lcd_fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    s_lcd_fb[1] = heap_caps_aligned_alloc(64, s_lcd_fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    ESP_RETURN_ON_FALSE(s_lcd_fb[0] && s_lcd_fb[1], ESP_ERR_NO_MEM, TAG, "LCD FB alloc");
    s_ppa_fb_idx = 1;

    ESP_LOGI(TAG, "LCD %" PRIu16 "x%" PRIu16 " ready (board manager)", s_lcd_w, s_lcd_h);
    return ESP_OK;
}

static esp_err_t init_ppa(void)
{
    ppa_client_config_t cfg = {
        .oper_type = PPA_OPERATION_SRM,
    };

    return ppa_register_client(&cfg, &s_ppa);
}

static void draw_frame_to_lcd(uint8_t *frame)
{
    float sx;
    float sy;
    uint32_t sx_int;
    uint32_t sy_int;
    uint32_t sx_frag;
    uint32_t sy_frag;
    float sx_use;
    float sy_use;
    void *back_fb;
    ppa_srm_oper_config_t srm;

    if (!s_ppa || !s_lcd_fb[0] || !s_lcd_fb[1] || !s_lcd_panel || s_width == 0 || s_height == 0) {
        return;
    }

    sx = (float)s_lcd_w / (float)s_width;
    sy = (float)s_lcd_h / (float)s_height;
    sx_int = (uint32_t)sx;
    sy_int = (uint32_t)sy;
    sx_frag = ((uint32_t)(sx * PPA_FRAG_MAX) & (PPA_FRAG_MAX - 1)) & ~1u;
    sy_frag = (uint32_t)(sy * PPA_FRAG_MAX) & (PPA_FRAG_MAX - 1);
    sx_use = (float)sx_int + (float)sx_frag / PPA_FRAG_MAX;
    sy_use = (float)sy_int + (float)sy_frag / PPA_FRAG_MAX;
    if (sx_use < 1.0f / PPA_FRAG_MAX) {
        sx_use = 1.0f / PPA_FRAG_MAX;
    }
    if (sy_use < 1.0f / PPA_FRAG_MAX) {
        sy_use = 1.0f / PPA_FRAG_MAX;
    }

    back_fb = s_lcd_fb[s_ppa_fb_idx];
    memset(back_fb, 0, s_lcd_fb_bytes);
    (void)esp_cache_msync(back_fb, s_lcd_fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    memset(&srm, 0, sizeof(srm));
    srm.in.buffer = frame;
    srm.in.pic_w = s_width;
    srm.in.pic_h = s_height;
    srm.in.block_w = s_width;
    srm.in.block_h = s_height;
    srm.in.srm_cm = cam_ppa_color_mode(s_pixfmt);
    srm.in.yuv_range = PPA_COLOR_RANGE_LIMIT;
    srm.in.yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601;
    srm.out.buffer = back_fb;
    srm.out.buffer_size = s_lcd_fb_bytes;
    srm.out.pic_w = s_lcd_w;
    srm.out.pic_h = s_lcd_h;
    srm.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    srm.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
    srm.scale_x = sx_use;
    srm.scale_y = sy_use;
    srm.mode = PPA_TRANS_MODE_BLOCKING;

    if (ppa_do_scale_rotate_mirror(s_ppa, &srm) != ESP_OK) {
        ESP_LOGE(TAG, "PPA scale failed");
        return;
    }
    (void)esp_cache_msync(back_fb, s_lcd_fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, 0, s_lcd_w, s_lcd_h, back_fb);
    s_ppa_fb_idx ^= 1;
}

static void stream_task(void *arg)
{
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bool logged = false;

    (void)arg;
    while (s_run) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(s_fd, VIDIOC_DQBUF, &buf) != 0) {
            if (errno == ETIMEDOUT) {
                continue;
            }
            if (s_run) {
                ESP_LOGE(TAG, "failed to receive video frame (errno=%d)", errno);
            }
            break;
        }
        if ((buf.flags & V4L2_BUF_FLAG_DONE) && buf.index < BUFFER_COUNT && s_cam_buf[buf.index]) {
            draw_frame_to_lcd(s_cam_buf[buf.index]);
            if (!logged) {
                ESP_LOGI(TAG, "stream frames started");
                logged = true;
            }
        }
        if (ioctl(s_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            break;
        }
    }

    s_run = false;
    s_stream_task = NULL;
    xTaskNotifyGive(s_main_task);
    vTaskDelete(NULL);
}

static esp_err_t start_stream(void)
{
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format format = {
        .type = type,
    };
    struct v4l2_requestbuffers req;
    struct timeval dqbuf_timeout = {
        .tv_sec = DQBUF_TIMEOUT_MS / 1000,
        .tv_usec = (DQBUF_TIMEOUT_MS % 1000) * 1000,
    };
    const uint32_t try_fmts[] = { V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_UYVY };

    s_fd = open(EXAMPLE_CAM_DEV_PATH, O_RDWR);
    if (s_fd < 0) {
        ESP_LOGE(TAG, "failed to open %s (errno=%d)", EXAMPLE_CAM_DEV_PATH, errno);
        return ESP_FAIL;
    }

    if (ioctl(s_fd, VIDIOC_S_DQBUF_TIMEOUT, &dqbuf_timeout) != 0) {
        ESP_LOGE(TAG, "failed to set DQBUF timeout (errno=%d)", errno);
        goto fail;
    }

    if (ioctl(s_fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        goto fail;
    }
    if (format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUV420 &&
            format.fmt.pix.pixelformat != V4L2_PIX_FMT_UYVY) {
        bool accepted = false;

        for (size_t i = 0; i < sizeof(try_fmts) / sizeof(try_fmts[0]); i++) {
            format.fmt.pix.pixelformat = try_fmts[i];
            if (ioctl(s_fd, VIDIOC_S_FMT, &format) != 0) {
                continue;
            }
            if (ioctl(s_fd, VIDIOC_G_FMT, &format) != 0) {
                ESP_LOGE(TAG, "failed to get format");
                goto fail;
            }
            if (format.fmt.pix.pixelformat == try_fmts[i]) {
                accepted = true;
                break;
            }
        }
        if (!accepted) {
            ESP_LOGE(TAG, "camera format is not YUV420 or UYVY");
            goto fail;
        }
    }
    s_width = format.fmt.pix.width;
    s_height = format.fmt.pix.height;
    s_pixfmt = format.fmt.pix.pixelformat;
    if (s_width == 0 || s_height == 0) {
        ESP_LOGE(TAG, "invalid frame size");
        goto fail;
    }

    memset(&req, 0, sizeof(req));
    req.count = BUFFER_COUNT;
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "failed to require buffer");
        goto fail;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(s_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to query buffer");
            goto fail;
        }
        s_cam_buf_len[i] = buf.length;
        s_cam_buf[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, s_fd, buf.m.offset);
        if (s_cam_buf[i] == MAP_FAILED) {
            s_cam_buf[i] = NULL;
            ESP_LOGE(TAG, "failed to map buffer");
            goto fail;
        }
        if (ioctl(s_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            goto fail;
        }
    }

    if (ioctl(s_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "failed to start stream");
        goto fail;
    }

    s_run = true;
    s_main_task = xTaskGetCurrentTaskHandle();
    if (xTaskCreate(stream_task, "exp_stream", STREAM_TASK_STACK, NULL,
                    STREAM_TASK_PRIORITY, &s_stream_task) != pdPASS) {
        ESP_LOGE(TAG, "failed to create stream task");
        ioctl(s_fd, VIDIOC_STREAMOFF, &type);
        goto fail;
    }

    ESP_LOGI(TAG, "camera %s %" PRIu32 "x%" PRIu32 " on LCD %" PRIu16 "x%" PRIu16,
             EXAMPLE_CAM_DEV_PATH, s_width, s_height, s_lcd_w, s_lcd_h);
    set_agc(false);
    print_show();
    print_help();
    return ESP_OK;

fail:
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (s_cam_buf[i]) {
            munmap(s_cam_buf[i], s_cam_buf_len[i]);
            s_cam_buf[i] = NULL;
        }
    }
    close(s_fd);
    s_fd = -1;
    return ESP_FAIL;
}

static void stop_stream(void)
{
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    s_run = false;
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(STREAM_STOP_WAIT_MS)) == 0) {
        ESP_LOGE(TAG, "stream task did not exit within %d ms; mapped buffers and fd stay open",
                 STREAM_STOP_WAIT_MS);
        return;
    }
    if (s_fd >= 0 && ioctl(s_fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGW(TAG, "STREAMOFF failed (errno=%d)", errno);
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (s_cam_buf[i]) {
            munmap(s_cam_buf[i], s_cam_buf_len[i]);
            s_cam_buf[i] = NULL;
        }
    }
    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
    }
    ESP_LOGI(TAG, "stream stopped");
}

static void console_write(const char *s, size_t len)
{
    /* Same path as ESP_LOGI (USB Serial/JTAG). esp_rom_printf goes to UART0. */
    if (len == 0) {
        return;
    }
    fwrite(s, 1, len, stdout);
    fflush(stdout);
}

static void console_putc(char c)
{
    console_write(&c, 1);
}

static int read_cmd_line(char *buf, size_t buflen)
{
    size_t n = 0;

    /*
     * Do not use fgets(): USB Serial/JTAG often returns EOF between keystrokes,
     * and fgets then yields a partial line (e.g. only "o") before Enter.
     * Echo every key on stdout so typing is visible immediately.
     */
    console_write("cmd> ", 5);
    while (s_run) {
        int c = fgetc(stdin);

        if (c == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            console_write("\r\n", 2);
            buf[n] = '\0';
            return (int)n;
        }
        if (c == '\b' || c == 0x7f) {
            if (n > 0) {
                n--;
                console_write("\b \b", 3);
            }
            continue;
        }
        if (c < 32 || c > 126) {
            continue;
        }
        if (n + 1 < buflen) {
            buf[n++] = (char)c;
            console_putc((char)c);
        }
    }
    buf[n] = '\0';
    return -1;
}

static void command_loop(void)
{
    char line[64];

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    while (s_run) {
        if (read_cmd_line(line, sizeof(line)) < 0) {
            break;
        }
        handle_line(line);
    }
}

void app_main(void)
{
    esp_err_t ret;

    /* LDO_MIPI must be on before CSI/DSI; board manager owns the channel. */
    ret = esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
    ESP_GOTO_ON_ERROR(ret, exit, TAG, "init MIPI LDO");
    ret = example_video_init();
    ESP_GOTO_ON_ERROR(ret, exit, TAG, "camera init failed");
    ret = init_lcd();
    ESP_GOTO_ON_ERROR(ret, deinit, TAG, "LCD init failed");
    ret = init_ppa();
    ESP_GOTO_ON_ERROR(ret, deinit, TAG, "PPA init failed");
    ret = start_stream();
    ESP_GOTO_ON_ERROR(ret, deinit, TAG, "camera stream failed");

    command_loop();
    stop_stream();

deinit:
    ESP_ERROR_CHECK(example_video_deinit());
exit:
    return;
}
