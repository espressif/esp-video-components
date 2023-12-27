/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_video.h"
// #include "private/esp_video_log.h"
#include "sim_picture.h"

#include "mipi_csi.h"
#include "esp_camera.h"

#include "sys_clkrst_struct.h"
#include "hp_clkrst_struct.h"
#include "hp_sys_struct.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "sccb.h"

#include "camera_isp.h"

#define SIM_CAMERA_DEVICE_NAME      CONFIG_SIMULATED_INTF_DEVICE_NAME
#define SIM_CAMERA_BUFFER_COUNT     CONFIG_SIMULATED_INTF_DEVICE_BUFFER_COUNT
#define SIM_CAMERA_COUNT            CONFIG_SIMULATED_INTF_DEVICE_COUNT
#define SIM_CAMERA_BUFFER_SIZE      1843200 + 16// 10 * 1024// (sim_picture_jpeg_len)

struct sim_camera {
    esp_timer_handle_t capture_timer;
    int fps;
};

static const char *TAG = "sim_camera";

extern esp_mipi_csi_handle_t csi_test_handle;

// Need to be optimized valiables
extern esp_pad_t *initial_pad = NULL;
static struct esp_video *g_video;

esp_err_t sim_camera_recv_vb(uint8_t *buffer, uint32_t offset, uint32_t len)
{
#ifdef CONFIG_ESP_VIDEO_MEDIA_CONTROLLER
    struct esp_video_buffer_element *element =
        container_of(buffer, struct esp_video_buffer_element, buffer);
    esp_media_event_t event;
    memset(&event, 0x0, sizeof(event));
    event.cmd = ESP_MEIDA_EVENT_CMD_DATA_RECV;
    event.pad = initial_pad;
    event.param = element;
    element->valid_offset = offset;
    element->valid_size = len;
    esp_media_event_post(&event, 0);
#else
    esp_video_recvdone_buffer(g_video, buffer, len, offset);
#endif

    return ESP_OK;
}

uint8_t *IRAM_ATTR sim_camera_get_new_vb(uint32_t len)
{
    if (len > g_video->buffer_size) {
        return NULL;
    }

    return esp_video_alloc_buffer(g_video);
}

static void esp32p4_system_clk_config(void)
{
    HP_CLKRST.sys_ctrl.sys_clk_div_num = (480000000 / 240000000) - 1;
    HP_CLKRST.peri1_ctrl.peri1_clk_div_num = (480000000 / 120000000) - 1;
    HP_CLKRST.peri2_ctrl.peri2_clk_div_num = (480000000 / 240000000) - 1;

    SYS_CLKRST.gdma_ctrl.gdma_clk_div_num = (480000000 / 240000000) - 1;
    SYS_CLKRST.jpeg_ctrl.jpeg_clk_div_num = (480000000 / 240000000) - 1;

    PERI2_CLKRST.peri2_apb_clk_div_ctrl.peri2_apb_clk_div_num = (240000000 / 120000000) - 1;

    printf("cpu_clk: %d\n", HP_CLKRST.cpu_ctrl.cpu_clk_div_num);
    printf("sys_clk: %d\n", HP_CLKRST.sys_ctrl.sys_clk_div_num);
    printf("peri1_clk: %d\n", HP_CLKRST.peri1_ctrl.peri1_clk_div_num);
    printf("peri2_clk: %d\n", HP_CLKRST.peri2_ctrl.peri2_clk_div_num);

    HP_CLKRST.hp_ctrl.hp_sys_root_clk_sel = 2;

    printf("hp_sys_root_clk_sel: %d\n", HP_CLKRST.hp_ctrl.hp_sys_root_clk_sel);
    printf("hp_cpu_root_clk_sel: %d\n", HP_CLKRST.hp_ctrl.hp_cpu_root_clk_sel);
}

