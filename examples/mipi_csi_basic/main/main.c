/* MIPI Camera Dump Data Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "sys_clkrst_struct.h"
#include "hp_clkrst_struct.h"
#include "peri2_clkrst_struct.h"
#include "hp_sys_struct.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "sccb.h"
#include "esp_camera.h"

#include "camera_isp.h"

#include "mipi_csi.h"

/* For sccb, it needs to be uniformly implemented for the use of sccb,
including the implementation of this interface by I2C and I3C.
Note that if use i3c as sccb bus, the sccb pin num is fixed.*/
#define SCCB_SCL_PIN_NUM    (18)
#define SCCB_SDA_PIN_NUM    (19)

static const char *TAG = "cam_dump";
static int IRAM_ATTR s_buffer_count;

static void delay_us(uint32_t t)
{
    ets_delay_us(t);
}

static void disp_buf(uint8_t *buf, uint16_t len)
{
    int i;
    for (i = 0; i < len; i++) {
        esp_rom_printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) {
            esp_rom_printf("\n");
        }
    }
    esp_rom_printf("\n");
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

#define EXAMPLE_SCCB_PORT_DEFAULT (0)

/*Note, used in ISR, unsafe functions cannot be used.*/
static esp_err_t IRAM_ATTR camera_recvdone_vb(uint8_t *buffer, uint32_t offset, uint32_t len)
{
    esp_rom_printf("buf=%p, offset=%u, l=%d\n", buffer, offset, len);
    disp_buf(buffer, 32);
    disp_buf(buffer + len - 32, 32);
    free(buffer);
    return ESP_OK;
}

/*Note, used in ISR or Task context, unsafe functions cannot be used.*/
static uint8_t *IRAM_ATTR camera_get_new_vb(uint32_t len)
{
    uint32_t _caps = MALLOC_CAP_8BIT;
    _caps |= MALLOC_CAP_SPIRAM;
    if (s_buffer_count > 3) {
        return NULL;
    }
    s_buffer_count++;
    return heap_caps_malloc(len + 16, _caps);
}

void app_main(void)
{
    esp_err_t ret = ESP_OK;
    esp32p4_system_clk_config();
    sccb_bus_init(18, 19);
    // This should auto detect, not call it in manual
    extern esp_camera_device_t sc2336_detect();
    esp_camera_device_t device = sc2336_detect();
    if (device) {
        uint8_t name[SENSOR_NAME_MAX_LEN];
        size_t size = sizeof(name);
        esp_camera_ioctl(device, CAM_SENSOR_G_NAME, name, &size);
    }
    /*Query caps*/
    sensor_capability_t caps = {0};
    esp_camera_ioctl(device, CAM_SENSOR_G_CAP, &caps, NULL);
    printf("cap = %u\n", caps.fmt_raw);

    /*Query formats and set/get format*/
    sensor_format_array_info_t formats = {0};
    esp_camera_ioctl(device, CAM_SENSOR_G_FORMAT_ARRAY, &formats, NULL);
    printf("format count = %d\n", formats.count);
    const sensor_format_t *parray = formats.format_array;
    for (int i = 0; i < formats.count; i++) {
        PRINT_CAM_SENSOR_FORMAT_INFO(&(parray[i].index));
    }
    esp_camera_ioctl(device, CAM_SENSOR_S_FORMAT, (void *)&(parray[0].index), NULL);

    const sensor_format_t *current_format = NULL;
    ret = esp_camera_ioctl(device, CAM_SENSOR_G_FORMAT, &current_format, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Format get fail");
    } else {
        PRINT_CAM_SENSOR_FORMAT_INFO(current_format);
    }

    int enable_flag = 1;
    /*Start sensor stream*/
    ret = esp_camera_ioctl(device, CAM_SENSOR_S_STREAM, &enable_flag, NULL);
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
    esp_mipi_csi_handle_t handle = NULL;
    ret = esp_mipi_csi_driver_install(MIPI_CSI_PORT0, &mipi_if_cfg, 0, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "csi init fail[%d]", ret);
        return;
    }

    isp_init(mipi_if_cfg.frame_width, mipi_if_cfg.frame_height, mipi_if_cfg.in_type, mipi_if_cfg.out_type, mipi_if_cfg.isp_enable, NULL);

    esp_mipi_csi_ops_t ops = {
        .alloc_buffer = camera_get_new_vb,
        .recved_data = camera_recvdone_vb,
    };
    if (esp_mipi_csi_ops_regist(handle, &ops) != ESP_OK) {
        ESP_LOGE(TAG, "ops register fail");
        return;
    }

    if (esp_mipi_csi_start(handle) != ESP_OK) {
        ESP_LOGE(TAG, "Driver start fail");
        return;
    }

    /*Delay to wait s_buffer_count>3*/
    delay_us(200000);

    ESP_LOGI(TAG, "Notify new buf");
    s_buffer_count = 0;
    ret = esp_mipi_csi_new_buffer_available(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Notify available buf fail, err=%d", ret);
        return;
    }

    delay_us(100000);

    if (esp_mipi_csi_stop(handle) != ESP_OK) {
        ESP_LOGE(TAG, "Driver stop fail");
        return;
    }
    ESP_LOGI(TAG, "Test Done");
}
