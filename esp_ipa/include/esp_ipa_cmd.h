/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include "esp_ipa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_IPA_IOCPARM_MASK        (0x0fffUL)
#define ESP_IPA_IOC_VOID            (0x20000000UL)
#define ESP_IPA_IOC_OUT             (0x40000000UL)
#define ESP_IPA_IOC_IN              (0x80000000UL)
#define ESP_IPA_IOC_INOUT           (ESP_IPA_IOC_IN | ESP_IPA_IOC_OUT)

#define ESP_IPA_IO(x, y)            ((long)(ESP_IPA_IOC_VOID | ((x) << 8) | (y)))
#define ESP_IPA_IOR(x, y, t)        ((long)(ESP_IPA_IOC_OUT  | ((sizeof(t) & ESP_IPA_IOCPARM_MASK) << 16) | ((x) << 8) | (y)))
#define ESP_IPA_IOW(x, y, t)        ((long)(ESP_IPA_IOC_IN   | ((sizeof(t) & ESP_IPA_IOCPARM_MASK) << 16) | ((x) << 8) | (y)))

#define ESP_IPA_IOC_SIZE(cmd)       (((cmd) >> 16) & ESP_IPA_IOCPARM_MASK)
#define ESP_IPA_IOC_TYPE(cmd)       (((cmd) >> 8) & 0xff)

#define ESP_IPA_IOC_TYPE_AGC        1
#define ESP_IPA_IOC_TYPE_AF         2
#define ESP_IPA_IOC_TYPE_AWB        3
#define ESP_IPA_IOC_TYPE_ACC        4
#define ESP_IPA_IOC_TYPE_IAN        5

#define ESP_IPA_AGC_S_STATUS        ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AGC,  0, int)
#define ESP_IPA_AGC_G_STATUS        ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AGC,  1, int)
#define ESP_IPA_AGC_S_MAX_EXPOSURE  ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AGC,  2, uint32_t) /* The application should limit the maximum exposure to the step of the exposure */
#define ESP_IPA_AGC_G_MAX_EXPOSURE  ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AGC,  3, uint32_t)
#define ESP_IPA_AGC_S_MIN_EXPOSURE  ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AGC,  4, uint32_t) /* The application should limit the minimum exposure to the step of the exposure */
#define ESP_IPA_AGC_G_MIN_EXPOSURE  ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AGC,  5, uint32_t)
#define ESP_IPA_AGC_S_METER_LUMA_WEIGHT         ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AGC,  6, esp_ipa_agc_meter_luma_weight_t)
#define ESP_IPA_AGC_G_METER_LUMA_WEIGHT         ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AGC,  7, esp_ipa_agc_meter_luma_weight_t)
#define ESP_IPA_AGC_S_RESET_METER_WEIGHT        ESP_IPA_IO(ESP_IPA_IOC_TYPE_AGC,   8) /* Restore the JSON AE weight table */
#define ESP_IPA_AGC_S_METER_TARGET_OFFSET       ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AGC,  9, float) /* Same luma shift added to JSON target / target_low / target_high */
#define ESP_IPA_AGC_G_METER_TARGET_OFFSET       ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AGC, 10, float)

#define ESP_IPA_AF_S_RESET          ESP_IPA_IO(ESP_IPA_IOC_TYPE_AF,  1) /* Restore JSON windows and weights, then restart the scan */
#define ESP_IPA_AF_G_WINDOWS        ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AF, 2, esp_ipa_af_windows_t)
#define ESP_IPA_AF_S_WINDOWS        ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AF, 3, esp_ipa_af_windows_t) /* Replace runtime windows and restart the scan */
#define ESP_IPA_AF_S_WINDOW_WEIGHT         ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AF, 4, esp_ipa_af_window_weight_t) /* Replace runtime window weights */
#define ESP_IPA_AF_G_WINDOW_WEIGHT         ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AF, 5, esp_ipa_af_window_weight_t)

#define ESP_IPA_AWB_S_CT            ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AWB, 0, uint32_t) /* Fixed Kelvin: preset, mired interpolation, or clamp */
#define ESP_IPA_AWB_S_RESET         ESP_IPA_IO(ESP_IPA_IOC_TYPE_AWB,  1) /* Restore JSON AWB mode: model 2 resumes statistics; model 3 restores default_ct */
#define ESP_IPA_AWB_S_STATUS        ESP_IPA_IOW(ESP_IPA_IOC_TYPE_AWB,  2, int) /* 0 stops AWB; non-zero runs it and clears the applied statistics window */
#define ESP_IPA_AWB_G_STATUS        ESP_IPA_IOR(ESP_IPA_IOC_TYPE_AWB,  3, int)

#define ESP_IPA_ACC_S_SAT_STATUS    ESP_IPA_IOW(ESP_IPA_IOC_TYPE_ACC,  0, int) /* 0 stops ACC saturation; non-zero runs it and sets the saturation index to -1 */
#define ESP_IPA_ACC_G_SAT_STATUS    ESP_IPA_IOR(ESP_IPA_IOC_TYPE_ACC,  1, int)
#define ESP_IPA_ACC_S_CCM_STATUS    ESP_IPA_IOW(ESP_IPA_IOC_TYPE_ACC,  2, int) /* 0 stops ACC CCM; non-zero runs it and sets the CCM index to -1 */
#define ESP_IPA_ACC_G_CCM_STATUS    ESP_IPA_IOR(ESP_IPA_IOC_TYPE_ACC,  3, int)

#define ESP_IPA_IAN_S_CT_STATUS     ESP_IPA_IOW(ESP_IPA_IOC_TYPE_IAN,  0, int) /* 0 stops IAN color-temperature estimation; non-zero runs it and sets the CT index to -1 */
#define ESP_IPA_IAN_G_CT_STATUS     ESP_IPA_IOR(ESP_IPA_IOC_TYPE_IAN,  1, int)

#define ESP_IPA_IOC_TYPE_WL         6

#define ESP_IPA_WL_S_CONFIG         ESP_IPA_IOW(ESP_IPA_IOC_TYPE_WL,   0, esp_ipa_wl_config_t)
#define ESP_IPA_WL_G_CONFIG         ESP_IPA_IOR(ESP_IPA_IOC_TYPE_WL,   1, esp_ipa_wl_config_t)

#ifdef __cplusplus
}
#endif
