/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/ppa.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"
#include "esp_board_manager_includes.h"
#include "dev_display_lcd.h"

#define BUFFER_COUNT            4
#define STREAM_TASK_STACK       8192
#define STREAM_TASK_PRIORITY    5
#define TOUCH_TASK_STACK        4096
#define TOUCH_TASK_PRIORITY     4
#define AF_METER_WINDOW_DIV     5
#define AF_METER_WINDOW_MIN     8
#define AE_ADD_DEFAULT          8
#define AE_ADD_SUN_STEP         4
#define TARGET_OFFSET_SUN_STEP  4
/* Tap within this fraction of af_win0 diagonal: weight only, no hardware window move. */
#define AF_NEAR_DIAG_FRAC       0.5f
/* Raise af_win0 software weight by this amount: weight[0] = ori_weight + offset (cap 255). */
#define AF_WEIGHT_OFFSET        8
#define LCD_RGB565_BPP          2
#define PPA_FRAG_MAX            16

#define UI_BAR_H                72
#define UI_PAD                  8
#define UI_BTN_H                (UI_BAR_H - 2 * UI_PAD)
#define UI_CROSS_HALF           18
#define UI_CROSS_THICK          3
#define TOUCH_DEBOUNCE_MS       250
#define TOUCH_POLL_MS           40

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))
#define COL_BAR         RGB565(16, 16, 24)
#define COL_BTN         RGB565(48, 56, 72)
#define COL_BTN_SEL     RGB565(32, 120, 200)
#define COL_SUN         RGB565(240, 180, 32)
#define COL_RST         RGB565(180, 64, 64)
#define COL_TEXT        RGB565(240, 240, 240)
#define COL_CROSS       RGB565(0, 255, 80)

typedef enum {
    UI_BTN_AWB_AUTO = 0,
    UI_BTN_AWB_3K,
    UI_BTN_AWB_5K,
    UI_BTN_AWB_65K,
    UI_BTN_SUN,
    UI_BTN_RESET,
    UI_BTN_COUNT,
} ui_btn_id_t;

typedef struct {
    int x0;
    int y0;
    int x1;
    int y1;
    const char *label;
    uint16_t fill;
} ui_btn_t;

static const char *TAG = "touch_3a";

static int s_fd = -1;
static uint32_t s_width;
static uint32_t s_height;
static uint32_t s_pixfmt;
static uint8_t *s_cam_buf[BUFFER_COUNT];
static volatile bool s_run;
static TaskHandle_t s_stream_task;
static TaskHandle_t s_main_task;
static SemaphoreHandle_t s_ctrl_lock;
static int s_target_offset;
static bool s_ae_on;
static uint32_t s_ae_l;
static uint32_t s_ae_t;
static uint32_t s_ae_w;
static uint32_t s_ae_h;
static bool s_af0_ok;
static uint32_t s_af0_l;
static uint32_t s_af0_t;
static uint32_t s_af0_w;
static uint32_t s_af0_h;
/* JSON / reset-time AF window weights; spot focus does ori[0] + AF_WEIGHT_OFFSET. */
static uint8_t s_af_w_ori[ISP_AF_WINDOW_NUM];
static uint8_t s_ae_add = AE_ADD_DEFAULT;
static int s_awb_sel; /* 0=auto, else Kelvin */
static int s_focus_lcd_x = -1;
static int s_focus_lcd_y = -1;
static esp_lcd_panel_handle_t s_lcd_panel;
static uint16_t s_lcd_w;
static uint16_t s_lcd_h;
static size_t s_lcd_fb_bytes;
static void *s_lcd_fb[2];
static uint8_t s_ppa_fb_idx;
static ppa_client_handle_t s_ppa;
static esp_lcd_touch_handle_t s_touch;
static esp_lcd_panel_io_handle_t s_touch_io;
static ui_btn_t s_btns[UI_BTN_COUNT];

