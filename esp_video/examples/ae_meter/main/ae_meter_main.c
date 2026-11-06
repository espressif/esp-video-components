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
#include "freertos/task.h"
#include "driver/ppa.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "example_video_common.h"
#include "esp_video_isp_pipeline.h"
#include "esp_board_manager_includes.h"
#include "dev_display_lcd.h"

/* PPA scale of 1920x1080 takes longer than one frame. Extra buffers keep CSI fed. */
#define BUFFER_COUNT            4
#define STREAM_TASK_STACK       8192
#define STREAM_TASK_PRIORITY    5
#define SCAN_STEPS_MAX          512
#define AF_METER_WINDOW_DIV     5
#define AF_METER_WINDOW_MIN     8
#define LCD_RGB565_BPP          2
#define PPA_FRAG_MAX            16

static const char *TAG = "ae_meter";

static int s_fd = -1;
static uint32_t s_width;
static uint32_t s_height;
static uint32_t s_pixfmt;
static uint8_t *s_cam_buf[BUFFER_COUNT];
static volatile bool s_run;
static TaskHandle_t s_stream_task;
static TaskHandle_t s_main_task;
static int s_target_offset;
static bool s_win_on[ISP_AF_WINDOW_NUM];
static uint32_t s_win_l[ISP_AF_WINDOW_NUM];
static uint32_t s_win_t[ISP_AF_WINDOW_NUM];
static uint32_t s_win_w[ISP_AF_WINDOW_NUM];
static uint32_t s_win_h[ISP_AF_WINDOW_NUM];
static uint8_t s_af_w[ISP_AF_WINDOW_NUM] = { 1, 1, 1 };
static uint8_t s_ae_add = 8;
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
    ESP_LOGI(TAG, "  meter <x> <y> [aew]        default window around this center, all 3 AF");
    ESP_LOGI(TAG, "  win <n> <l> <t> <w> <h> [aew]  add AF window n, n is 0 1 or 2");
    ESP_LOGI(TAG, "  add win <n> <l> <t> <w> <h> [aew] same as win");
    ESP_LOGI(TAG, "  del win <n>                delete AF window n");
    ESP_LOGI(TAG, "  winw <n> <weight>          set AF window n weight");
    ESP_LOGI(TAG, "  aew <n>                    add n to AE weight of blocks in the AF window");
    ESP_LOGI(TAG, "  afw <w0> <w1> <w2>         set 3 AF window weights");
    ESP_LOGI(TAG, "  up [n]                   brighter by n luma, n defaults to 1");
    ESP_LOGI(TAG, "  down [n]                 darker by n luma, n defaults to 1");
    ESP_LOGI(TAG, "  ct <kelvin>              hold AWB at this color temperature");
    ESP_LOGI(TAG, "  scan ct <start> <end> <step> <ms>");
    ESP_LOGI(TAG, "  scan ae <start> <end> <step> <ms>");
    ESP_LOGI(TAG, "  scan af <step> <ms>         move current AF window left to right, top to bottom");
    ESP_LOGI(TAG, "  reset                    restore JSON AE weights, AF windows, AWB mode");
    ESP_LOGI(TAG, "  show                     print AE weights, AF windows, AWB ct");
    ESP_LOGI(TAG, "  quit                     stop the stream");
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

static void trim_line(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
        line[--len] = '\0';
    }
}

static void print_target_luma(void)
{
    float offset = 0.0f;
    float luma = -1.0f;
    esp_video_isp_pipeline_sensor_ae_t ae = {0};
    esp_err_t ret = esp_video_isp_pipeline_get_agc_meter_target_offset(&offset);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get target offset failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_video_isp_pipeline_get_sensor_ae(&ae);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get sensor ae failed: %s", esp_err_to_name(ret));
        return;
    }
    if (esp_video_isp_pipeline_get_env_float("env.luma.avg", &luma) != ESP_OK) {
        luma = -1.0f;
    }
    ESP_LOGI(TAG, "target_offset=%0.2f luma=%0.1f exposure=%" PRIu32 "us [%" PRIu32 ",%" PRIu32 "] gain=%0.2f [%0.2f,%0.2f]",
             offset, luma, ae.exposure_us, ae.min_exposure_us, ae.max_exposure_us,
             ae.gain, ae.min_gain, ae.max_gain);
}

