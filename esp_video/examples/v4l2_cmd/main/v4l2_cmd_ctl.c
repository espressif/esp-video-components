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
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include "linux/videodev2.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "v4l2_cmd.h"
#include "esp_video_ioctl.h"

#define DEFAULT_SKIP_COUNT 3
#define DEFAULT_MMAP_COUNT 3
#define DEFAULT_STREAM_COUNT 1
#define MAX_MMAP_COUNT 32

#if CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
#define BASE_PATH CONFIG_EXAMPLE_SPI_FLASH_MOUNT_POINT
#else
#define BASE_PATH CONFIG_EXAMPLE_SDMMC_MOUNT_POINT
#endif

static const char *TAG = "v4l2-ctl";

static struct {
    struct arg_lit *all;
    struct arg_str *dev;
    struct arg_str *set_ctrl;
    struct arg_lit *list_formats;
    struct arg_lit *list_devices;
    struct arg_str *set_fmt_video;
    struct arg_str *set_fmt_video_out;
    struct arg_str *stream_from;
    struct arg_str *stream_to;
    struct arg_int *stream_mmap;
    struct arg_int *stream_skip;
    struct arg_int *stream_count;
    struct arg_end *end;
} v4l2_ctl_main_arg;

int parse_video_parameters(const char *param_str, struct v4l2_format *format)
{
    char *str_copy = strdup(param_str);
    if (str_copy == NULL) {
        return -1;
    }

    char *saveptr = NULL;
    const char *pair_delimiters = ",";

    char *token = strtok_r(str_copy, pair_delimiters, &saveptr);

    while (token != NULL) {
        char *key = token;
        while (*key == ' ') {
            key++;
        }

        char *value = strchr(key, '=');
        if (value != NULL) {
            *value = '\0';
            value++;

            while (*value == ' ') {
                value++;
            }
            if (strcmp(key, "width") == 0) {
                int w = atoi(value);
                if (w <= 0) {
                    free(str_copy);
                    return -1;
                }
                format->fmt.pix.width = (uint32_t)w;
            } else if (strcmp(key, "height") == 0) {
                int h = atoi(value);
                if (h <= 0) {
                    free(str_copy);
                    return -1;
                }
                format->fmt.pix.height = (uint32_t)h;
            } else if (strcmp(key, "pixelformat") == 0) {
                if (strlen(value) != 4) {
                    free(str_copy);
                    return -1;
                }
                memcpy(&format->fmt.pix.pixelformat, value, 4);
            }
        }

        token = strtok_r(NULL, pair_delimiters, &saveptr);
    }

    free(str_copy);

    return 0;
}