/* 5x7 glyphs for A-Z 0-9 space and a few symbols; bit0 = top row leftmost. */
static const uint8_t s_font5x7[][5] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['+'] = {0x08, 0x08, 0x3E, 0x08, 0x08},
    ['0'] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
    ['1'] = {0x00, 0x42, 0x7F, 0x40, 0x00},
    ['2'] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3'] = {0x21, 0x41, 0x45, 0x4B, 0x31},
    ['4'] = {0x18, 0x14, 0x12, 0x7F, 0x10},
    ['5'] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6'] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
    ['7'] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8'] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9'] = {0x06, 0x49, 0x49, 0x29, 0x1E},
    ['A'] = {0x7E, 0x11, 0x11, 0x11, 0x7E},
    ['B'] = {0x7F, 0x49, 0x49, 0x49, 0x36},
    ['C'] = {0x3E, 0x41, 0x41, 0x41, 0x22},
    ['E'] = {0x7F, 0x49, 0x49, 0x49, 0x41},
    ['K'] = {0x7F, 0x08, 0x14, 0x22, 0x41},
    ['L'] = {0x7F, 0x40, 0x40, 0x40, 0x40},
    ['N'] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
    ['O'] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
    ['R'] = {0x7F, 0x09, 0x19, 0x29, 0x46},
    ['S'] = {0x26, 0x49, 0x49, 0x49, 0x32},
    ['T'] = {0x01, 0x01, 0x7F, 0x01, 0x01},
    ['U'] = {0x3F, 0x40, 0x40, 0x40, 0x3F},
};

static void ui_layout(void)
{
    const char *labels[UI_BTN_COUNT] = {
        "AUTO", "3K", "5K", "65K", "SUN+", "RST"
    };
    const int widths[UI_BTN_COUNT] = {120, 90, 90, 100, 110, 100};
    int x = UI_PAD;
    int y = s_lcd_h - UI_BAR_H + UI_PAD;
    int i;

    for (i = 0; i < UI_BTN_COUNT; i++) {
        s_btns[i].x0 = x;
        s_btns[i].y0 = y;
        s_btns[i].x1 = x + widths[i];
        s_btns[i].y1 = y + UI_BTN_H;
        s_btns[i].label = labels[i];
        s_btns[i].fill = COL_BTN;
        x = s_btns[i].x1 + UI_PAD;
    }
    s_btns[UI_BTN_SUN].fill = COL_SUN;
    s_btns[UI_BTN_RESET].fill = COL_RST;
}

static void fb_fill_rect(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t color)
{
    int x;
    int y;

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > s_lcd_w) {
        x1 = s_lcd_w;
    }
    if (y1 > s_lcd_h) {
        y1 = s_lcd_h;
    }
    for (y = y0; y < y1; y++) {
        uint16_t *row = fb + y * s_lcd_w;

        for (x = x0; x < x1; x++) {
            row[x] = color;
        }
    }
}

static void fb_draw_char(uint16_t *fb, int x, int y, char c, uint16_t color, int scale)
{
    const uint8_t *g;
    int col;
    int row;
    unsigned uc = (unsigned char)c;

    if (uc >= sizeof(s_font5x7) / sizeof(s_font5x7[0])) {
        fb_fill_rect(fb, x, y, x + 5 * scale, y + 7 * scale, color);
        return;
    }
    g = s_font5x7[uc];
    for (col = 0; col < 5; col++) {
        uint8_t bits = g[col];

        for (row = 0; row < 7; row++) {
            if ((bits >> row) & 0x1) {
                int px = x + col * scale;
                int py = y + row * scale;

                fb_fill_rect(fb, px, py, px + scale, py + scale, color);
            }
        }
    }
}

static void fb_draw_text(uint16_t *fb, int x, int y, const char *s, uint16_t color, int scale)
{
    while (*s) {
        fb_draw_char(fb, x, y, *s, color, scale);
        x += (5 + 1) * scale;
        s++;
    }
}

static void draw_cross(uint16_t *fb, int cx, int cy)
{
    if (cx < 0 || cy < 0) {
        return;
    }
    fb_fill_rect(fb, cx - UI_CROSS_HALF, cy - UI_CROSS_THICK / 2,
                 cx + UI_CROSS_HALF, cy + UI_CROSS_THICK / 2 + 1, COL_CROSS);
    fb_fill_rect(fb, cx - UI_CROSS_THICK / 2, cy - UI_CROSS_HALF,
                 cx + UI_CROSS_THICK / 2 + 1, cy + UI_CROSS_HALF, COL_CROSS);
}

