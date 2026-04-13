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
#include <math.h>
#include "linux/videodev2.h"
#include "esp_video_isp_ioctl.h"
#include "esp_err.h"
#include "esp_log.h"
#include "v4l2_cmd.h"

typedef enum v4l2_bf_action {
    V4L2_BF_DUMP = 0,
    V4L2_BF_ENABLE,
    V4L2_BF_DISABLE,
} v4l2_bf_action_t;

static const char *TAG = "v4l2-bf";

static struct {
    struct arg_lit *all;
    struct arg_lit *disable;
    struct arg_lit *enable;
    struct arg_str *dev;
    struct arg_int *level;
    struct arg_str *matrix;
    struct arg_end *end;
} v4l2_bf_main_arg;

static void bf_action(const char *devname, v4l2_bf_action_t action, uint8_t *level, uint8_t *matrix, uint32_t matrix_mask)
{
    int fd;
    esp_video_isp_bf_t bf;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    ESP_LOGD(TAG, "action=%s", action == V4L2_BF_DUMP ? "dump" :
             action == V4L2_BF_ENABLE ? "enable" :
             action == V4L2_BF_DISABLE ? "disable" : "none");

    fd = open(devname, O_RDWR);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device=%s", devname);
        return;
    }

    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_USER_ESP_ISP_BF;
    control[0].p_u8     = (uint8_t *)&bf;
    control[0].size     = sizeof(bf);
    if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "failed to get BF control");
        goto exit;
    }

    if (action == V4L2_BF_DUMP) {
        printf("\nBF state:  \t%s\n", bf.enable ? "enable" : "disable");
        printf("BF level:  \t%d\n", bf.level);
        printf("BF matrix:\n");
        printf("\t\t      0  1  2\n\n");
        for (int i = 0; i < ISP_BF_TEMPLATE_X_NUMS; i++) {
            printf("\t\t%d  | %2d %2d %2d |\n", i, bf.matrix[i][0], bf.matrix[i][1], bf.matrix[i][2]);
        }
        printf("\n");
    } else if (action == V4L2_BF_DISABLE) {
        bf.enable = false;
        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set BF control");
            goto exit;
        }
    } else if (action == V4L2_BF_ENABLE) {
        bf.enable = true;
        if (level) {
            bf.level = *level;
        }
        if (matrix) {
            for (int i = 0; i < ISP_BF_TEMPLATE_X_NUMS * ISP_BF_TEMPLATE_Y_NUMS; i++) {
                if (matrix_mask & (1 << i)) {
                    ((uint8_t *)bf.matrix)[i] = matrix[i];
                }
            }
        }

        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set BF control");
            goto exit;
        }
    }

    ESP_LOGD(TAG, "success");

exit:
    close(fd);
}

static int decode_matrix(const char *options, uint8_t *matrix, uint32_t *mask)
{
    while (*options) {
        char *endptr;
        unsigned long index = strtoul(options, &endptr, 10);
        if (endptr == options) {
            return -1;
        }
        options = endptr;

        if (index >= ISP_BF_TEMPLATE_X_NUMS * ISP_BF_TEMPLATE_Y_NUMS) {
            return -1;
        }

        if (*options != '=') {
            return -1;
        }

        options++;
        unsigned long val = strtoul(options, &endptr, 10);
        if (endptr == options) {
            return -1;
        }
        options = endptr;

        if (val > UINT8_MAX) {
            return -1;
        }

        if (*options != ',' && *options != '\0') {
            return -1;
        }

        *mask |= 1 << index;
        matrix[index] = val;

        if (*options == ',') {
            options++;
        }
    }

    return 0;
}

static int v4l2_bf_main(int argc, char **argv)
{
    char dev_name[16] = ESP_VIDEO_ISP1_DEVICE_NAME;

    EXAMPLE_V4L2_CMD_CHECK(v4l2_bf_main_arg);

    if (v4l2_bf_main_arg.dev->count) {
        if (decode_dev_name(v4l2_bf_main_arg.dev->sval[0], dev_name, sizeof(dev_name))) {
            ESP_LOGE(TAG, "decode device name=%s error", v4l2_bf_main_arg.dev->sval[0]);
            return -1;
        }
    }

    if (v4l2_bf_main_arg.all->count) {
        bf_action(dev_name, V4L2_BF_DUMP, NULL, NULL, 0);
    } else if (v4l2_bf_main_arg.disable->count) {
        bf_action(dev_name, V4L2_BF_DISABLE, NULL, NULL, 0);
    } else if (v4l2_bf_main_arg.enable->count) {
        uint8_t level;
        uint8_t *level_ptr = NULL;
        uint32_t matrix_mask = 0;
        uint8_t matrix[ISP_BF_TEMPLATE_X_NUMS * ISP_BF_TEMPLATE_Y_NUMS];
        uint8_t *matrix_ptr = NULL;

        if (v4l2_bf_main_arg.level->count) {
            int level_val = v4l2_bf_main_arg.level->ival[0];
            if (level_val < 0 || level_val > UINT8_MAX) {
                ESP_LOGE(TAG, "level must be in range [0, %u], got %d", (unsigned)UINT8_MAX, level_val);
                return -1;
            }
            level = (uint8_t)level_val;
            level_ptr = &level;
        }

        if (v4l2_bf_main_arg.matrix->count) {
            if (decode_matrix(v4l2_bf_main_arg.matrix->sval[0], matrix, &matrix_mask)) {
                ESP_LOGE(TAG, "decode matrix error");
                return -1;
            }
            matrix_ptr = matrix;
        }

        bf_action(dev_name, V4L2_BF_ENABLE, level_ptr, matrix_ptr, matrix_mask);
    }

    return 0;
}

void v4l2_cmd_bf_register(void)
{
    v4l2_bf_main_arg.all = arg_lit0(NULL, "all", "display all information available");
    v4l2_bf_main_arg.disable = arg_lit0(NULL, "disable", "disable BF process");
    v4l2_bf_main_arg.enable = arg_lit0(NULL, "enable", "enable BF process");
    v4l2_bf_main_arg.dev = arg_str0("d", "device", "<dev>", "use device <dev> instead of /dev/video0, if <dev> starts with a digit, then /dev/video<dev> is used");
    v4l2_bf_main_arg.level = arg_int0("l", "level", "<level>", "Bayer filter denoising level");
    v4l2_bf_main_arg.matrix = arg_str0("m", "matrix", "<offset>=<value><,<offset>=<value>...>", "Bayer filter denoising matrix");
    v4l2_bf_main_arg.end = arg_end(6);

    const esp_console_cmd_t cmd = {
        .command = "v4l2-bf",
        .help = "Control V4L2 type video device bayer filter(BF) process",
        .hint = NULL,
        .func = &v4l2_bf_main,
        .argtable = &v4l2_bf_main_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