static void dump_dev_info(const char *name)
{
    int fd;
    int type = 0;
    struct v4l2_format format;
    struct v4l2_capability capability;
    struct v4l2_query_ext_ctrl qctrl = {0};

    fd = open(name, O_RDWR);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device=%s", name);
        return;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        goto exit;
    }

    printf("Driver Info:\n");
    printf("\tDriver name      : %s\n", capability.driver);
    printf("\tCard type        : %s\n", capability.card);
    printf("\tBus info         : %s\n", capability.bus_info);
    printf("\tDriver version   : %d.%d.%d\n", (uint16_t)(capability.version >> 16),
           (uint8_t)(capability.version >> 8),
           (uint8_t)capability.version);
    printf("\tCapabilities     : 0x%" PRIx32 "\n", capability.capabilities);
    if (capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        printf("\t\tVideo Capture\n");
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    if (capability.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
        printf("\t\tVideo Output\n");
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    }
    if (capability.capabilities & V4L2_CAP_VIDEO_M2M) {
        printf("\t\tVideo M2M\n");
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    if (capability.capabilities & V4L2_CAP_READWRITE) {
        printf("\t\tRead/Write\n");
    }
    if (capability.capabilities & V4L2_CAP_STREAMING) {
        printf("\t\tStreaming\n");
    }
    if (capability.capabilities & V4L2_CAP_META_CAPTURE) {
        printf("\t\tMetadata Capture\n");
        type = V4L2_BUF_TYPE_META_CAPTURE;
    }
    if (capability.capabilities & V4L2_CAP_EXT_PIX_FORMAT) {
        printf("\t\tExtended Pix Format\n");
    }

    if (capability.capabilities & V4L2_CAP_DEVICE_CAPS) {
        printf("\t\tDevice Capabilities\n");

        printf("\tDevice Caps      : 0x%" PRIx32 "\n", capability.device_caps);
        if (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
            printf("\t\tVideo Capture\n");
        }
        if (capability.device_caps & V4L2_CAP_VIDEO_OUTPUT) {
            printf("\t\tVideo Output\n");
        }
        if (capability.device_caps & V4L2_CAP_VIDEO_M2M) {
            printf("\t\tVideo M2M\n");
        }
        if (capability.device_caps & V4L2_CAP_READWRITE) {
            printf("\t\tRead/Write\n");
        }
        if (capability.device_caps & V4L2_CAP_STREAMING) {
            printf("\t\tStreaming\n");
        }
        if (capability.device_caps & V4L2_CAP_EXT_PIX_FORMAT) {
            printf("\t\tExtended Pix Format\n");
        }
        if (capability.device_caps & V4L2_CAP_TIMEPERFRAME) {
            printf("\t\tTIMEPERFRAME\n");
        }
    }

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == 0) {
        printf("Format Video Capture:\n");
        printf("\tWidth/Height     : %" PRIu32 "/%" PRIu32 "\n", format.fmt.pix.width, format.fmt.pix.height);
        printf("\tPixel Format     : " V4L2_FMT_STR "\n", V4L2_FMT_STR_ARG(format.fmt.pix.pixelformat));
    }

    printf("\nControls\n\n");
    while (1) {
        char *type;

        qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        if (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl) < 0) {
            goto exit;
        }

        type = qctrl.type == V4L2_CTRL_TYPE_INTEGER ? "int" :
               qctrl.type == V4L2_CTRL_TYPE_MENU ? "menu" :
               qctrl.type == V4L2_CTRL_TYPE_INTEGER_MENU ? "intmenu" :
               qctrl.type == V4L2_CTRL_TYPE_BITMASK ? "bitmask" :
               qctrl.type == V4L2_CTRL_TYPE_U8 ? "u8" :
               qctrl.type == V4L2_CTRL_TYPE_U16 ? "u16" :
               qctrl.type == V4L2_CTRL_TYPE_U32 ? "u32" : "unknown";

        printf("%31s 0x%08" PRIx32 " (%s)   \t: min=%lld max=%lld step=%llu default=%lld\n", qctrl.name,
               qctrl.id, type, qctrl.minimum, qctrl.maximum, qctrl.step, qctrl.default_value);

    }

exit:
    close(fd);
}

static int decode_ctrl_arg(const char *options, const char **key, const char **val)
{
    const char *p = options;

    *key = p;

    while (*p != '\0' && *p++ != '=') { }

    if (*p == '\0') {
        ESP_LOGE(TAG, "controller value is not set, options=%s is invalid", options);
        return -1;
    }

    *val = p;

    while (*p != '\0' && isdigit((int)*p)) {
        p++;
    }

    if (*val == p || (*p != '\0' && *p != ',')) {
        ESP_LOGE(TAG, "controller value only supports integer, options=%s is invalid", options);
        return -1;
    }

    if (*p == ',') {
        p++;
    }

    return p - options;
}

static int compare_ctrl_key(const char *key, const char *target)
{
    while (*key == *target) {
        key++;
        target++;
    }

    if (*key == '=' && *target == '\0') {
        return 0;
    }

    return -1;
}

static void set_dev_ctrl(const char *name, const char *options)
{
    int fd;
    bool set_done = false;
    const char *s = options;

    fd = open(name, O_RDWR);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device=%s", name);
        return;
    }

    while (*s != '\0') {
        const char *key;
        const char *val;
        int ret = decode_ctrl_arg(s, &key, &val);
        if (ret < 0) {
            ESP_LOGE(TAG, "failed to set %s", s);
            goto exit;
        }

        s += ret;
        if (*s == ',') {
            s++;
        }

        struct v4l2_query_ext_ctrl qctrl = {0};

        qctrl.id = 0;
        while (1) {
            qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            if (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl) < 0) {
                break;
            }

            if (!compare_ctrl_key(key, qctrl.name)) {
                struct v4l2_ext_controls controls;
                struct v4l2_ext_control control[1];

                controls.ctrl_class = V4L2_CTRL_CLASS_USER;
                controls.count      = 1;
                controls.controls   = control;
                control[0].id       = qctrl.id;
                control[0].value    = atoi(val);
                control[0].size     = sizeof(int);
                if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
                    ESP_LOGE(TAG, "failed to set controller %s", key);
                    goto exit;
                }

                set_done = true;
                break;
            }
        }
    }