static void draw_ui(uint16_t *fb)
{
    int i;
    int sel = UI_BTN_AWB_AUTO;

    fb_fill_rect(fb, 0, s_lcd_h - UI_BAR_H, s_lcd_w, s_lcd_h, COL_BAR);

    if (s_awb_sel == 3000) {
        sel = UI_BTN_AWB_3K;
    } else if (s_awb_sel == 5000) {
        sel = UI_BTN_AWB_5K;
    } else if (s_awb_sel == 6500) {
        sel = UI_BTN_AWB_65K;
    }

    for (i = 0; i < UI_BTN_COUNT; i++) {
        uint16_t fill = s_btns[i].fill;
        int tw;
        int tx;
        int ty;

        if (i == sel && i <= UI_BTN_AWB_65K) {
            fill = COL_BTN_SEL;
        }
        fb_fill_rect(fb, s_btns[i].x0, s_btns[i].y0, s_btns[i].x1, s_btns[i].y1, fill);
        tw = (int)strlen(s_btns[i].label) * 6 * 2;
        tx = s_btns[i].x0 + ((s_btns[i].x1 - s_btns[i].x0) - tw) / 2;
        ty = s_btns[i].y0 + (UI_BTN_H - 7 * 2) / 2;
        fb_draw_text(fb, tx, ty, s_btns[i].label, COL_TEXT, 2);
    }
    draw_cross(fb, s_focus_lcd_x, s_focus_lcd_y);
}

static uint32_t default_spot_span(uint32_t frame)
{
    uint32_t divided = frame / AF_METER_WINDOW_DIV;

    if (divided >= AF_METER_WINDOW_MIN) {
        return divided;
    }
    if (frame < AF_METER_WINDOW_MIN) {
        return frame;
    }
    return AF_METER_WINDOW_MIN;
}

static void default_meter_rect(uint32_t cx, uint32_t cy, uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h)
{
    uint32_t win_w = default_spot_span(s_width);
    uint32_t win_h = default_spot_span(s_height);
    uint32_t left = cx > win_w / 2 ? cx - win_w / 2 : 0;
    uint32_t top = cy > win_h / 2 ? cy - win_h / 2 : 0;

    if (left + win_w > s_width) {
        left = s_width - win_w;
    }
    if (top + win_h > s_height) {
        top = s_height - win_h;
    }
    *x = left;
    *y = top;
    *w = win_w;
    *h = win_h;
}

static void fill_isp_window(isp_window_t *out, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    out->top_left.x = x;
    out->top_left.y = y;
    out->btm_right.x = x + w - 1;
    out->btm_right.y = y + h - 1;
}

static int ae_index(int x, int y)
{
    return x * ISP_AE_BLOCK_Y_NUM + y;
}

static uint8_t add_block_weight(uint8_t base, uint8_t add)
{
    uint16_t weight = (uint16_t)(base ? base : 1) + add;

    return weight > UINT8_MAX ? UINT8_MAX : (uint8_t)weight;
}

static bool ae_block_overlaps(uint32_t bx, uint32_t by, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint32_t blk_x0 = bx * s_width / ISP_AE_BLOCK_X_NUM;
    uint32_t blk_x1 = (bx + 1) * s_width / ISP_AE_BLOCK_X_NUM;
    uint32_t blk_y0 = by * s_height / ISP_AE_BLOCK_Y_NUM;
    uint32_t blk_y1 = (by + 1) * s_height / ISP_AE_BLOCK_Y_NUM;

    return x < blk_x1 && (x + w) > blk_x0 && y < blk_y1 && (y + h) > blk_y0;
}

static void log_awb_result(esp_err_t ret, const char *action)
{
    if (ret == ESP_ERR_NOT_SUPPORTED || ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "awb %s skipped, this AWB has no color-temperature table", action);
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "awb %s failed: %s", action, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "awb %s ok", action);
}

static void log_af_result(esp_err_t ret, const char *action)
{
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "af %s skipped, AF is not in this pipeline", action);
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "af %s failed: %s", action, esp_err_to_name(ret));
        return;
    }
}

static bool cache_af_win0(void)
{
    isp_window_t windows[ISP_AF_WINDOW_NUM];
    uint8_t weight[ISP_AF_WINDOW_NUM];
    esp_err_t ret = esp_video_isp_pipeline_get_af_windows(windows);

    if (ret == ESP_ERR_NOT_SUPPORTED) {
        s_af0_ok = false;
        return false;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get af windows failed: %s", esp_err_to_name(ret));
        s_af0_ok = false;
        return false;
    }

    s_af0_l = windows[0].top_left.x;
    s_af0_t = windows[0].top_left.y;
    s_af0_w = windows[0].btm_right.x - windows[0].top_left.x + 1;
    s_af0_h = windows[0].btm_right.y - windows[0].top_left.y + 1;
    if (s_af0_w == 0 || s_af0_h == 0) {
        s_af0_ok = false;
        return false;
    }

    ret = esp_video_isp_pipeline_get_af_window_weight(weight);
    if (ret == ESP_OK) {
        memcpy(s_af_w_ori, weight, sizeof(s_af_w_ori));
    } else if (ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "get af weight failed: %s, keep previous ori", esp_err_to_name(ret));
    }

    s_af0_ok = true;
    return true;
}

