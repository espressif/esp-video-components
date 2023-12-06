/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_types.h>
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_check.h"
#include "rom/cache.h"
#include "soc/dw_gdma_struct.h"
#include "soc/interrupt_core0_reg.h"

#include "mipi_csi_hal.h"
#include "mipi_csi.h"
#include "esp_color_formats.h"

#define MIPI_CSI_HANDLE_NULL_ERROR_STR               "cam handle can't be NULL"
#define MIPI_CSI_NULL_POINTER_CHECK(tag, p)          ESP_RETURN_ON_FALSE((p), ESP_ERR_INVALID_ARG, tag, "input parameter '"#p"' is NULL")

extern uint32_t MIPI_CSI_MEM;
#define MIPI_CSI_DMA_TRANS_SAR  &MIPI_CSI_MEM

#define GDMA_INTR_SOURCE        (((INTERRUPT_CORE0_GDMA_INT_MAP_REG - DR_REG_INTERRUPT_CORE0_BASE) / 4))
#define DW_GDMA_BUFFER_MASTER   (1)
#define CONFIG_CSI_TR_WIDTH     (64)

#define AXI_DMA_CH_ENA_S        (0)
#define AXI_DMA_CH_ENA_WE_S     (8)
#define TEST_AXIDMA_CH  0

typedef enum {
    MIPI_CSI_STATE_IDLE = 0,
    MIPI_CSI_STATE_READY,
    MIPI_CSI_STATE_START,
    MIPI_CSI_STATE_STOP,
    MIPI_CSI_STATE_SUSPEND
} mipi_csi_driv_state_t;

typedef struct {
    camera_fb_t fb;        // for application
    uint32_t dma_buf_addr; // dam addr for fb->buf
    size_t fb_offset;      // for dma align
} mipi_csi_dam_frames_t;

typedef struct esp_mipi_csi_obj {
    mipi_csi_dam_frames_t frames;
    uint8_t dma_channel_num;
    intr_handle_t dma_intr_handle;
    mipi_csi_driv_config_t *driver_config;
    mipi_csi_interface_t interface;
    mipi_csi_driv_state_t state;
    esp_mipi_csi_ops_t ops;
    portMUX_TYPE spinlock;
} esp_mipi_csi_obj_t;

const char *MIPI_CSI_TAG = "MIPI_CSI";

static inline void axi_dma_ll_channel_ena(uint8_t channel)
{
    DW_GDMA.chen0.val = 0x101 << channel;
}

static inline void axi_dma_ll_channel_disable(uint8_t channel)
{
    DW_GDMA.chen0.val = 0x100 << channel;
    return;
}

static inline void esp_mipi_csi_enable_gdma_with_addr(esp_mipi_csi_obj_t *csi_cam_obj, uint32_t start_gdma_addr)
{
    DW_GDMA.ch[csi_cam_obj->dma_channel_num].dar0.dar0 = start_gdma_addr;
    axi_dma_ll_channel_ena(csi_cam_obj->dma_channel_num);
    return;
}

static inline void esp_mipi_csi_disable_gdma(esp_mipi_csi_obj_t *csi_cam_obj)
{
    axi_dma_ll_channel_disable(csi_cam_obj->dma_channel_num);
}

static inline uint8_t esp_mipi_csi_get_dma_align(void)
{
    // Transfer width. 0x2: transfer width is 32bits, 0x03: trans width is 64bits
    // return (GDMA.ch[cam->dma_channel_num].ctl0.src_tr_width == 0x03 ? 8: 4);
    return (CONFIG_CSI_TR_WIDTH == 64 ? 8 : 4);
}

static uint32_t dma_access_addr_map(uint32_t addr)
{
    uint32_t map_addr = addr;

    if (addr >= 0x30100000 && addr <= 0x30103FFF) {
    } else if (addr == 0 || addr & 0x80000000) {

    } else {
        map_addr += 0x40000000;
    }

    return map_addr;
}