exit:
    close(fd);
    if (!set_done) {
        ESP_LOGE(TAG, "failed to set ctrl %s", options);
    }
}

static void list_dev_formats(const char *name)
{
    int fd;
    struct v4l2_fmtdesc fmtdesc = {0};
    struct v4l2_capability capability;

    fd = open(name, O_RDWR);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device=%s", name);
        return;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        goto exit;
    }

    printf("ioctl: VIDIOC_ENUM_FMT\n");

    if (capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        printf("\tType : Video Capture\n\n");
        while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
            printf("\t[%" PRIu32 "]: " V4L2_FMT_STR "\n", fmtdesc.index, V4L2_FMT_STR_ARG(fmtdesc.pixelformat));
            fmtdesc.index++;
        }
    } else if (capability.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        printf("\tType : Video Output\n\n");
        while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
            printf("\t[%" PRIu32 "]: " V4L2_FMT_STR "\n", fmtdesc.index, V4L2_FMT_STR_ARG(fmtdesc.pixelformat));
            fmtdesc.index++;
        }
    } else if (capability.capabilities & V4L2_CAP_VIDEO_M2M) {
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        printf("\tType : Video Capture\n\n");
        while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
            printf("\t[%" PRIu32 "]: " V4L2_FMT_STR "\n", fmtdesc.index, V4L2_FMT_STR_ARG(fmtdesc.pixelformat));
            fmtdesc.index++;
        }

        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        fmtdesc.index = 0;
        printf("\tType : Video Output\n\n");
        while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
            printf("\t[%" PRIu32 "]: " V4L2_FMT_STR "\n", fmtdesc.index, V4L2_FMT_STR_ARG(fmtdesc.pixelformat));
            fmtdesc.index++;
        }
    } else {
        ESP_LOGE(TAG, "device=%s is not a video capture device", name);
    }

exit:
    close(fd);
}

static void list_devices(void)
{
    char *dev_names[] = {
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
        ESP_VIDEO_MIPI_CSI_DEVICE_NAME,
#endif
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
        ESP_VIDEO_DVP_DEVICE_NAME,
#endif
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
        ESP_VIDEO_SPI_DEVICE_0_NAME,
#endif
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_1_SENSOR
        ESP_VIDEO_SPI_DEVICE_1_NAME,
#endif
#if CONFIG_ESP_VIDEO_ENABLE_JPEG_VIDEO_DEVICE
        ESP_VIDEO_JPEG_DEVICE_NAME,
#endif
#if CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
        ESP_VIDEO_H264_DEVICE_NAME,
#endif
    };

    for (int i = 0; i < sizeof(dev_names) / sizeof(dev_names[0]); i++) {
        int fd = open(dev_names[i], O_RDWR);
        if (fd < 0) {
            ESP_LOGD(TAG, "failed to open device=%s", dev_names[i]);
            continue;
        }

        struct v4l2_capability capability;
        if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
            ESP_LOGD(TAG, "failed to get capability");
            close(fd);
            continue;
        }

        printf("%s (%s):\n", capability.card, capability.bus_info);
        printf("\t%s\n\n", dev_names[i]);

        close(fd);
    }

    for (int i = 0; i < (ESP_VIDEO_USB_UVC_DEVICE_ID_MAX - ESP_VIDEO_USB_UVC_DEVICE_ID_MIN + 1); i++) {
        int fd = open(ESP_VIDEO_USB_UVC_DEVICE_NAME(i), O_RDWR);
        if (fd < 0) {
            ESP_LOGD(TAG, "failed to open device=%s", ESP_VIDEO_USB_UVC_DEVICE_NAME(i));
            continue;
        }

        struct v4l2_capability capability;
        if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
            ESP_LOGD(TAG, "failed to get capability");
            close(fd);
            continue;
        }

        printf("%s (%s):\n", capability.card, capability.bus_info);
        printf("\t%s\n\n", ESP_VIDEO_USB_UVC_DEVICE_NAME(i));

        close(fd);
    }
}

static void stream_to_capture(const char *name, const char *file)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_requestbuffers req;
    int skip_count = DEFAULT_SKIP_COUNT;
    int mmap_count = DEFAULT_MMAP_COUNT;
    int stream_count = DEFAULT_STREAM_COUNT;
    char *file_path;

#if EXAMPLE_TINYUSB_MSC_STORAGE
    bool is_in_use = false;

    if (example_msc_storage_in_use_by_usb_host(example_storage_get_handle(), &is_in_use) != ESP_OK || is_in_use) {
        ESP_LOGE(TAG, "MSC is in use by USB host, exit...");
        return;
    }