static void print_meter(void)
{
    uint8_t weight[ISP_AE_REGIONS];
    esp_err_t ret;

    print_target_luma();
    ret = esp_video_isp_pipeline_get_agc_meter_luma_weight(weight);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get weight failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "frame=%" PRIu32 "x%" PRIu32 " ae_add=%u", s_width, s_height, s_ae_add);
    for (int y = 0; y < ISP_AE_BLOCK_Y_NUM; y++) {
        char row[ISP_AE_BLOCK_X_NUM * 4 + 1];
        size_t used = 0;

        row[0] = '\0';
        for (int x = 0; x < ISP_AE_BLOCK_X_NUM; x++) {
            int wrote = snprintf(row + used, sizeof(row) - used, "%4u",
                                 weight[x * ISP_AE_BLOCK_Y_NUM + y]);
            if (wrote < 0 || (size_t)wrote >= sizeof(row) - used) {
                break;
            }
            used += (size_t)wrote;
        }
        ESP_LOGI(TAG, "%s", row);
    }
}

static void print_af_window(void)
{
    isp_window_t windows[ISP_AF_WINDOW_NUM];
    uint8_t weight[ISP_AF_WINDOW_NUM];
    esp_err_t ret = esp_video_isp_pipeline_get_af_windows(windows);

    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGI(TAG, "af not in pipeline");
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get af windows failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_video_isp_pipeline_get_af_window_weight(weight);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get af weight failed: %s", esp_err_to_name(ret));
        return;
    }
    for (int i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        ESP_LOGI(TAG, "af[%d] (%" PRIu32 ",%" PRIu32 ")-(%" PRIu32 ",%" PRIu32 ") weight=%u",
                 i, windows[i].top_left.x, windows[i].top_left.y,
                 windows[i].btm_right.x, windows[i].btm_right.y, weight[i]);
    }
}

static void print_awb_ct(void)
{
    int32_t ct = 0;
    esp_err_t ret = esp_video_isp_pipeline_get_env_int32("ct", &ct);

    if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "awb ct not published");
        return;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get awb ct failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "awb ct=%" PRId32, ct);
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
    if (strcmp(action, "ct") != 0) {
        ESP_LOGI(TAG, "awb auto");
        return;
    }
    print_awb_ct();
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
    print_af_window();
}

static void fill_isp_window(isp_window_t *out, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    out->top_left.x = x;
    out->top_left.y = y;
    out->btm_right.x = x + w - 1;
    out->btm_right.y = y + h - 1;
}

static bool win_index_ok(unsigned n)
{
    if (n >= ISP_AF_WINDOW_NUM) {
        ESP_LOGE(TAG, "window n must be 0 to %d", ISP_AF_WINDOW_NUM - 1);
        return false;
    }
    return true;
}

static bool win_rect_ok(unsigned l, unsigned t, unsigned w, unsigned h)
{
    if (w == 0 || h == 0 || l >= s_width || t >= s_height ||
            w > s_width - l || h > s_height - t) {
        ESP_LOGE(TAG, "window %u %u %u %u is outside %" PRIu32 "x%" PRIu32,
                 l, t, w, h, s_width, s_height);
        return false;
    }
    return true;
}

static void clear_user_wins(void)
{
    memset(s_win_on, 0, sizeof(s_win_on));
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

static void apply_active_wins(void)
{
    uint8_t luma[ISP_AE_REGIONS];
    isp_window_t af[ISP_AF_WINDOW_NUM];
    uint8_t weight[ISP_AF_WINDOW_NUM];
    bool any = false;
    esp_err_t ret;
    int i;

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

    for (i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        if (s_win_on[i]) {
            any = true;
            fill_isp_window(&af[i], s_win_l[i], s_win_t[i], s_win_w[i], s_win_h[i]);
            weight[i] = s_af_w[i];
            ESP_LOGI(TAG, "AF[%d] left=%" PRIu32 " top=%" PRIu32 " w=%" PRIu32 " h=%" PRIu32 " weight=%u ae_add=%u",
                     i, s_win_l[i], s_win_t[i], s_win_w[i], s_win_h[i], s_af_w[i], s_ae_add);
        } else {
            fill_isp_window(&af[i], 0, 0, AF_METER_WINDOW_MIN, AF_METER_WINDOW_MIN);
            weight[i] = 0;
            ESP_LOGI(TAG, "AF[%d] off", i);
        }
    }

    if (any) {
        for (int bx = 0; bx < ISP_AE_BLOCK_X_NUM; bx++) {
            for (int by = 0; by < ISP_AE_BLOCK_Y_NUM; by++) {
                int idx = ae_index(bx, by);

                for (i = 0; i < ISP_AF_WINDOW_NUM; i++) {
                    if (s_win_on[i] &&
                            ae_block_overlaps((uint32_t)bx, (uint32_t)by,
                                              s_win_l[i], s_win_t[i], s_win_w[i], s_win_h[i])) {
                        luma[idx] = add_block_weight(luma[idx], s_ae_add);
                        break;
                    }
                }
            }
        }
        ret = esp_video_isp_pipeline_set_agc_meter_luma_weight(luma);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "set weight failed: %s", esp_err_to_name(ret));
            return;
        }
    } else {
        ESP_LOGI(TAG, "no AF window, AE weights restored");
    }

    print_meter();
    log_af_result(esp_video_isp_pipeline_set_af_windows(af), "win");
    log_af_result(esp_video_isp_pipeline_set_af_window_weight(weight), "weight");
}