static inline bool esp_mipi_csi_get_next_free_framebuf(esp_mipi_csi_obj_t *csi_cam_obj)
{
    if (csi_cam_obj->ops.alloc_buffer) {
        // Todo, DMA align offset too large
        csi_cam_obj->frames.fb.buf = csi_cam_obj->ops.alloc_buffer(csi_cam_obj->driver_config->fb_size);
        if (csi_cam_obj->frames.fb.buf) {
            uint8_t dma_align = esp_mipi_csi_get_dma_align();
            if ((uint32_t)csi_cam_obj->frames.fb.buf % dma_align) {
                csi_cam_obj->frames.fb_offset = dma_align - ((uint32_t)csi_cam_obj->frames.fb.buf % dma_align);
            } else {
                csi_cam_obj->frames.fb_offset = 0;
            }
            csi_cam_obj->frames.fb.len = csi_cam_obj->driver_config->fb_size;
            Cache_WriteBack_Addr(CACHE_MAP_L1_DCACHE | CACHE_MAP_L2_CACHE,
                                 (uint32_t)csi_cam_obj->frames.fb.buf + csi_cam_obj->frames.fb_offset, csi_cam_obj->driver_config->fb_size);
            csi_cam_obj->frames.dma_buf_addr = dma_access_addr_map((uint32_t)csi_cam_obj->frames.fb.buf + csi_cam_obj->frames.fb_offset);// get dma addr;
            // esp_rom_printf("%s %d line 0x%x\r\n", __func__, __LINE__, csi_cam_obj->frames.dma_buf_addr);
            return true;
        } else {
            esp_rom_printf("%s: Alloc fb buffer failed\r\n", MIPI_CSI_TAG);
        }
    }

    return false;
}

static bool IRAM_ATTR esp_mipi_csi_start_framebuf_filled(esp_mipi_csi_obj_t *csi_cam_obj)
{
    if (esp_mipi_csi_get_next_free_framebuf(csi_cam_obj)) {
        portENTER_CRITICAL_SAFE(&csi_cam_obj->spinlock);
        csi_cam_obj->frames.fb.len = 0;
        csi_cam_obj->state = MIPI_CSI_STATE_START;
        portEXIT_CRITICAL_SAFE(&csi_cam_obj->spinlock);
        esp_mipi_csi_enable_gdma_with_addr(csi_cam_obj, csi_cam_obj->frames.dma_buf_addr);
        return true;
    }
    return false;
}

static mipi_csi_driv_config_t *mipi_csi_new_driver_cfg(mipi_csi_port_config_t *config)
{
    mipi_csi_driv_config_t *cfg = (mipi_csi_driv_config_t *)calloc(sizeof(mipi_csi_driv_config_t), 1);
    if (cfg == NULL) {
        return cfg;
    }

    cfg->height = config->frame_height;
    cfg->width = config->frame_width;

    cfg->in_type = config->in_type;
    cfg->out_type = config->out_type;
    cfg->in_type_bits_per_pixel = pixformat_info_map[config->in_type].bits_per_pixel;

    cfg->out_type_bits_per_pixel = pixformat_info_map[config->out_type].bits_per_pixel;
    cfg->cam_isp_en = config->isp_enable;

    // Note: Width * Height * BitsPerPixel must be divisible by 8
    size_t fb_size_in_bits = cfg->width * cfg->height * cfg->out_type_bits_per_pixel;
    if (fb_size_in_bits % 8) {
        ESP_LOGD(MIPI_CSI_TAG, "framesize not 8bit aligned");
        free(cfg);
        return NULL;
    }

    cfg->fb_size = fb_size_in_bits >> 3;
    // ESP_LOGD(MIPI_CSI_TAG, "FB Size=%u", cfg->fb_size);
    return cfg;
}