#endif

    if (v4l2_ctl_main_arg.stream_skip->count) {
        skip_count = v4l2_ctl_main_arg.stream_skip->ival[0];
    }
    if (v4l2_ctl_main_arg.stream_mmap->count) {
        mmap_count = v4l2_ctl_main_arg.stream_mmap->ival[0];
        if (mmap_count <= 0 || mmap_count > MAX_MMAP_COUNT) {
            ESP_LOGE(TAG, "mmap_count must be in range [1, %d]", MAX_MMAP_COUNT);
            return;
        }
    }
    if (v4l2_ctl_main_arg.stream_count->count) {
        stream_count = v4l2_ctl_main_arg.stream_count->ival[0];
    }

    ESP_LOGI(TAG, "skip_count=%d, mmap_count=%d, stream_count=%d", skip_count, mmap_count, stream_count);

    uint8_t *buffer[MAX_MMAP_COUNT];

    int fd = open(name, O_RDWR);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device=%s", name);
        return;
    }

    if (v4l2_ctl_main_arg.set_fmt_video->count) {
        struct v4l2_format format = {0};

        format.type = type;
        if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
            ESP_LOGE(TAG, "failed to get format");
            goto fail0;
        }

        if (parse_video_parameters(v4l2_ctl_main_arg.set_fmt_video->sval[0], &format)) {
            ESP_LOGE(TAG, "failed to parse video parameters");
            goto fail0;
        }

        format.type = type;
        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            ESP_LOGE(TAG, "failed to set format");
            goto fail0;
        }
    }

    memset(&req, 0, sizeof(req));
    req.count  = mmap_count;
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "failed to require buffer");
        goto fail0;
    }

    for (int i = 0; i < mmap_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type        = type;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to query buffer");
            goto fail0;
        }

        buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, fd, buf.m.offset);
        if (buffer[i] == MAP_FAILED) {
            ESP_LOGE(TAG, "failed to map buffer");
            goto fail0;
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            goto fail0;
        }
    }

    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "failed to start stream");
        goto fail0;
    }

    for (int i = 0; i < skip_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to dequeue video frame");
            goto fail1;
        }
        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            goto fail1;
        }
    }

    if (asprintf(&file_path, "%s/%s", BASE_PATH, file) < 0) {
        ESP_LOGE(TAG, "failed to asprintf file_path");
        goto fail1;
    }

    int fd_file = open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    free(file_path);
    if (fd_file < 0) {
        ESP_LOGE(TAG, "failed to open file=%s", file);
        goto fail1;
    }

    for (int i = 0; i < stream_count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to dequeue video frame");
            goto fail2;
        }

        int ret = write(fd_file, buffer[buf.index], buf.bytesused);
        if (ret < 0) {
            ESP_LOGE(TAG, "failed to write to file=%s", file);
            goto fail2;
        } else {
            ESP_LOGI(TAG, "wrote %d bytes to file=%s", ret, file);
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
            goto fail2;
        }
    }

fail2:
    close(fd_file);
fail1:
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "failed to stop stream");
    }
fail0:
    close(fd);
}