static void apply_ae_meter(void)
{
    uint8_t luma[ISP_AE_REGIONS];
    esp_err_t ret;

    ret = esp_video_isp_pipeline_reset_agc_meter_weight();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "meter reset failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_video_isp_pipeline_get_agc_meter_luma_weight(luma);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get weight failed: %s", esp_err_to_name(ret));
        return;
    }
    if (!s_ae_on) {
        return;
    }

    for (int bx = 0; bx < ISP_AE_BLOCK_X_NUM; bx++) {
        for (int by = 0; by < ISP_AE_BLOCK_Y_NUM; by++) {
            if (ae_block_overlaps((uint32_t)bx, (uint32_t)by, s_ae_l, s_ae_t, s_ae_w, s_ae_h)) {
                int idx = ae_index(bx, by);

                luma[idx] = add_block_weight(luma[idx], s_ae_add);
            }
        }
    }
    ret = esp_video_isp_pipeline_set_agc_meter_luma_weight(luma);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set weight failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "ae meter cam=(%" PRIu32 ",%" PRIu32 ") win=%" PRIu32 "x%" PRIu32 " ae_add=%u",
             s_ae_l + s_ae_w / 2, s_ae_t + s_ae_h / 2, s_ae_w, s_ae_h, s_ae_add);
}

/**
 * @brief Raise af_win0 weight only: weight[i] = ori, weight[0] = ori + AF_WEIGHT_OFFSET.
 *
 * IPA blends AF scores with these software weights. Other windows keep JSON ori
 * so a tap does not zero them out. Ori is cached at boot / after RST.
 */
static void apply_af_spot_weight(void)
{
    uint8_t weight[ISP_AF_WINDOW_NUM];
    int i;

    for (i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        weight[i] = s_af_w_ori[i];
    }
    weight[0] = add_block_weight(s_af_w_ori[0], AF_WEIGHT_OFFSET);
    log_af_result(esp_video_isp_pipeline_set_af_window_weight(weight), "weight");
}

/**
 * @brief Recenter hardware af_win0 only; keep win1/win2 geometry from pipeline.
 *
 * set_af_windows() still needs ISP_AF_WINDOW_NUM entries, but only [0] moves.
 * Then apply_af_spot_weight() raises win0 priority the same way as a near tap.
 */
static void apply_af_move_win0(void)
{
    isp_window_t af[ISP_AF_WINDOW_NUM];
    esp_err_t ret;

    if (!s_af0_ok) {
        return;
    }
    ret = esp_video_isp_pipeline_get_af_windows(af);
    if (ret != ESP_OK) {
        log_af_result(ret, "get win");
        return;
    }
    fill_isp_window(&af[0], s_af0_l, s_af0_t, s_af0_w, s_af0_h);
    log_af_result(esp_video_isp_pipeline_set_af_windows(af), "win");
    apply_af_spot_weight();
}

static void recenter_af_win0(uint32_t cx, uint32_t cy)
{
    uint32_t left = cx > s_af0_w / 2 ? cx - s_af0_w / 2 : 0;
    uint32_t top = cy > s_af0_h / 2 ? cy - s_af0_h / 2 : 0;

    if (left + s_af0_w > s_width) {
        left = s_width - s_af0_w;
    }
    if (top + s_af0_h > s_height) {
        top = s_height - s_af0_h;
    }
    s_af0_l = left;
    s_af0_t = top;
}

static bool tap_near_af_win0(uint32_t cam_x, uint32_t cam_y)
{
    float cx;
    float cy;
    float half_diag;
    float dist;

    if (!s_af0_ok) {
        return false;
    }
    cx = (float)s_af0_l + (float)s_af0_w * 0.5f;
    cy = (float)s_af0_t + (float)s_af0_h * 0.5f;
    half_diag = AF_NEAR_DIAG_FRAC * hypotf((float)s_af0_w, (float)s_af0_h);
    dist = hypotf((float)cam_x - cx, (float)cam_y - cy);
    return dist <= half_diag;
}