static esp_err_t mipi_csi_del_driver_cfg(mipi_csi_driv_config_t *config)
{
    if (config) {
        free(config);
    }

    return ESP_OK;
}

static int esp_mipi_csi_gdma_config(esp_mipi_csi_obj_t *driv_obj)
{
    // Block transfer size
    uint32_t csi_block_ts;

    // To do, find a free channle to use. if not, just return err.
    driv_obj->dma_channel_num = TEST_AXIDMA_CH;

    // if ISP used, this should be ISP output bits_per_pixel. otherwise this should be sensor output bits_per_pixel
    csi_block_ts = driv_obj->driver_config->width * driv_obj->driver_config->height * driv_obj->driver_config->out_type_bits_per_pixel / CONFIG_CSI_TR_WIDTH;
    // ESP_LOGD(MIPI_CSI_TAG, "DMA Ch=%u, csi block trans size=%u", driv_obj->dma_channel_num, csi_block_ts);

    // Enable GDMA(global control)

    DW_GDMA.cfg0.val = 0x0;
    DW_GDMA.cfg0.int_en = 0x1;
    DW_GDMA.cfg0.dmac_en = 0x1;

    // CSI DMA interrupt enable
    // INTSTATUS_ENABLEREG
    DW_GDMA.ch[driv_obj->dma_channel_num].int_st_ena0.val = 0x0;
    // INTSIGNAL_ENABLEREG
    DW_GDMA.ch[driv_obj->dma_channel_num].int_sig_ena0.val = 0x0;
    // INTSTATUS_ENABLEREG, enable trans done interrupt
    DW_GDMA.ch[driv_obj->dma_channel_num].int_st0.dma_tfr_done_intstat = 0x1;
    // enable trans done single
    DW_GDMA.ch[driv_obj->dma_channel_num].int_sig_ena0.enable_dma_tfr_done_intsignal = 0x1;

    DW_GDMA.ch[driv_obj->dma_channel_num].cfg0.val = 0x0;
    // Define the number of AXI Unique ID's supported for the AXI write/read channel
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg0.wr_uid = 0x2;
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg0.rd_uid = 0x2;
    // Source Mutil Block Transfer Type, 0x00 is contiguous
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg0.src_multblk_type = 0x0;
    // Destination Mutil Block Transfer Type, 0x00 is contiguous
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg0.dst_multblk_type = 0x0;
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg1.val = 0x0;
    // Transfer type and flow control, 0x4: transfer type is peripheral to memory and flow controller is source peripheral
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg1.tt_fc = 0x4;
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg1.hs_sel_src = 0x0;
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg1.hs_sel_dst = 0x0;
    // Assigns a hardware handshaking interface
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg1.src_per = 0x1;
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg1.dst_per = 0x0;
    // Source outstanding request limit
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg1.src_osr_lmt = 0x1;
    DW_GDMA.ch[driv_obj->dma_channel_num].cfg1.dst_osr_lmt = 0x4;
    // Source Address Register
    DW_GDMA.ch[driv_obj->dma_channel_num].sar0.sar0 = (uint32_t)MIPI_CSI_DMA_TRANS_SAR;
    // Destination Address Register
    DW_GDMA.ch[driv_obj->dma_channel_num].dar0.dar0 = driv_obj->frames.dma_buf_addr;

    // Block transfer size
    DW_GDMA.ch[driv_obj->dma_channel_num].block_ts0.val = csi_block_ts - 1;

    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.val = 0x0;
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.sms = 0x0;
    // Destination master select
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.dms = DW_GDMA_BUFFER_MASTER;

    // Source address increment. 1: no change, 0: increment.
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.sinc = 0x1;
    // Destination address increment. 1: no change, 0: increment
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.dinc = 0x0;

    // Transfer width. 0x2: transfer width is 32bits, 0x03: trans width is 64bits
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.src_tr_width = (CONFIG_CSI_TR_WIDTH == 64 ? 0x3 : 0x2);
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.dst_tr_width = (CONFIG_CSI_TR_WIDTH == 64 ? 0x3 : 0x2);
    // Burst transaction length. 0x7: 256 Data Item read from Source in the burst transaction
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.src_msize = 0x7;
    // Burst transaction length. 0x8: 512 Data Item read from Destination in the burst transaction
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl0.dst_msize = 0x8;
    // Interrupt on completion of block transfer, 0 disable block transfer done
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl1.ioc_blktfr = 0;
    // Source status fetch register
    DW_GDMA.ch[driv_obj->dma_channel_num].sstatar0.sstatar0 = (uint32_t)MIPI_CSI_DMA_TRANS_SAR + 16;

    // Source burst length
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl1.arlen = 16;
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl1.arlen_en = 1;
    // Destination burst length
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl1.awlen = 16;
    DW_GDMA.ch[driv_obj->dma_channel_num].ctl1.awlen_en = 1;

    // vTaskDelay((2/portTICK_PERIOD_MS)?(2/portTICK_PERIOD_MS):1);
    vTaskDelay(2 / portTICK_PERIOD_MS);

    ESP_LOGI(MIPI_CSI_TAG, "GDMA init done");
    return 0;
}