static void set_one_win(unsigned n, unsigned l, unsigned t, unsigned w, unsigned h)
{
    if (!win_index_ok(n) || !win_rect_ok(l, t, w, h)) {
        return;
    }
    s_win_on[n] = true;
    s_win_l[n] = l;
    s_win_t[n] = t;
    s_win_w[n] = w;
    s_win_h[n] = h;
    if (s_af_w[n] == 0) {
        s_af_w[n] = 1;
    }
    apply_active_wins();
}

static void del_one_win(unsigned n)
{
    if (!win_index_ok(n)) {
        return;
    }
    if (!s_win_on[n]) {
        ESP_LOGI(TAG, "AF[%u] already off", n);
        return;
    }
    s_win_on[n] = false;
    s_af_w[n] = 0;
    apply_active_wins();
}

static void set_one_win_weight(unsigned n, unsigned weight)
{
    if (!win_index_ok(n)) {
        return;
    }
    if (weight > 255) {
        ESP_LOGE(TAG, "AF weight must be 0 to 255");
        return;
    }
    s_af_w[n] = (uint8_t)weight;
    apply_active_wins();
}

static void set_ae_add(unsigned add)
{
    if (add > 255) {
        ESP_LOGE(TAG, "aew must be 0 to 255");
        return;
    }
    s_ae_add = (uint8_t)add;
    ESP_LOGI(TAG, "ae_add=%u, overlapping AE = JSON + %u", s_ae_add, s_ae_add);
    apply_active_wins();
}

static void set_meter_default(unsigned x, unsigned y)
{
    uint32_t left;
    uint32_t top;
    uint32_t ww;
    uint32_t hh;
    int i;

    if (x >= s_width || y >= s_height) {
        ESP_LOGE(TAG, "meter %u %u is outside %" PRIu32 "x%" PRIu32, x, y, s_width, s_height);
        return;
    }
    default_meter_rect(x, y, &left, &top, &ww, &hh);
    for (i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        s_win_on[i] = true;
        s_win_l[i] = left;
        s_win_t[i] = top;
        s_win_w[i] = ww;
        s_win_h[i] = hh;
        s_af_w[i] = 1;
    }
    s_target_offset = 0;
    (void)esp_video_isp_pipeline_set_agc_meter_target_offset(0.0f);
    apply_active_wins();
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
    return true;
}

static void set_target_offset(int step)
{
    if (!apply_target_offset(step)) {
        return;
    }
    /* AE applies the new target on later frames. */
    vTaskDelay(pdMS_TO_TICKS(300));
    print_target_luma();
}

static bool scan_args_ok(int start, int end, int step)
{
    int64_t steps;

    if (step == 0) {
        ESP_LOGE(TAG, "scan step must not be 0");
        return false;
    }
    if ((step > 0 && start > end) || (step < 0 && start < end)) {
        ESP_LOGE(TAG, "scan step sign does not match start and end");
        return false;
    }
    steps = ((int64_t)end - start) / step + 1;
    if (steps < 1 || steps > SCAN_STEPS_MAX) {
        ESP_LOGE(TAG, "scan needs %" PRId64 " steps, max is %d", steps, SCAN_STEPS_MAX);
        return false;
    }
    return true;
}