static bool apply_target_offset(int value)
{
    float offset = (float)value;
    esp_err_t ret = esp_video_isp_pipeline_set_agc_meter_target_offset(offset);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set target offset failed: %s", esp_err_to_name(ret));
        return false;
    }
    if (esp_video_isp_pipeline_get_agc_meter_target_offset(&offset) == ESP_OK) {
        s_target_offset = (int)lroundf(offset);
    } else {
        s_target_offset = value;
    }
    ESP_LOGI(TAG, "target_offset=%d", s_target_offset);
    return true;
}

static void do_reset(void)
{
    esp_err_t ret = esp_video_isp_pipeline_reset_agc_meter_weight();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "reset failed: %s", esp_err_to_name(ret));
        return;
    }
    s_target_offset = 0;
    (void)esp_video_isp_pipeline_set_agc_meter_target_offset(0.0f);
    s_ae_on = false;
    s_ae_add = AE_ADD_DEFAULT;
    s_awb_sel = 0;
    s_focus_lcd_x = -1;
    s_focus_lcd_y = -1;
    log_af_result(esp_video_isp_pipeline_reset_af(), "reset");
    log_awb_result(esp_video_isp_pipeline_set_awb_reset(), "reset");
    (void)cache_af_win0();
    ESP_LOGI(TAG, "reset AE/AF/AWB to JSON defaults");
}

static void set_awb_auto(void)
{
    s_awb_sel = 0;
    log_awb_result(esp_video_isp_pipeline_set_awb_reset(), "reset");
}

static void set_awb_ct(uint32_t ct)
{
    s_awb_sel = (int)ct;
    log_awb_result(esp_video_isp_pipeline_set_awb_ct(ct), "ct");
}

static void bump_sun(void)
{
    int next_add = (int)s_ae_add + AE_ADD_SUN_STEP;
    int next_off = s_target_offset + TARGET_OFFSET_SUN_STEP;

    if (next_add > 255) {
        next_add = 255;
    }
    if (next_off > (int)ESP_IPA_AGC_METER_TARGET_OFFSET_MAX) {
        next_off = (int)ESP_IPA_AGC_METER_TARGET_OFFSET_MAX;
    }
    s_ae_add = (uint8_t)next_add;
    (void)apply_target_offset(next_off);
    if (s_ae_on) {
        apply_ae_meter();
    }
    ESP_LOGI(TAG, "sun ae_add=%u target_offset=%d", s_ae_add, s_target_offset);
}

typedef struct {
    float scale;
    uint32_t block_w;
    uint32_t block_h;
    uint32_t in_off_x;
    uint32_t in_off_y;
    uint32_t dst_w;
    uint32_t dst_h;
    uint32_t out_off_x;
    uint32_t out_off_y;
} preview_cover_t;

static float ppa_cover_scale(uint32_t src_w, uint32_t src_h, uint16_t lcd_w, uint16_t lcd_h)
{
    float need = fmaxf((float)lcd_w / (float)src_w, (float)lcd_h / (float)src_h);
    uint32_t n = (uint32_t)ceilf(need * (float)PPA_FRAG_MAX);

    if (n < 1) {
        n = 1;
    }
    /* SRM x-scale fractional part must be even (1/16 steps). */
    if (n & 1u) {
        n++;
    }
    return (float)n / (float)PPA_FRAG_MAX;
}

/* Same crop the preview scaler uses: cover scale, center input crop, output offset. */
static bool preview_cover(preview_cover_t *cover)
{
    if (s_width == 0 || s_height == 0 || s_lcd_w == 0 || s_lcd_h == 0) {
        return false;
    }

    cover->scale = ppa_cover_scale(s_width, s_height, s_lcd_w, s_lcd_h);
    cover->block_w = ((uint32_t)((float)s_lcd_w / cover->scale)) & ~1u;
    cover->block_h = ((uint32_t)((float)s_lcd_h / cover->scale)) & ~1u;
    if (cover->block_w > s_width) {
        cover->block_w = s_width & ~1u;
    }
    if (cover->block_h > s_height) {
        cover->block_h = s_height & ~1u;
    }
    if (cover->block_w == 0 || cover->block_h == 0) {
        return false;
    }
    cover->in_off_x = ((s_width - cover->block_w) / 2) & ~1u;
    cover->in_off_y = ((s_height - cover->block_h) / 2) & ~1u;
    cover->dst_w = (uint32_t)((float)cover->block_w * cover->scale);
    cover->dst_h = (uint32_t)((float)cover->block_h * cover->scale);
    if (cover->dst_w > s_lcd_w) {
        cover->dst_w = s_lcd_w;
    }
    if (cover->dst_h > s_lcd_h) {
        cover->dst_h = s_lcd_h;
    }
    cover->out_off_x = (s_lcd_w - cover->dst_w) / 2;
    cover->out_off_y = (s_lcd_h - cover->dst_h) / 2;
    return true;
}