static void stream_to_m2m(const char *name, const char *in_file, const char *out_file)
{
    int type;
    uint8_t *out_buffer;
    uint8_t *cap_buffer;
    struct v4l2_buffer out_buf;
    struct v4l2_buffer cap_buf;
    struct v4l2_format out_format;
    struct v4l2_format cap_format;
    struct v4l2_requestbuffers req;
    char *file_in_path;
    char *file_out_path;

#if EXAMPLE_TINYUSB_MSC_STORAGE
    bool is_in_use = false;
    if (example_msc_storage_in_use_by_usb_host(example_storage_get_handle(), &is_in_use) != ESP_OK || is_in_use) {
        ESP_LOGE(TAG, "MSC is in use by USB host, exit...");
        return;
    }
#endif

    if (!v4l2_ctl_main_arg.set_fmt_video_out->count) {
        ESP_LOGE(TAG, "set-fmt-video-out is not set, exit...");
        return;
    }

    int fd = open(name, O_RDWR);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device=%s", name);
        return;
    }

    if (asprintf(&file_in_path, "%s/%s", BASE_PATH, in_file) < 0) {
        ESP_LOGE(TAG, "failed to asprintf file_in_path");
        goto fail0;
    }

    int in_fd = open(file_in_path, O_RDONLY);
    free(file_in_path);
    if (in_fd < 0) {
        ESP_LOGE(TAG, "failed to open file=%s", in_file);
        goto fail0;
    }

    struct stat in_stat;
    if (fstat(in_fd, &in_stat) != 0) {
        ESP_LOGE(TAG, "failed to fstat file=%s", in_file);
        goto fail1;
    }

    out_buffer = heap_caps_malloc(in_stat.st_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED);
    if (out_buffer == NULL) {
        ESP_LOGE(TAG, "failed to malloc out_buffer");
        goto fail1;
    }

    if (read(in_fd, out_buffer, in_stat.st_size) != in_stat.st_size) {
        ESP_LOGE(TAG, "failed to read file=%s", in_file);
        goto fail2;
    }

    if (asprintf(&file_out_path, "%s/%s", BASE_PATH, out_file) < 0) {
        ESP_LOGE(TAG, "failed to asprintf file_path");
        goto fail2;
    }

    int out_fd = open(file_out_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    free(file_out_path);
    if (out_fd < 0) {
        ESP_LOGE(TAG, "failed to open file=%s", out_file);
        goto fail2;
    }

    /* Setup output format and buffer */

    memset(&out_format, 0, sizeof(out_format));
    out_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(fd, VIDIOC_G_FMT, &out_format) != 0) {
        ESP_LOGE(TAG, "failed to get out_format");
        goto fail3;
    }

    if (parse_video_parameters(v4l2_ctl_main_arg.set_fmt_video_out->sval[0], &out_format)) {
        ESP_LOGE(TAG, "failed to parse video parameters");
        goto fail3;
    }

    out_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(fd, VIDIOC_S_FMT, &out_format) != 0) {
        ESP_LOGE(TAG, "failed to set out_format");
        goto fail3;
    }

    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "failed to require out_buf");
        goto fail3;
    }

    memset(&out_buf, 0, sizeof(out_buf));
    out_buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    out_buf.memory      = V4L2_MEMORY_USERPTR;
    out_buf.index       = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &out_buf) != 0) {
        ESP_LOGE(TAG, "failed to query out_buf");
        goto fail3;
    }

    out_buf.m.userptr = (uintptr_t)out_buffer;
    if (ioctl(fd, VIDIOC_QBUF, &out_buf) != 0) {
        ESP_LOGE(TAG, "failed to out queue video frame");
        goto fail3;
    }

    /* Setup capture format and buffer */

    memset(&cap_format, 0, sizeof(cap_format));
    cap_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &cap_format) != 0) {
        ESP_LOGE(TAG, "failed to get cap_format");
        goto fail3;
    }

    /* Use output image size as capture image dimension */
    cap_format.fmt.pix.width = out_format.fmt.pix.width;
    cap_format.fmt.pix.height = out_format.fmt.pix.height;
    if (v4l2_ctl_main_arg.set_fmt_video->count) {
        if (parse_video_parameters(v4l2_ctl_main_arg.set_fmt_video->sval[0], &cap_format)) {
            ESP_LOGE(TAG, "failed to parse video parameters");
            goto fail3;
        }
    }

    cap_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_S_FMT, &cap_format) != 0) {
        ESP_LOGE(TAG, "failed to set cap_format");
        goto fail3;
    }

    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "failed to require buffer");
        goto fail3;
    }

    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory      = V4L2_MEMORY_MMAP;
    cap_buf.index       = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to query cap_buf");
        goto fail3;
    }

    cap_buffer = (uint8_t *)mmap(NULL, cap_buf.length, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, fd, cap_buf.m.offset);
    if (cap_buffer == MAP_FAILED) {
        ESP_LOGE(TAG, "failed to map cap_buffer");
        goto fail3;
    }

    if (ioctl(fd, VIDIOC_QBUF, &cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to cap queue video frame");
        goto fail3;
    }

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "failed to start output stream");
        goto fail3;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "failed to start capture stream");
        goto fail4;
    }

    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &cap_buf) != 0) {
        ESP_LOGE(TAG, "failed to dequeue capture video frame");
        goto fail5;
    }

    memset(&out_buf, 0, sizeof(out_buf));
    out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    out_buf.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(fd, VIDIOC_DQBUF, &out_buf) != 0) {
        ESP_LOGE(TAG, "failed to dequeue output video frame");
        goto fail5;
    }

    int ret = write(out_fd, cap_buffer, cap_buf.bytesused);
    if (ret < 0) {
        ESP_LOGE(TAG, "failed to write to file=%s", out_file);
    } else {
        ESP_LOGI(TAG, "wrote %d bytes to file=%s", ret, out_file);
    }