static esp_err_t sim_camera_init(struct esp_video *video)
{
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    camera->capture_timer = NULL;
    camera->fps = 0;

    // esp_timer_create_args_t capture_timer_args = {
    //     .callback = sim_camera_capture_timer_isr,
    //     .dispatch_method = ESP_TIMER_ISR,
    //     .arg = video,
    //     .name = "camera_capture",
    // };

    // ret = esp_timer_create(&capture_timer_args, &camera->capture_timer);
    // if (ret != ESP_OK) {
    //     ESP_VIDEO_LOGE("Failed to create timer ret=%x", ret);
    //     return ret;
    // }
    {
        esp_err_t ret = ESP_OK;
        esp32p4_system_clk_config();
        sccb_bus_init(18, 19);
        // This should auto detect, not call it in manual
        extern esp_camera_device_t sc2336_detect();
        esp_camera_device_t device = sc2336_detect();
        if (device) {
            const char *name = esp_camera_get_name(device);
            if (name) {
                ESP_LOGI(TAG, "device name is: %s", name);
            }
        }
        /**
         * Todo: AEG-1058
         */
        /*Query caps*/
        sensor_capability_t caps = {0};
        esp_camera_get_capability(device, &caps);
        printf("cap = %u\n", caps.fmt_raw);

        /*Query formats and set/get format*/
        sensor_format_array_info_t formats = {0};
        esp_camera_query_format(device, &formats);
        printf("format count = %d\n", formats.count);
        const sensor_format_t *parray = formats.format_array;
        for (int i = 0; i < formats.count; i++) {
            PRINT_CAM_SENSOR_FORMAT_INFO(&(parray[i].index));
        }

        sensor_format_t current_format;
        current_format.index = parray[0].index;
        esp_camera_set_format(device, &current_format);

        ret = esp_camera_get_format(device, &current_format);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Format get fail");
        } else {
            PRINT_CAM_SENSOR_FORMAT_INFO(&current_format);
        }

        int enable_flag = 1;
        /*Start sensor stream*/
        ret = esp_camera_ioctl(device, CAM_SENSOR_IOC_S_STREAM, &enable_flag);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Start stream fail");
        }

        /*Init cam interface*/
        mipi_csi_port_config_t mipi_if_cfg = {
            .frame_height = current_format->height,
            .frame_width = current_format->width,
            .mipi_clk_freq_hz = current_format->mipi_info.mipi_clk,
        };
        if (current_format->format == CAM_SENSOR_PIXFORMAT_RAW10) {
            mipi_if_cfg.in_type = PIXFORMAT_RAW10;
        } else if (current_format->format == CAM_SENSOR_PIXFORMAT_RAW8) {
            mipi_if_cfg.in_type = PIXFORMAT_RAW8;
        }
        if (current_format->isp_info) {
            mipi_if_cfg.isp_enable = true;
            // Todo, copy ISP info to ISP processor
        }
        if (current_format->port == MIPI_CSI_OUTPUT_LANE1) {
            mipi_if_cfg.lane_num = 1;
        } else if (current_format->port == MIPI_CSI_OUTPUT_LANE2) {
            mipi_if_cfg.lane_num = 2;
        }
        mipi_if_cfg.out_type = PIXFORMAT_RGB565;
        ret = esp_mipi_csi_driver_install(MIPI_CSI_PORT0, &mipi_if_cfg, 0, &csi_test_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "csi init fail[%d]", ret);
            return;
        }

        isp_init(mipi_if_cfg.frame_width, mipi_if_cfg.frame_height, mipi_if_cfg.in_type, mipi_if_cfg.out_type, mipi_if_cfg.isp_enable, NULL);
    }

    g_video = video;
    return ESP_OK;
}

static esp_err_t sim_camera_deinit(struct esp_video *video)
{
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    // ret = esp_timer_delete(camera->capture_timer);
    // if (ret != ESP_OK) {
    //     ESP_VIDEO_LOGE("Failed to delete ret=%x", ret);
    //     return ret;
    // }

    camera->capture_timer = NULL;

    return ESP_OK;
}

static esp_err_t sim_camera_start_capture(struct esp_video *video)
{
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    // ret = esp_timer_start_periodic(camera->capture_timer, 1000000 / camera->fps);
    // if (ret != ESP_OK) {
    //     ESP_VIDEO_LOGE("Failed to start timer ret=%x", ret);
    //     return ret;
    // }
    esp_mipi_csi_ops_t ops = {
        .alloc_buffer = sim_camera_get_new_vb,
        .recved_data = sim_camera_recv_vb,
    };
    printf("%s %d line\r\n", __func__, __LINE__);
    esp_mipi_csi_ops_regist(csi_test_handle, &ops);
    esp_mipi_csi_start(csi_test_handle);
    return ESP_OK;
}

static esp_err_t sim_camera_stop_capture(struct esp_video *video)
{
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    // ret = esp_timer_stop(camera->capture_timer);
    // if (ret != ESP_OK) {
    //     ESP_VIDEO_LOGE("Failed to stop timer ret=%x", ret);
    //     return ret;
    // }

    return ESP_OK;
}

static esp_err_t sim_camera_set_format(struct esp_video *video, const struct esp_video_format *format)
{
    esp_err_t ret;
    struct sim_camera *camera = (struct sim_camera *)video->priv;

    camera->fps = (int)format->fps;

    ret = esp_mipi_csi_get_fb_info(csi_test_handle, &video->buffer_size, &video->buffer_align_size, &video->buffer_caps);

    return ret;
}

static esp_err_t sim_camera_capability(struct esp_video *video, struct esp_video_capability *capability)
{
    if (!capability) {
        ESP_LOGE(TAG, "capability=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(capability, 0, sizeof(struct esp_video_capability));
    capability->fmt_jpeg = 1;

    return ESP_OK;
}

static esp_err_t sim_camera_description(struct esp_video *video, char *buffer, uint32_t size)
{
    int ret;

    ret = snprintf(buffer, size, "Simulation Camera:\n\tFormat: RGB565\n\tPixel: 1080P\n");
    if (ret <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static const struct esp_video_ops s_sim_camera_ops = {
    .init          = sim_camera_init,
    .deinit        = sim_camera_deinit,
    .start_capture = sim_camera_start_capture,
    .stop_capture  = sim_camera_stop_capture,
    .set_format    = sim_camera_set_format,
    .capability    = sim_camera_capability,
    .description   = sim_camera_description
};

esp_err_t sim_initialize_camera(void)
{
    char *name;
    struct esp_video *video;
    struct sim_camera *camera;
    int ret = 0;

    for (int i = 0; i < SIM_CAMERA_COUNT; i++) {
        ret = asprintf(&name, "%s%d", SIM_CAMERA_DEVICE_NAME, i);
        assert(ret > 0);

        camera = heap_caps_malloc(sizeof(struct sim_camera), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        assert(camera);

        video = esp_video_create(name, NULL, &s_sim_camera_ops, camera);
        assert(video);
        free(name);
    }

    return ESP_OK;
}