static bool lcd_point_to_cam(int lcd_x, int lcd_y, uint32_t *cam_x, uint32_t *cam_y)
{
    preview_cover_t cover;
    int x = lcd_x;
    int y = lcd_y;
    int x1;
    int y1;

    if (!preview_cover(&cover)) {
        return false;
    }
    x1 = (int)cover.out_off_x + (int)cover.dst_w - 1;
    y1 = (int)cover.out_off_y + (int)cover.dst_h - 1;
    if (x < (int)cover.out_off_x) {
        x = (int)cover.out_off_x;
    } else if (x > x1) {
        x = x1;
    }
    if (y < (int)cover.out_off_y) {
        y = (int)cover.out_off_y;
    } else if (y > y1) {
        y = y1;
    }

    *cam_x = cover.in_off_x + (uint32_t)((float)(x - (int)cover.out_off_x) / cover.scale);
    *cam_y = cover.in_off_y + (uint32_t)((float)(y - (int)cover.out_off_y) / cover.scale);
    if (*cam_x < cover.in_off_x) {
        *cam_x = cover.in_off_x;
    }
    if (*cam_y < cover.in_off_y) {
        *cam_y = cover.in_off_y;
    }
    if (*cam_x >= cover.in_off_x + cover.block_w) {
        *cam_x = cover.in_off_x + cover.block_w - 1;
    }
    if (*cam_y >= cover.in_off_y + cover.block_h) {
        *cam_y = cover.in_off_y + cover.block_h - 1;
    }
    if (*cam_x >= s_width) {
        *cam_x = s_width - 1;
    }
    if (*cam_y >= s_height) {
        *cam_y = s_height - 1;
    }
    return true;
}

/**
 * @brief Touch preview: AE spot meter + AF near/far policy on af_win0.
 *
 * AE always recenters a software meter rect and raises overlapping 5x5 luma weights.
 * AF (when in pipeline):
 * - near current af_win0 (within half diagonal): keep hardware windows, only
 *   set weight[0] = ori_weight + AF_WEIGHT_OFFSET;
 * - farther: move only af_win0 to the tap, keep win1/win2, then same weight bump.
 * Similar to IDF camera demos: app comments document the 3A policy for average users.
 */
static void focus_at_lcd(int lcd_x, int lcd_y)
{
    uint32_t cam_x;
    uint32_t cam_y;
    bool near;

    if (s_width == 0 || s_height == 0) {
        return;
    }
    if (lcd_y >= s_lcd_h - UI_BAR_H) {
        return;
    }
    if (!lcd_point_to_cam(lcd_x, lcd_y, &cam_x, &cam_y)) {
        return;
    }

    if (!s_af0_ok) {
        (void)cache_af_win0();
    }

    default_meter_rect(cam_x, cam_y, &s_ae_l, &s_ae_t, &s_ae_w, &s_ae_h);
    s_ae_on = true;
    s_focus_lcd_x = lcd_x;
    s_focus_lcd_y = lcd_y;
    apply_ae_meter();

    near = tap_near_af_win0(cam_x, cam_y);
    if (near) {
        apply_af_spot_weight();
        ESP_LOGI(TAG, "af near win0, weight-only cam=(%" PRIu32 ",%" PRIu32 ") w0=%u",
                 cam_x, cam_y, add_block_weight(s_af_w_ori[0], AF_WEIGHT_OFFSET));
    } else if (s_af0_ok) {
        recenter_af_win0(cam_x, cam_y);
        apply_af_move_win0();
        ESP_LOGI(TAG, "af far, move win0 to (%" PRIu32 ",%" PRIu32 ") size=%" PRIu32 "x%" PRIu32 " w0=%u",
                 s_af0_l + s_af0_w / 2, s_af0_t + s_af0_h / 2, s_af0_w, s_af0_h,
                 add_block_weight(s_af_w_ori[0], AF_WEIGHT_OFFSET));
    } else {
        ESP_LOGW(TAG, "af skipped, no win0 cached");
    }
}

static int hit_button(int x, int y)
{
    int i;

    for (i = 0; i < UI_BTN_COUNT; i++) {
        if (x >= s_btns[i].x0 && x < s_btns[i].x1 &&
                y >= s_btns[i].y0 && y < s_btns[i].y1) {
            return i;
        }
    }
    return -1;
}

