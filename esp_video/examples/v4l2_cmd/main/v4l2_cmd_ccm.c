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

typedef enum v4l2_ccm_action {
    V4L2_CCM_DUMP = 0,
    V4L2_CCM_ENABLE,
    V4L2_CCM_DISABLE,
} v4l2_ccm_action_t;

static const char *TAG = "v4l2-ccm";

static struct {
    struct arg_lit *all;
    struct arg_lit *disable;
    struct arg_lit *enable;
    struct arg_str *dev;
    struct arg_str *matrix;
    struct arg_end *end;
} v4l2_ccm_main_arg;

static void ccm_action(const char *devname, v4l2_ccm_action_t action, float *matrix, uint32_t matrix_mask)
{
    int fd;
    esp_video_isp_ccm_t ccm;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    ESP_LOGD(TAG, "action=%s", action == V4L2_CCM_DUMP ? "dump" :
             action == V4L2_CCM_ENABLE ? "enable" :
             action == V4L2_CCM_DISABLE ? "disable" : "none");

    fd = open(devname, O_RDWR);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device=%s", devname);
        return;
    }

    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_USER_ESP_ISP_CCM;
    control[0].p_u8     = (uint8_t *)&ccm;
    control[0].size     = sizeof(ccm);
    if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "failed to get CCM control");
        goto exit;
    }

    if (action == V4L2_CCM_DUMP) {
        printf("\nCCM state:  \t%s\n", ccm.enable ? "enable" : "disable");
        printf("CCM matrix:\n");
        printf("\t\t        R    G    B\n\n");
        for (int i = 0; i < ISP_CCM_DIMENSION; i++) {
            float *m = ccm.matrix[i];

            printf("\t\t%d  | %0.4f  %0.4f  %0.4f |\n", i, m[0], m[1], m[2]);
        }
        printf("\n");
    } else if (action == V4L2_CCM_DISABLE) {
        ccm.enable = false;
        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set CCM control");
            goto exit;
        }
    } else if (action == V4L2_CCM_ENABLE) {
        ccm.enable = true;
        if (matrix) {
            float *m = ccm.matrix[0];

            for (int i = 0; i < ISP_CCM_DIMENSION * ISP_CCM_DIMENSION; i++) {
                if (matrix_mask & (1 << i)) {
                    m[i]  = matrix[i];
                }
            }
        }

        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set CCM control");
            goto exit;
        }
    }

    ESP_LOGD(TAG, "success");

exit:
    close(fd);
}

static int decode_matrix(const char *options, float *matrix, uint32_t *mask)
{
    while (*options) {
        int index = 0;
        float val = 0.0;
        bool neg = false;
        bool point = false;
        float exp = 10;

        bool has_digit = false;
        while (isdigit((int)*options)) {
            has_digit = true;
            index = index * 10 + *options++ - '0';
        }

        if (!has_digit) {
            return -1;  /* missing index, e.g. "=1.0" */
        }
        if (index >= ISP_CCM_DIMENSION * ISP_CCM_DIMENSION) {
            return -1;
        }

        if (*options != '=') {
            return -1;
        }
        options++;

        if (*options == '-') {
            neg = true;
            options++;
        }

        bool has_value_digit = false;
        while (isdigit((int)*options) || *options == '.') {
            if (*options == '.') {
                if (point) {
                    return -1;
                }
                point = true;
            } else {
                has_value_digit = true;
                if (!point) {
                    val = val * 10 + *options - '0';
                } else {
                    val = val + (float)(*options - '0') / exp;
                    exp *= 10;
                }
            }
            options++;
        }

        if (!has_value_digit) {
            return -1;  /* missing value, e.g. "0=" or "0=." */
        }

        if (neg) {
            val = -val;
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

static int v4l2_ccm_main(int argc, char **argv)
{
    int ret = 0;
    char dev_name[64] = ESP_VIDEO_ISP1_DEVICE_NAME;

    EXAMPLE_V4L2_CMD_CHECK(v4l2_ccm_main_arg);

    if (v4l2_ccm_main_arg.dev->count) {
        if (decode_dev_name(v4l2_ccm_main_arg.dev->sval[0], dev_name, sizeof(dev_name))) {
            ESP_LOGE(TAG, "decode device name=%s error", v4l2_ccm_main_arg.dev->sval[0]);
            return -1;
        }
    }

    if (v4l2_ccm_main_arg.all->count) {
        ccm_action(dev_name, V4L2_CCM_DUMP, NULL, 0);
    } else if (v4l2_ccm_main_arg.disable->count) {
        ccm_action(dev_name, V4L2_CCM_DISABLE, NULL, 0);
    } else if (v4l2_ccm_main_arg.enable->count) {
        uint32_t matrix_mask = 0;
        float matrix[ISP_CCM_DIMENSION * ISP_CCM_DIMENSION];
        float *matrix_ptr = NULL;

        if (v4l2_ccm_main_arg.matrix->count) {
            if (decode_matrix(v4l2_ccm_main_arg.matrix->sval[0], matrix, &matrix_mask)) {
                ESP_LOGE(TAG, "decode matrix error");
                ret = -1;
                goto exit;
            }
            matrix_ptr = matrix;
        }

        ccm_action(dev_name, V4L2_CCM_ENABLE, matrix_ptr, matrix_mask);
    }

exit:
    return ret;
}

void v4l2_cmd_ccm_register(void)
{
    v4l2_ccm_main_arg.all = arg_lit0(NULL, "all", "display all information available");
    v4l2_ccm_main_arg.disable = arg_lit0(NULL, "disable", "disable CCM process");
    v4l2_ccm_main_arg.enable = arg_lit0(NULL, "enable", "enable CCM process");
    v4l2_ccm_main_arg.dev = arg_str0("d", "device", "<dev>", "use device <dev> instead of /dev/video0, if <dev> starts with a digit, then /dev/video<dev> is used");
    v4l2_ccm_main_arg.matrix = arg_str0("m", "matrix", "<offset>=<value><,<offset>=<value>...>", "color correct matrix");
    v4l2_ccm_main_arg.end = arg_end(5);

    const esp_console_cmd_t cmd = {
        .command = "v4l2-ccm",
        .help = "Control V4L2 type video color correct matrix(CCM) process",
        .hint = NULL,
        .func = &v4l2_ccm_main,
        .argtable = &v4l2_ccm_main_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