static esp_err_t esp_mipi_csi_set_config(mipi_csi_interface_t interface, mipi_csi_config_t *config)
{
    esp_err_t ret = ESP_OK;
    mipi_csi_hal_context_t hal;
    if (interface > MIPI_CSI_PORT_MAX || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Todo, remove this when ESP32-P4 System clk be ready
    // SYS_CLKRST.csi_ctrl.csi_clk_div_num = (480000000 / 240000000) - 1;
    // SYS_CLKRST.csi_ctrl.csi_apb_clk_en = 1;
    // SYS_CLKRST.csi_ctrl.csi_clk_en = 1;
    // SYS_CLKRST.csi_ctrl.csi_clk_sync_en = 1;
    // SYS_CLKRST.csi_ctrl.csi_clk_force_sync_en = 1;
    // SYS_CLKRST.csi_ctrl.csi_rstn = 0;
    // SYS_CLKRST.csi_ctrl.csi_rstn = 1;
    // SYS_CLKRST.csi_ctrl.csi_brg_rstn = 0;
    // SYS_CLKRST.csi_ctrl.csi_brg_rstn = 1;

    hal.bridge_dev = MIPI_CSI_BRIDGE_LL_GET_HW(0);
    hal.host_dev = MIPI_CSI_HOST_LL_GET_HW(0);
    hal.frame_height = config->frame_height;
    hal.frame_width = config->frame_width;
    hal.mipi_clk = config->mipi_clk;
    hal.in_bits_per_pixel = config->in_bits_per_pixel;
    hal.lanes_num = config->lane_num;
    hal.version = MIPI_CSI_HAL_LAYER_VERSION;

    mipi_csi_hal_host_phy_initialization(&hal);
    mipi_csi_hal_bridge_initialization(&hal);
    return ret;
}

// Configure the relevant parameters on ESP32.
static esp_err_t mipi_csi_set_mipi_config(mipi_csi_interface_t interface, const mipi_csi_port_config_t *config)
{
    mipi_csi_config_t mipi_config = {0};

    mipi_config.lane_num = config->lane_num;
    mipi_config.mipi_clk = config->mipi_clk_freq_hz;
    mipi_config.frame_width = config->frame_width;
    mipi_config.frame_height = config->frame_height;
    mipi_config.in_bits_per_pixel = pixformat_info_map[config->in_type].bits_per_pixel;
    mipi_config.out_bits_per_pixel = pixformat_info_map[config->out_type].bits_per_pixel;

    return esp_mipi_csi_set_config(MIPI_CSI_PORT_NUM_DEFAULT, &mipi_config);
};

static void IRAM_ATTR esp_mipi_csi_grab_mode_gdma_isr(void *arg)
{
    esp_mipi_csi_obj_t *driv_obj_ptr = (esp_mipi_csi_obj_t *)arg;
    esp_mipi_csi_disable_gdma(driv_obj_ptr);
    portBASE_TYPE HPTaskAwoken = pdFALSE;
    camera_fb_t *frame_buffer_event = &driv_obj_ptr->frames.fb;

    typeof(DW_GDMA.ch[driv_obj_ptr->dma_channel_num].int_st0) status = DW_GDMA.ch[driv_obj_ptr->dma_channel_num].int_st0;
    if (status.dma_tfr_done_intstat) { // Is channelx "transfer done" interrupt event.
        driv_obj_ptr->frames.fb.len = driv_obj_ptr->driver_config->fb_size;
#if DEBUG_FB_SEQ
        driv_obj_ptr->frames.fb.frame_trans_cnt++;
#endif
        Cache_Invalidate_Addr(CACHE_MAP_L1_DCACHE | CACHE_MAP_L2_CACHE, (uint32_t)frame_buffer_event->buf + driv_obj_ptr->frames.fb_offset, driv_obj_ptr->driver_config->fb_size);
        if (driv_obj_ptr->ops.recved_data
                && (driv_obj_ptr->ops.recved_data(frame_buffer_event->buf, driv_obj_ptr->frames.fb_offset, driv_obj_ptr->driver_config->fb_size) != ESP_OK)) {
            Cache_WriteBack_Addr(CACHE_MAP_L1_DCACHE | CACHE_MAP_L2_CACHE,
                                 (uint32_t)driv_obj_ptr->frames.fb.buf + driv_obj_ptr->frames.fb_offset, driv_obj_ptr->driver_config->fb_size);
            esp_mipi_csi_enable_gdma_with_addr(driv_obj_ptr, driv_obj_ptr->frames.dma_buf_addr);
        } else {
            if (!esp_mipi_csi_start_framebuf_filled(driv_obj_ptr)) {
                driv_obj_ptr->state = MIPI_CSI_STATE_SUSPEND;
            }
        }
    }

    DW_GDMA.ch[driv_obj_ptr->dma_channel_num].int_clr0.val = 0xffffffff;

    // We only need to check here if there is a high-priority task needs to be switched.
    if (HPTaskAwoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t esp_mipi_csi_driver_install(mipi_csi_interface_t interface, mipi_csi_port_config_t *config, int intr_alloc_flags, esp_mipi_csi_handle_t *ret_handle)
{
    if ((interface > MIPI_CSI_PORT_MAX)) {
        return ESP_ERR_INVALID_STATE;
    }

    // printf("MIPI_CSI_DMA_TRANS_SAR=0x%p\r\n", MIPI_CSI_DMA_TRANS_SAR);
    esp_mipi_csi_obj_t *csi_obj_p = (esp_mipi_csi_obj_t *)calloc(sizeof(esp_mipi_csi_obj_t), 1);
    if (csi_obj_p == NULL) {
        return ESP_ERR_NO_MEM;
    }

    csi_obj_p->driver_config = mipi_csi_new_driver_cfg(config);
    if (!csi_obj_p->driver_config) {
        free(csi_obj_p);
        return ESP_ERR_INVALID_ARG;
    }

    if (mipi_csi_set_mipi_config(interface, config) != ESP_OK) {
        ESP_LOGE(MIPI_CSI_TAG, "MIPI set config fail");
    }

    esp_mipi_csi_gdma_config(csi_obj_p);

    if (esp_intr_alloc(GDMA_INTR_SOURCE, ESP_INTR_FLAG_LEVEL2, esp_mipi_csi_grab_mode_gdma_isr, csi_obj_p, &csi_obj_p->dma_intr_handle) != ESP_OK) {
        ESP_LOGE(MIPI_CSI_TAG, "Alloc gdma itr fail");
        mipi_csi_del_driver_cfg(csi_obj_p->driver_config);
        free(csi_obj_p);
        return ESP_ERR_NO_MEM;
    }

    csi_obj_p->interface = interface;
    csi_obj_p->state = MIPI_CSI_STATE_READY;
    csi_obj_p->spinlock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;

    *ret_handle = csi_obj_p;
    return ESP_OK;
}

esp_err_t esp_mipi_csi_ops_regist(esp_mipi_csi_handle_t handle, esp_mipi_csi_ops_t *ops)
{
    esp_mipi_csi_obj_t *csi_cam_obj = (esp_mipi_csi_obj_t *)handle;
    if (csi_cam_obj) {
        if (ops) {
            csi_cam_obj->ops.alloc_buffer = ops->alloc_buffer;
            csi_cam_obj->ops.recved_data = ops->recved_data;
        } else {
            csi_cam_obj->ops.alloc_buffer = NULL;
            csi_cam_obj->ops.recved_data = NULL;
        }
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t esp_mipi_csi_start(esp_mipi_csi_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, MIPI_CSI_TAG, MIPI_CSI_HANDLE_NULL_ERROR_STR);
    esp_mipi_csi_obj_t *csi_cam_obj = (esp_mipi_csi_obj_t *)handle;
    if (csi_cam_obj->state != MIPI_CSI_STATE_READY && csi_cam_obj->state != MIPI_CSI_STATE_STOP) {
        ESP_LOGE(MIPI_CSI_TAG, "invalid status");
        return ESP_ERR_INVALID_STATE;
    }

    if (esp_mipi_csi_start_framebuf_filled(csi_cam_obj) != true) {
        ESP_LOGE(MIPI_CSI_TAG, "start failed");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(MIPI_CSI_TAG, "mipi csi transfer start");
    return ESP_OK;
}

esp_err_t esp_mipi_csi_stop(esp_mipi_csi_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, MIPI_CSI_TAG, MIPI_CSI_HANDLE_NULL_ERROR_STR);
    esp_mipi_csi_obj_t *csi_cam_obj = (esp_mipi_csi_obj_t *)handle;

    if (csi_cam_obj->state == MIPI_CSI_STATE_START || csi_cam_obj->state == MIPI_CSI_STATE_SUSPEND) {
        portENTER_CRITICAL_SAFE(&csi_cam_obj->spinlock);
        esp_mipi_csi_disable_gdma(csi_cam_obj);
        csi_cam_obj->state = MIPI_CSI_STATE_STOP;
        portEXIT_CRITICAL_SAFE(&csi_cam_obj->spinlock);
    }

    return ESP_OK;
}

esp_err_t esp_mipi_csi_driver_delete(esp_mipi_csi_handle_t handle)
{
    MIPI_CSI_NULL_POINTER_CHECK(MIPI_CSI_TAG, handle);
    esp_mipi_csi_obj_t *csi_cam_obj = (esp_mipi_csi_obj_t *)handle;
    esp_mipi_csi_stop(handle);

    if (csi_cam_obj->dma_intr_handle) {
        esp_intr_free(csi_cam_obj->dma_intr_handle);
        csi_cam_obj->dma_intr_handle = NULL;
    }

    free(csi_cam_obj);
    csi_cam_obj = NULL;
    handle = NULL;
    return ESP_OK;
}

esp_err_t esp_mipi_csi_new_buffer_available(esp_mipi_csi_handle_t handle)
{
    esp_mipi_csi_obj_t *csi_cam_obj = (esp_mipi_csi_obj_t *)handle;
    if (csi_cam_obj->state == MIPI_CSI_STATE_SUSPEND) {
        if (esp_mipi_csi_start_framebuf_filled(csi_cam_obj) != true) {
            return ESP_ERR_CAMERA_FAILED_TO_START_FRAME;
        }
    } else {
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}