static void scan_wait(unsigned time_ms)
{
    if (time_ms == 0) {
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(time_ms));
}

static void scan_ct(int start, int end, int step, unsigned time_ms)
{
    int value;

    if (!scan_args_ok(start, end, step)) {
        return;
    }
    if (start <= 0 || end <= 0) {
        ESP_LOGE(TAG, "scan ct start and end must be > 0");
        return;
    }

    ESP_LOGI(TAG, "scan ct start=%d end=%d step=%d time=%ums", start, end, step, time_ms);
    for (value = start; s_run; value += step) {
        if ((step > 0 && value > end) || (step < 0 && value < end)) {
            break;
        }
        ESP_LOGI(TAG, "scan ct=%d", value);
        log_awb_result(esp_video_isp_pipeline_set_awb_ct((uint32_t)value), "ct");
        scan_wait(time_ms);
        if ((step > 0 && value >= end) || (step < 0 && value <= end)) {
            break;
        }
    }
    ESP_LOGI(TAG, "scan ct done");
}

static void scan_ae(int start, int end, int step, unsigned time_ms)
{
    int value;

    if (!scan_args_ok(start, end, step)) {
        return;
    }

    ESP_LOGI(TAG, "scan ae start=%d end=%d step=%d time=%ums", start, end, step, time_ms);
    for (value = start; s_run; value += step) {
        if ((step > 0 && value > end) || (step < 0 && value < end)) {
            break;
        }
        ESP_LOGI(TAG, "scan ae offset=%d", value);
        if (!apply_target_offset(value)) {
            break;
        }
        scan_wait(time_ms);
        print_target_luma();
        if ((step > 0 && value >= end) || (step < 0 && value <= end)) {
            break;
        }
    }
    ESP_LOGI(TAG, "scan ae done");
}

static int scan_axis_count(uint32_t max_pos, uint32_t step)
{
    int n;

    if (max_pos == 0) {
        return 1;
    }
    n = (int)(max_pos / step) + 1;
    if ((max_pos % step) != 0) {
        n++;
    }
    return n;
}

static uint32_t scan_axis_pos(uint32_t index, uint32_t max_pos, uint32_t step)
{
    uint64_t pos = (uint64_t)index * step;

    if (pos >= max_pos) {
        return max_pos;
    }
    return (uint32_t)pos;
}

static void scan_af(unsigned step, unsigned time_ms)
{
    uint32_t origin_l = UINT32_MAX;
    uint32_t origin_t = UINT32_MAX;
    uint32_t group_r = 0;
    uint32_t group_b = 0;
    uint32_t rel_l[ISP_AF_WINDOW_NUM] = {0};
    uint32_t rel_t[ISP_AF_WINDOW_NUM] = {0};
    uint32_t max_left;
    uint32_t max_top;
    int nx;
    int ny;
    int ix;
    int iy;
    int i;
    bool any = false;

    if (step == 0) {
        ESP_LOGE(TAG, "scan af step must be > 0");
        return;
    }

    for (i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        if (!s_win_on[i]) {
            continue;
        }
        any = true;
        if (s_win_l[i] < origin_l) {
            origin_l = s_win_l[i];
        }
        if (s_win_t[i] < origin_t) {
            origin_t = s_win_t[i];
        }
        if (s_win_l[i] + s_win_w[i] > group_r) {
            group_r = s_win_l[i] + s_win_w[i];
        }
        if (s_win_t[i] + s_win_h[i] > group_b) {
            group_b = s_win_t[i] + s_win_h[i];
        }
    }
    if (!any) {
        ESP_LOGE(TAG, "scan af needs a window, use meter or win first");
        return;
    }

    for (i = 0; i < ISP_AF_WINDOW_NUM; i++) {
        if (s_win_on[i]) {
            rel_l[i] = s_win_l[i] - origin_l;
            rel_t[i] = s_win_t[i] - origin_t;
        }
    }

    max_left = s_width - (group_r - origin_l);
    max_top = s_height - (group_b - origin_t);
    nx = scan_axis_count(max_left, step);
    ny = scan_axis_count(max_top, step);
    if ((int64_t)nx * (int64_t)ny > SCAN_STEPS_MAX) {
        ESP_LOGE(TAG, "scan af needs %d steps, max is %d", nx * ny, SCAN_STEPS_MAX);
        return;
    }

    ESP_LOGI(TAG, "scan af step=%u time=%ums cells=%dx%d win=%" PRIu32 "x%" PRIu32,
             step, time_ms, nx, ny, group_r - origin_l, group_b - origin_t);

    for (iy = 0; iy < ny && s_run; iy++) {
        uint32_t top = scan_axis_pos((uint32_t)iy, max_top, step);

        for (ix = 0; ix < nx && s_run; ix++) {
            uint32_t left = scan_axis_pos((uint32_t)ix, max_left, step);

            for (i = 0; i < ISP_AF_WINDOW_NUM; i++) {
                if (!s_win_on[i]) {
                    continue;
                }
                s_win_l[i] = left + rel_l[i];
                s_win_t[i] = top + rel_t[i];
            }
            ESP_LOGI(TAG, "scan af left=%" PRIu32 " top=%" PRIu32, left, top);
            apply_active_wins();
            scan_wait(time_ms);
        }
    }
    ESP_LOGI(TAG, "scan af done");
}