fail5:
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "failed to stop capture stream");
    }
fail4:
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        ESP_LOGE(TAG, "failed to stop output stream");
    }
fail3:
    close(out_fd);
fail2:
    heap_caps_free(out_buffer);
fail1:
    close(in_fd);
fail0:
    close(fd);
}

static int v4l2_ctl_main(int argc, char **argv)
{
    char dev_name[16] = V4L2_CTL_DEFAULT_DEV;

    EXAMPLE_V4L2_CMD_CHECK(v4l2_ctl_main_arg);

    if (v4l2_ctl_main_arg.dev->count) {
        if (decode_dev_name(v4l2_ctl_main_arg.dev->sval[0], dev_name, sizeof(dev_name))) {
            ESP_LOGE(TAG, "decode device name=%s error", v4l2_ctl_main_arg.dev->sval[0]);
            return -1;
        }
    }

    if (v4l2_ctl_main_arg.all->count) {
        dump_dev_info(dev_name);
    } else if (v4l2_ctl_main_arg.set_ctrl->count) {
        set_dev_ctrl(dev_name, v4l2_ctl_main_arg.set_ctrl->sval[0]);
    } else if (v4l2_ctl_main_arg.list_formats->count) {
        list_dev_formats(dev_name);
    } else if (v4l2_ctl_main_arg.list_devices->count) {
        list_devices();
    } else if (v4l2_ctl_main_arg.stream_from->count) {
        if (v4l2_ctl_main_arg.stream_to->count) {
            stream_to_m2m(dev_name, v4l2_ctl_main_arg.stream_from->sval[0], v4l2_ctl_main_arg.stream_to->sval[0]);
        } else {
            ESP_LOGE(TAG, "stream-to is not set, exit...");
            return -1;
        }
    } else if (v4l2_ctl_main_arg.stream_to->count) {
        stream_to_capture(dev_name, v4l2_ctl_main_arg.stream_to->sval[0]);
    }

    return 0;
}

void v4l2_cmd_ctl_register(void)
{
    v4l2_ctl_main_arg.all = arg_lit0(NULL, "all", "display all information available");
    v4l2_ctl_main_arg.set_ctrl = arg_str0("c", "set-ctrl", "<ctrl>=<val>[,<ctrl>=<val>...]", "set the value of the controls");
    v4l2_ctl_main_arg.list_formats = arg_lit0(NULL, "list-formats", "display supported video formats");
    v4l2_ctl_main_arg.list_devices = arg_lit0(NULL, "list-devices", "list all v4l2 video devices");
    v4l2_ctl_main_arg.set_fmt_video = arg_str0(NULL, "set-fmt-video", "width=<w>,height=<h>,pixelformat=<pf>", "set the video capture format, pixelformat is either the format index as reported by --list-formats, or the fourcc value as a string");
    v4l2_ctl_main_arg.set_fmt_video_out = arg_str0(NULL, "set-fmt-video-out", "width=<w>,height=<h>,pixelformat=<pf>", "set the video output format, pixelformat is either the format index as reported by --list-formats, or the fourcc value as a string");
    v4l2_ctl_main_arg.stream_mmap = arg_int0(NULL, "stream-mmap", "<count>", "capture video using mmap(), count: the number of buffers to allocate");
    v4l2_ctl_main_arg.stream_skip = arg_int0(NULL, "stream-skip", "<count>", "skip the first <count> buffers");
    v4l2_ctl_main_arg.stream_to = arg_str0(NULL, "stream-to", "<file>", "write captured frames to <file>");
    v4l2_ctl_main_arg.stream_from = arg_str0(NULL, "stream-from", "<file>", "stream from this <file>");
    v4l2_ctl_main_arg.stream_count = arg_int0(NULL, "stream-count", "<count>", "capture <count> frames");
    v4l2_ctl_main_arg.dev = arg_str0("d", "device", "<dev>", "use device <dev> instead of /dev/video0, if <dev> starts with a digit, then /dev/video<dev> is used");
    v4l2_ctl_main_arg.end = arg_end(10);

    const esp_console_cmd_t cmd = {
        .command = "v4l2-ctl",
        .help = "Control V4L2 type video device",
        .hint = NULL,
        .func = &v4l2_ctl_main,
        .argtable = &v4l2_ctl_main_arg
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