static void handle_touch(int x, int y)
{
    int btn;

    if (xSemaphoreTake(s_ctrl_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
        return;
    }

    btn = hit_button(x, y);
    if (btn == UI_BTN_AWB_AUTO) {
        set_awb_auto();
    } else if (btn == UI_BTN_AWB_3K) {
        set_awb_ct(3000);
    } else if (btn == UI_BTN_AWB_5K) {
        set_awb_ct(5000);
    } else if (btn == UI_BTN_AWB_65K) {
        set_awb_ct(6500);
    } else if (btn == UI_BTN_SUN) {
        bump_sun();
    } else if (btn == UI_BTN_RESET) {
        do_reset();
    } else if (y < s_lcd_h - UI_BAR_H) {
        focus_at_lcd(x, y);
    }

    xSemaphoreGive(s_ctrl_lock);
}

static esp_err_t init_touch(void)
{
    i2c_master_bus_handle_t bus = NULL;
    const uint16_t addrs[] = {
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP,
    };
    uint16_t found = 0;
    esp_err_t ret;

    ret = i2c_master_get_bus_handle(EXAMPLE_MIPI_CSI_SCCB_PORT, &bus);
    if (ret != ESP_OK || bus == NULL) {
        ESP_LOGE(TAG, "get SCCB bus failed: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        if (i2c_master_probe(bus, addrs[i], 200) == ESP_OK) {
            found = addrs[i];
            break;
        }
    }
    if (found == 0) {
        ESP_LOGE(TAG, "GT911 not found on I2C port %d", EXAMPLE_MIPI_CSI_SCCB_PORT);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "GT911 at 0x%02x", found);

    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.dev_addr = found;
    ret = esp_lcd_new_panel_io_i2c(bus, &io_cfg, &s_touch_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch panel io failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = s_lcd_w,
        .y_max = s_lcd_h,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    ret = esp_lcd_touch_new_i2c_gt911(s_touch_io, &tp_cfg, &s_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gt911 create failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static void touch_task(void *arg)
{
    bool was_pressed = false;
    int64_t last_ms = 0;

    (void)arg;
    while (s_run) {
        esp_lcd_touch_point_data_t point[1];
        uint8_t count = 0;

        if (s_touch) {
            esp_lcd_touch_read_data(s_touch);
            if (esp_lcd_touch_get_data(s_touch, point, &count, 1) == ESP_OK && count > 0) {
                int64_t now = esp_timer_get_time() / 1000;

                if (!was_pressed && (now - last_ms) >= TOUCH_DEBOUNCE_MS) {
                    handle_touch(point[0].x, point[0].y);
                    last_ms = now;
                }
                was_pressed = true;
            } else {
                was_pressed = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_MS));
    }
    vTaskDelete(NULL);
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

    ui_layout();
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
    preview_cover_t cover;
    void *back_fb;
    ppa_srm_oper_config_t srm;

    if (!s_ppa || !s_lcd_fb[0] || !s_lcd_fb[1] || !s_lcd_panel) {
        return;
    }
    if (!preview_cover(&cover)) {
        return;
    }

    back_fb = s_lcd_fb[s_ppa_fb_idx];
    memset(back_fb, 0, s_lcd_fb_bytes);
    (void)esp_cache_msync(back_fb, s_lcd_fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    memset(&srm, 0, sizeof(srm));
    srm.in.buffer = frame;
    srm.in.pic_w = s_width;
    srm.in.pic_h = s_height;
    srm.in.block_w = cover.block_w;
    srm.in.block_h = cover.block_h;
    srm.in.block_offset_x = cover.in_off_x;
    srm.in.block_offset_y = cover.in_off_y;
    srm.in.srm_cm = cam_ppa_color_mode(s_pixfmt);
    srm.in.yuv_range = PPA_COLOR_RANGE_LIMIT;
    srm.in.yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601;
    srm.out.buffer = back_fb;
    srm.out.buffer_size = s_lcd_fb_bytes;
    srm.out.pic_w = s_lcd_w;
    srm.out.pic_h = s_lcd_h;
    srm.out.block_offset_x = cover.out_off_x;
    srm.out.block_offset_y = cover.out_off_y;
    srm.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    srm.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
    srm.scale_x = cover.scale;
    srm.scale_y = cover.scale;
    srm.mode = PPA_TRANS_MODE_BLOCKING;

    if (ppa_do_scale_rotate_mirror(s_ppa, &srm) != ESP_OK) {
        ESP_LOGE(TAG, "PPA scale failed");
        return;
    }

    if (xSemaphoreTake(s_ctrl_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        draw_ui((uint16_t *)back_fb);
        xSemaphoreGive(s_ctrl_lock);
        (void)esp_cache_msync(back_fb, s_lcd_fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }

    esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, 0, s_lcd_w, s_lcd_h, back_fb);
    s_ppa_fb_idx ^= 1;
}

static void stream_task(void *arg)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bool logged = false;

    (void)arg;
    while (s_run) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(s_fd, VIDIOC_DQBUF, &buf) != 0) {
            if (s_run) {
                ESP_LOGE(TAG, "failed to receive video frame");
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
    xTaskNotifyGive(s_main_task);
    s_stream_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t start_stream(void)
{
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format format = {
        .type = type,
    };
    struct v4l2_requestbuffers req;
    const uint32_t try_fmts[] = { V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_UYVY };

    s_fd = open(EXAMPLE_CAM_DEV_PATH, O_RDONLY);
    if (s_fd < 0) {
        ESP_LOGE(TAG, "failed to open %s", EXAMPLE_CAM_DEV_PATH);
        return ESP_FAIL;
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
    if (xTaskCreate(stream_task, "t3a_stream", STREAM_TASK_STACK, NULL, STREAM_TASK_PRIORITY, &s_stream_task) != pdPASS) {
        ESP_LOGE(TAG, "failed to create stream task");
        ioctl(s_fd, VIDIOC_STREAMOFF, &type);
        goto fail;
    }
    if (xTaskCreate(touch_task, "t3a_touch", TOUCH_TASK_STACK, NULL, TOUCH_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create touch task");
        s_run = false;
        ioctl(s_fd, VIDIOC_STREAMOFF, &type);
        goto fail;
    }

    ESP_LOGI(TAG, "[ 1 ] touch 3A ready, camera %" PRIu32 "x%" PRIu32 " on LCD %" PRIu16 "x%" PRIu16,
             s_width, s_height, s_lcd_w, s_lcd_h);
    if (cache_af_win0()) {
        ESP_LOGI(TAG, "af_win0 (%" PRIu32 ",%" PRIu32 ")-(%" PRIu32 ",%" PRIu32 "), near=%.0f%% diagonal",
                 s_af0_l, s_af0_t, s_af0_l + s_af0_w - 1, s_af0_t + s_af0_h - 1,
                 (double)(AF_NEAR_DIAG_FRAC * 100.0f));
    }
    ESP_LOGI(TAG, "tap near af_win0: AF weight only; tap far: move af_win0; bar: AWB/SUN+/RST");
    return ESP_OK;

fail:
    close(s_fd);
    s_fd = -1;
    return ESP_FAIL;
}

static void stop_stream(void)
{
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    s_run = false;
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3000)) == 0 && s_fd >= 0) {
        ESP_LOGW(TAG, "stream task is still inside the driver, stopping the stream");
        ioctl(s_fd, VIDIOC_STREAMOFF, &type);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    } else if (s_fd >= 0) {
        ioctl(s_fd, VIDIOC_STREAMOFF, &type);
    }

    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
    }
    ESP_LOGI(TAG, "[ 2 ] stream stopped");
}

void app_main(void)
{
    esp_err_t ret;

    s_ctrl_lock = xSemaphoreCreateMutex();
    if (!s_ctrl_lock) {
        ESP_LOGE(TAG, "mutex alloc failed");
        return;
    }

    /* LDO_MIPI must be on before CSI/DSI; board manager owns the channel. */
    ret = esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
    ESP_GOTO_ON_ERROR(ret, exit, TAG, "init MIPI LDO");
    ret = example_video_init();
    ESP_GOTO_ON_ERROR(ret, exit, TAG, "camera init failed");
    ret = init_lcd();
    ESP_GOTO_ON_ERROR(ret, deinit, TAG, "LCD init failed");
    ret = init_touch();
    ESP_GOTO_ON_ERROR(ret, deinit, TAG, "touch init failed");
    ret = init_ppa();
    ESP_GOTO_ON_ERROR(ret, deinit, TAG, "PPA init failed");
    ret = start_stream();
    ESP_GOTO_ON_ERROR(ret, deinit, TAG, "camera stream failed");

    while (s_run) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    stop_stream();

deinit:
    ESP_ERROR_CHECK(example_video_deinit());
exit:
    return;
}