static void handle_line(char *line)
{
    unsigned x;
    unsigned y;
    unsigned ct;
    unsigned w0;
    unsigned w1;
    unsigned w2;
    unsigned n;
    unsigned weight;
    unsigned time_ms;
    int delta;
    int start;
    int end;
    int step;
    esp_err_t ret;

    trim_line(line);
    if (line[0] == '\0') {
        return;
    }

    if (strcmp(line, "help") == 0) {
        print_help();
    } else if (sscanf(line, "del win %u", &n) == 1) {
        del_one_win(n);
    } else if (strncmp(line, "del", 3) == 0) {
        ESP_LOGE(TAG, "usage: del win <n>");
    } else if (sscanf(line, "winw %u %u", &n, &weight) == 2) {
        set_one_win_weight(n, weight);
    } else if (strncmp(line, "winw", 4) == 0) {
        ESP_LOGE(TAG, "usage: winw <n> <weight>");
    } else if (sscanf(line, "aew %u", &weight) == 1) {
        set_ae_add(weight);
    } else if (strncmp(line, "aew", 3) == 0) {
        ESP_LOGE(TAG, "usage: aew <n>");
    } else if (sscanf(line, "add win %u %u %u %u %u %u", &n, &x, &y, &w0, &w1, &weight) == 6) {
        set_ae_add(weight);
        set_one_win(n, x, y, w0, w1);
    } else if (sscanf(line, "add win %u %u %u %u %u", &n, &x, &y, &w0, &w1) == 5) {
        set_one_win(n, x, y, w0, w1);
    } else if (strncmp(line, "add", 3) == 0) {
        ESP_LOGE(TAG, "usage: add win <n> <l> <t> <w> <h> [aew]");
    } else if (sscanf(line, "win %u %u %u %u %u %u", &n, &x, &y, &w0, &w1, &weight) == 6) {
        set_ae_add(weight);
        set_one_win(n, x, y, w0, w1);
    } else if (sscanf(line, "win %u %u %u %u %u", &n, &x, &y, &w0, &w1) == 5) {
        set_one_win(n, x, y, w0, w1);
    } else if (strncmp(line, "win", 3) == 0) {
        ESP_LOGE(TAG, "usage: win <n> <l> <t> <w> <h> [aew]");
    } else if (sscanf(line, "meter %u %u %u", &x, &y, &weight) == 3) {
        set_ae_add(weight);
        set_meter_default(x, y);
    } else if (sscanf(line, "meter %u %u", &x, &y) == 2) {
        set_meter_default(x, y);
    } else if (strncmp(line, "meter", 5) == 0) {
        ESP_LOGE(TAG, "usage: meter <x> <y> [aew]");
    } else if (strcmp(line, "up") == 0 || sscanf(line, "up %d", &delta) == 1) {
        if (sscanf(line, "up %d", &delta) != 1) {
            delta = 1;
        }
        if (delta < 0) {
            ESP_LOGE(TAG, "up n must be >= 0");
            return;
        }
        if (s_target_offset >= (int)ESP_IPA_AGC_METER_TARGET_OFFSET_MAX) {
            ESP_LOGI(TAG, "target_offset already %d", (int)ESP_IPA_AGC_METER_TARGET_OFFSET_MAX);
            print_target_luma();
            return;
        }
        set_target_offset(s_target_offset + delta);
    } else if (strcmp(line, "down") == 0 || sscanf(line, "down %d", &delta) == 1) {
        if (sscanf(line, "down %d", &delta) != 1) {
            delta = 1;
        }
        if (delta < 0) {
            ESP_LOGE(TAG, "down n must be >= 0");
            return;
        }
        if (s_target_offset <= (int)ESP_IPA_AGC_METER_TARGET_OFFSET_MIN) {
            ESP_LOGI(TAG, "target_offset already %d", (int)ESP_IPA_AGC_METER_TARGET_OFFSET_MIN);
            print_target_luma();
            return;
        }
        set_target_offset(s_target_offset - delta);
    } else if (sscanf(line, "scan af %u %u", &n, &time_ms) == 2) {
        scan_af(n, time_ms);
    } else if (sscanf(line, "scan ct %d %d %d %u", &start, &end, &step, &time_ms) == 4) {
        scan_ct(start, end, step, time_ms);
    } else if (sscanf(line, "scan ae %d %d %d %u", &start, &end, &step, &time_ms) == 4) {
        scan_ae(start, end, step, time_ms);
    } else if (strncmp(line, "scan", 4) == 0) {
        ESP_LOGE(TAG, "usage: scan ct|ae <start> <end> <step> <ms> or scan af <step> <ms>");
    } else if (sscanf(line, "ct %u", &ct) == 1) {
        if (ct == 0) {
            ESP_LOGE(TAG, "ct 0 is rejected");
            return;
        }
        log_awb_result(esp_video_isp_pipeline_set_awb_ct(ct), "ct");
    } else if (sscanf(line, "afw %u %u %u", &w0, &w1, &w2) == 3) {
        uint8_t weight[ISP_AF_WINDOW_NUM];

        if (w0 > 255 || w1 > 255 || w2 > 255) {
            ESP_LOGE(TAG, "afw weights must be 0 to 255");
            return;
        }
        s_af_w[0] = (uint8_t)w0;
        s_af_w[1] = (uint8_t)w1;
        s_af_w[2] = (uint8_t)w2;
        weight[0] = s_af_w[0];
        weight[1] = s_af_w[1];
        weight[2] = s_af_w[2];
        log_af_result(esp_video_isp_pipeline_set_af_window_weight(weight), "weight");
    } else if (strcmp(line, "reset") == 0) {
        ret = esp_video_isp_pipeline_reset_agc_meter_weight();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "reset failed: %s", esp_err_to_name(ret));
            return;
        }
        s_target_offset = 0;
        (void)esp_video_isp_pipeline_set_agc_meter_target_offset(0.0f);
        clear_user_wins();
        s_af_w[0] = 1;
        s_af_w[1] = 1;
        s_af_w[2] = 1;
        s_ae_add = 8;
        ESP_LOGI(TAG, "restored original weights");
        print_target_luma();
        log_af_result(esp_video_isp_pipeline_reset_af(), "reset");
        log_awb_result(esp_video_isp_pipeline_set_awb_reset(), "reset");
    } else if (strcmp(line, "show") == 0) {
        print_meter();
        print_af_window();
        print_awb_ct();
    } else if (strcmp(line, "quit") == 0) {
        s_run = false;
    } else {
        ESP_LOGW(TAG, "unknown command: %s", line);
        print_help();
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

static void draw_frame_to_lcd(uint8_t *frame)
{
    float scale;
    uint32_t block_w;
    uint32_t block_h;
    uint32_t in_off_x;
    uint32_t in_off_y;
    uint32_t dst_w;
    uint32_t dst_h;
    uint32_t out_off_x;
    uint32_t out_off_y;
    void *back_fb;
    ppa_srm_oper_config_t srm;

    if (!s_ppa || !s_lcd_fb[0] || !s_lcd_fb[1] || !s_lcd_panel ||
            s_width == 0 || s_height == 0 || s_lcd_w == 0 || s_lcd_h == 0) {
        return;
    }

    /*
     * Cover the LCD: quantize scale up to 1/16, crop the camera frame from the
     * center, and fill. Letterbox left rainbow corners on 1920x1080 -> 1024x600.
     */
    scale = ppa_cover_scale(s_width, s_height, s_lcd_w, s_lcd_h);
    block_w = ((uint32_t)((float)s_lcd_w / scale)) & ~1u;
    block_h = ((uint32_t)((float)s_lcd_h / scale)) & ~1u;
    if (block_w > s_width) {
        block_w = s_width & ~1u;
    }
    if (block_h > s_height) {
        block_h = s_height & ~1u;
    }
    if (block_w == 0 || block_h == 0) {
        return;
    }
    in_off_x = ((s_width - block_w) / 2) & ~1u;
    in_off_y = ((s_height - block_h) / 2) & ~1u;
    dst_w = (uint32_t)((float)block_w * scale);
    dst_h = (uint32_t)((float)block_h * scale);
    if (dst_w > s_lcd_w) {
        dst_w = s_lcd_w;
    }
    if (dst_h > s_lcd_h) {
        dst_h = s_lcd_h;
    }
    out_off_x = (s_lcd_w - dst_w) / 2;
    out_off_y = (s_lcd_h - dst_h) / 2;

    back_fb = s_lcd_fb[s_ppa_fb_idx];
    memset(back_fb, 0, s_lcd_fb_bytes);
    (void)esp_cache_msync(back_fb, s_lcd_fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    memset(&srm, 0, sizeof(srm));
    srm.in.buffer = frame;
    srm.in.pic_w = s_width;
    srm.in.pic_h = s_height;
    srm.in.block_w = block_w;
    srm.in.block_h = block_h;
    srm.in.block_offset_x = in_off_x;
    srm.in.block_offset_y = in_off_y;
    srm.in.srm_cm = cam_ppa_color_mode(s_pixfmt);
    srm.in.yuv_range = PPA_COLOR_RANGE_LIMIT;
    srm.in.yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601;
    srm.out.buffer = back_fb;
    srm.out.buffer_size = s_lcd_fb_bytes;
    srm.out.pic_w = s_lcd_w;
    srm.out.pic_h = s_lcd_h;
    srm.out.block_offset_x = out_off_x;
    srm.out.block_offset_y = out_off_y;
    srm.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    srm.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
    srm.scale_x = scale;
    srm.scale_y = scale;
    srm.mode = PPA_TRANS_MODE_BLOCKING;

    if (ppa_do_scale_rotate_mirror(s_ppa, &srm) != ESP_OK) {
        ESP_LOGE(TAG, "PPA scale failed");
        return;
    }
    esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, 0, s_lcd_w, s_lcd_h, back_fb);
    s_ppa_fb_idx ^= 1;
}

static void stream_task(void *arg)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    (void)arg;
    bool logged = false;

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
    if (xTaskCreate(stream_task, "ae_stream", STREAM_TASK_STACK, NULL, STREAM_TASK_PRIORITY, &s_stream_task) != pdPASS) {
        ESP_LOGE(TAG, "failed to create stream task");
        ioctl(s_fd, VIDIOC_STREAMOFF, &type);
        goto fail;
    }

    ESP_LOGI(TAG, "[ 1 ] AE meter ready, camera %" PRIu32 "x%" PRIu32 " on LCD %" PRIu16 "x%" PRIu16,
             s_width, s_height, s_lcd_w, s_lcd_h);
    ESP_LOGI(TAG, "meter x y uses camera pixels, not LCD pixels");
    print_help();
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

static void console_flush(void)
{
    fflush(stdout);
    /* USB Serial/JTAG keeps bytes in the TX FIFO until newline or fsync. */
    fsync(fileno(stdout));
}

static void console_puts(const char *s)
{
    fputs(s, stdout);
    console_flush();
}

static int read_command_line(char *line, size_t size)
{
    size_t n = 0;

    console_puts("> ");

    while (s_run) {
        int c = fgetc(stdin);

        if (c == EOF) {
            /* USB Serial/JTAG reports EOF while the host is attaching. */
            if (!s_run) {
                return -1;
            }
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (c == '\r' || c == '\n') {
            console_puts("\n");
            line[n] = '\0';
            return 0;
        }
        if (c == '\b' || c == 127) {
            if (n > 0) {
                n--;
                console_puts("\b \b");
            }
            continue;
        }
        if (c < 32 || c > 126) {
            continue;
        }
        if (n + 1 >= size) {
            continue;
        }
        line[n++] = (char)c;
        fputc(c, stdout);
        console_flush();
    }

    return -1;
}

static void command_loop(void)
{
    char line[64];

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    while (s_run) {
        if (read_command_line(line, sizeof(line)) != 0) {
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
