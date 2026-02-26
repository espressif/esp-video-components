/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include "linux/videodev2.h"
#include "esp_video_isp_ioctl.h"
#include "esp_err.h"
#include "esp_log.h"
#include "v4l2_cmd.h"

typedef enum v4l2_gamma_action {
    V4L2_GAMMA_DUMP = 0,
    V4L2_GAMMA_ENABLE,
    V4L2_GAMMA_DISABLE,
} v4l2_gamma_action_t;

static const char *TAG = "v4l2-gamma";

static struct {
    struct arg_lit *all;
    struct arg_lit *disable;
    struct arg_lit *enable;
    struct arg_str *dev;
    struct arg_dbl *gamma;
    struct arg_end *end;
} v4l2_gamma_main_arg;

static void gamma_action(const char *devname, v4l2_gamma_action_t action, esp_video_isp_gamma_point_t *points)
{
    int fd;
    esp_video_isp_gamma_t gamma;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    ESP_LOGD(TAG, "action=%s", action == V4L2_GAMMA_DUMP ? "dump" :
             action == V4L2_GAMMA_ENABLE ? "enable" :
             action == V4L2_GAMMA_DISABLE ? "disable" : "none");

    fd = open(devname, O_RDWR);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device=%s", devname);
        return;
    }

    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_USER_ESP_ISP_GAMMA;
    control[0].p_u8     = (uint8_t *)&gamma;
    control[0].size     = sizeof(gamma);
    if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "failed to get GAMMA control");
        goto exit;
    }

    if (action == V4L2_GAMMA_DUMP) {
        printf("\nGAMMA state:\t  %s\n", gamma.enable ? "enable" : "disable");
        printf("GAMMA coordinate array:\n");
        printf("\t\t         x,   y\n\n");
        for (int i = 0; i < ISP_GAMMA_CURVE_POINTS_NUM; i++) {
            printf("\t\t  %02d: {%3d, %3d}\n", i, i * 16, gamma.points[i].y);
        }
    } else if (action == V4L2_GAMMA_DISABLE) {
        gamma.enable = false;
        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set GAMMA control");
            goto exit;
        }
    } else if (action == V4L2_GAMMA_ENABLE) {
        gamma.enable = true;
        if (points) {
            memcpy(&gamma.points, points, sizeof(gamma.points));
        }
        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set GAMMA control");
            goto exit;
        }
    }

    ESP_LOGD(TAG, "success");

exit:
    close(fd);
}

static int v4l2_gamma_main(int argc, char **argv)
{
    char dev_name[16] = ESP_VIDEO_ISP1_DEVICE_NAME;

    EXAMPLE_V4L2_CMD_CHECK(v4l2_gamma_main_arg);

    if (v4l2_gamma_main_arg.dev->count) {
        if (decode_dev_name(v4l2_gamma_main_arg.dev->sval[0], dev_name, sizeof(dev_name))) {
            ESP_LOGE(TAG, "decode device name=%s error", v4l2_gamma_main_arg.dev->sval[0]);
            return -1;
        }
    }

    if (v4l2_gamma_main_arg.all->count) {
        gamma_action(dev_name, V4L2_GAMMA_DUMP, NULL);
    } else if (v4l2_gamma_main_arg.disable->count) {
        gamma_action(dev_name, V4L2_GAMMA_DISABLE, NULL);
    } else if (v4l2_gamma_main_arg.enable->count) {
        esp_video_isp_gamma_point_t *points_ptr = NULL;
        esp_video_isp_gamma_point_t points[ISP_GAMMA_CURVE_POINTS_NUM];

        if (v4l2_gamma_main_arg.gamma->count) {
            double gamma = v4l2_gamma_main_arg.gamma->dval[0];
            if (gamma <= 0.0 || gamma > 10.0) {
                ESP_LOGE(TAG, "gamma value must be in range (0, 10], got %f", gamma);
                return -1;
            }

            int max = UINT8_MAX + 1;
            int step = max / ISP_GAMMA_CURVE_POINTS_NUM;
            double gain = 1.0 / pow((double)(max - step) / UINT8_MAX, gamma);

            for (int i = 0; i < ISP_GAMMA_CURVE_POINTS_NUM; i++) {
                int x = i * step;

                points[i].x = x;
                points[i].y = (uint8_t)(MIN(pow((double)x / UINT8_MAX, gamma) * UINT8_MAX * gain + 0.5, UINT8_MAX));
            }

            points_ptr = points;
        }

        gamma_action(dev_name, V4L2_GAMMA_ENABLE, points_ptr);
    }

    return 0;
}

void v4l2_cmd_gamma_register(void)
{
    v4l2_gamma_main_arg.all = arg_lit0(NULL, "all", "display all information available");
    v4l2_gamma_main_arg.disable = arg_lit0(NULL, "disable", "disable GAMMA process");
    v4l2_gamma_main_arg.enable = arg_lit0(NULL, "enable", "enable GAMMA process");
    v4l2_gamma_main_arg.dev = arg_str0("d", "device", "<dev>", "use device <dev> instead of /dev/video0, if <dev> starts with a digit, then /dev/video<dev> is used");
    v4l2_gamma_main_arg.gamma = arg_dbl0("g", "gamma", "<gamma>", "GAMMA parameter");
    v4l2_gamma_main_arg.end = arg_end(4);

    const esp_console_cmd_t cmd = {
        .command = "v4l2-gamma",
        .help = "Control V4L2 type video device GAMMA process",
        .hint = NULL,
        .func = &v4l2_gamma_main,
        .argtable = &v4l2_gamma_main_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
