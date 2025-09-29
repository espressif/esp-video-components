/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include <sys/param.h>
#include <stdbool.h>
#include "hal/isp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)               (sizeof(a) / sizeof((a)[0]))
#endif

#define ISP_AWB_REGIONS             1   /*!< Auto white balance regions */
#define ISP_AE_REGIONS              (ISP_AE_BLOCK_X_NUM * ISP_AE_BLOCK_Y_NUM)   /*!< Auto exposure regions */

/**
 * @brief ISP statistics flags.
 */
#define IPA_STATS_FLAGS_AWB         (1 << 0)    /*!< ISP statistics has auto white balance */
#define IPA_STATS_FLAGS_AE          (1 << 1)    /*!< ISP statistics has auto exposure */
#define IPA_STATS_FLAGS_HIST        (1 << 2)    /*!< ISP statistics has histogram */
#define IPA_STATS_FLAGS_SHARPEN     (1 << 3)    /*!< ISP statistics has sharpen */
#define IPA_STATS_FLAGS_AF          (1 << 4)    /*!< ISP statistics has auto focus */

/**
 * @brief IPA meta data flags.
 */
#define IPA_METADATA_FLAGS_AWB      (1 << 0)    /*!< Meta data has AWB */
#define IPA_METADATA_FLAGS_RG       (1 << 1)    /*!< Meta data has red gain */
#define IPA_METADATA_FLAGS_BG       (1 << 2)    /*!< Meta data has blue gain */
#define IPA_METADATA_FLAGS_ET       (1 << 3)    /*!< Meta data has exposure */
#define IPA_METADATA_FLAGS_GN       (1 << 4)    /*!< Meta data has pixel gain */
#define IPA_METADATA_FLAGS_BF       (1 << 5)    /*!< Meta data has bayer filter */
#define IPA_METADATA_FLAGS_SH       (1 << 6)    /*!< Meta data has sharpen */
#define IPA_METADATA_FLAGS_GAMMA    (1 << 7)    /*!< Meta data has GAMMA */
#define IPA_METADATA_FLAGS_CCM      (1 << 8)    /*!< Meta data has CCM */
#define IPA_METADATA_FLAGS_BR       (1 << 9)    /*!< Meta data has brightness */
#define IPA_METADATA_FLAGS_CN       (1 << 10)   /*!< Meta data has contrast */
#define IPA_METADATA_FLAGS_ST       (1 << 11)   /*!< Meta data has saturation */
#define IPA_METADATA_FLAGS_HUE      (1 << 12)   /*!< Meta data has hue */
#define IPA_METADATA_FLAGS_DM       (1 << 13)   /*!< Meta data has demosaic */
#define IPA_METADATA_FLAGS_LSC      (1 << 14)   /*!< Meta data has LSC */
#define IPA_METADATA_FLAGS_AETL     (1 << 15)   /*!< Meta data has sensor AE target level */
#define IPA_METADATA_FLAGS_SR       (1 << 16)   /*!< Meta data has statistics region */
#define IPA_METADATA_FLAGS_AF       (1 << 17)   /*!< Meta data has AF */
#define IPA_METADATA_FLAGS_FP       (1 << 18)   /*!< Meta data has focus position */

/**
 * @brief Auto gain control metering mode
 */
typedef enum esp_ipa_agc_meter_mode {
    ESP_IPA_AGC_METER_AVERAGE = 0,              /*!< Use default metering weight table */
    ESP_IPA_AGC_METER_HIGHLIGHT_PRIOR,          /*!< Use default metering weight table + high light priority config */
    ESP_IPA_AGC_METER_LOWLIGHT_PRIOR,           /*!< Use default metering weight table + low light priority config */
    ESP_IPA_AGC_METER_LIGHT_THRESHOLD           /*!< Use default metering weight table + light threshold config */
} esp_ipa_agc_meter_mode_t;

/**
 * @brief The source data model type of analyzing image color temperature.
 */
typedef enum esp_ipa_ian_ct_model {
    ESP_IPA_IAN_CT_MODEL_0 = 0,                 /*!< The source data model type 0 */
    ESP_IPA_IAN_CT_MODEL_1,                     /*!< The source data model type 1 */
    ESP_IPA_IAN_CT_MODEL_2,                     /*!< The source data model type 2 */
} esp_ipa_ian_ct_model_t;

/**
 * @brief CCM data generation model
 */
typedef enum esp_ipa_acc_ccm_model {
    ESP_IPA_ACC_CCM_MODEL_0 = 0,                /*!< CCM data generation model type 0 */
    ESP_IPA_ACC_CCM_MODEL_1,                    /*!< CCM data generation model type 1 */
} esp_ipa_acc_ccm_model_t;

/**
 * @brief Auto gain control anti-flicker mode
 */
typedef enum esp_ipa_agc_anti_flicker_mode {
    ESP_IPA_AGC_ANTI_FLICKER_FULL = 0,
    ESP_IPA_AGC_ANTI_FLICKER_PART,
    ESP_IPA_AGC_ANTI_FLICKER_NONE,
} esp_ipa_agc_anti_flicker_mode_t;

/**
 * @brief AF data model type
 */
typedef enum esp_ipa_af_model {
    ESP_IPA_AF_MODEL_0 = 0,                     /*!< AF data process model type 0 */
} esp_ipa_af_model_t;

/**
 * @brief ATC data model type
 */
typedef enum esp_ipa_atc_model {
    ESP_IPA_ATC_MODEL_0 = 0,                     /*!< ATC data process model type 0 */
} esp_ipa_atc_model_t;

/**
 * @brief GAMMA model type
 */
typedef enum esp_ipa_aen_gamma_model {
    ESP_IPA_AEN_GAMMA_MODEL_0 = 0,             /*!< GAMMA model type 0 */
    ESP_IPA_AEN_GAMMA_MODEL_1,                 /*!< GAMMA model type 1 */
} esp_ipa_aen_gamma_model_t;

struct esp_ipa;
struct esp_ipa_pipeline;

/**
 * @brief Sensor focus information.
 */
typedef struct esp_ipa_sensor_focus {
    uint32_t max_pos;                           /*!< Maximum focus position */
    uint32_t min_pos;                           /*!< Minimum focus position */
    uint32_t cur_pos;                           /*!< Current focus position */
    uint32_t step_pos;                          /*!< Step focus position */

    int64_t start_time;                         /*!< Absolute time of focus starts moving */
    uint16_t period_in_us;                      /*!< Period in microseconds per code */
    uint16_t codes_per_step;                    /*!< Codes number per step */
} esp_ipa_sensor_focus_t;

/**
 * @brief Camera sensor information.
 */
typedef struct esp_ipa_sensor {
    uint32_t width;                         /*!< Current output picture width */
    uint32_t height;                        /*!< Current output picture height */

    uint32_t max_exposure;                  /*!< Maximum exposure, unit is micro second */
    uint32_t min_exposure;                  /*!< Minmium exposure, unit is micro second */
    uint32_t cur_exposure;                  /*!< Current exposure, unit is micro second */
    uint32_t step_exposure;                 /*!< Step exposure, unit is micro second; if step_exposure == 0, step size is uneven */

    float max_gain;                         /*!< Maximum gain */
    float min_gain;                         /*!< Minimum gain */
    float cur_gain;                         /*!< Current gain */
    float step_gain;                        /*!< Step gain; if step_gain == 0.0, step size is uneven */

    uint32_t max_ae_target_level;           /*!< Maximum Sensor AE target level */
    uint32_t min_ae_target_level;           /*!< Minimum Sensor AE target level */
    uint32_t cur_ae_target_level;           /*!< Current Sensor AE target level */
    uint32_t step_ae_target_level;          /*!< Step Sensor AE target level */

    esp_ipa_sensor_focus_t *focus_info;     /*!< Sensor focus information, if sensor does not support setting focus, set to NULL */
} esp_ipa_sensor_t;

/**
 * @brief The selected image region.
 */
typedef struct esp_ipa_region {
    uint32_t left;                          /*!< The left coordinate of the selected image region */
    uint32_t top;                           /*!< The top coordinate of the selected image region */
    uint32_t width;                         /*!< The width of the selected image region */
    uint32_t height;                        /*!< The height of the selected image region */
} esp_ipa_region_t;

/**
 * @brief ISP auto white balance statistics.
 */
typedef struct esp_ipa_stats_awb {
    uint32_t counted;                       /*!< white patch number that counted by AWB in the window */
    uint32_t sum_r;                         /*!< The sum of R channel of these white patches */
    uint32_t sum_g;                         /*!< The sum of G channel of these white patches */
    uint32_t sum_b;                         /*!< The sum of B channel of these white patches */
} esp_ipa_stats_awb_t;

/**
 * @brief ISP auto exposure statistics.
 */
typedef struct esp_ipa_stats_ae {
    uint32_t luminance;                     /*!< Luminance, it refers how luminant an image is */
} esp_ipa_stats_ae_t;

/**
 * @brief ISP histogram statistics.
 */
typedef struct esp_ipa_stats_hist {
    uint32_t value;                         /*!< Histogram value */
} esp_ipa_stats_hist_t;

/**
 * @brief ISP sharpen statistics.
 */
typedef struct esp_ipa_stats_sharpen {
    uint8_t value;                          /*!< Sharpen high frequency pixel maximum value */
} esp_ipa_stats_sharpen_t;

/**
 * @brief ISP auto focus statistics.
 */
typedef struct esp_ipa_stats_af {
    uint32_t definition;                    /*!< Definition value of auto focus statistics region */
    uint32_t luminance;                     /*!< Luma value of auto focus statistics region */
} esp_ipa_stats_af_t;

/**
 * @brief ISP statistics for IPA.
 */
typedef struct esp_ipa_stats {
    uint64_t seq;                           /*!< ISP statistics sequence */

    uint32_t flags;                         /*!< ISP statistics flags */

    /*!< ISP auto white balance statistics */

    esp_ipa_stats_awb_t awb_stats[ISP_AWB_REGIONS];

    /*!< ISP auto exposure statistics */

    esp_ipa_stats_ae_t ae_stats[ISP_AE_REGIONS];

    /*!< ISP histogram statistics */

    esp_ipa_stats_hist_t hist_stats[ISP_HIST_SEGMENT_NUMS];

    /*!< ISP sharpen statistics */

    esp_ipa_stats_sharpen_t sharpen_stats;

    /*!< ISP auto focus statistics */

    esp_ipa_stats_af_t af_stats[ISP_AF_WINDOW_NUM];
} esp_ipa_stats_t;


/**
 * @brief ISP BF(bayer filter) meta data.
 */
typedef struct esp_ipa_denoising_bf {
    uint8_t level;      /*!< BF denoising level*/
    uint8_t matrix[ISP_BF_TEMPLATE_X_NUMS][ISP_BF_TEMPLATE_Y_NUMS];     /*!< BF filter matrix */
} esp_ipa_denoising_bf_t;

/**
 * @brief ISP sharpen meta data.
 */
typedef struct esp_ipa_sharpen {
    uint8_t h_thresh;   /*!< Sharpen high threshold of high frequency component */
    uint8_t l_thresh;   /*!< Sharpen low threshold of high frequency component */
    float h_coeff;      /*!< Sharpen coefficient of high threshold */
    float m_coeff;      /*!< Sharpen coefficient of middle threshold(value between "l_thresh" and "h_thresh" ) */
    uint8_t matrix[ISP_SHARPEN_TEMPLATE_X_NUMS][ISP_SHARPEN_TEMPLATE_Y_NUMS];   /*!< Sharpen filter matrix */
} esp_ipa_sharpen_t;

/**
 * @brief ISP GAMMA meta data.
 */
typedef struct esp_ipa_gamma {
    uint8_t x[ISP_GAMMA_CURVE_POINTS_NUM];  /*!< GAMMA point X coordinate */
    uint8_t y[ISP_GAMMA_CURVE_POINTS_NUM];  /*!< GAMMA point Y coordinate */
} esp_ipa_gamma_t;

/**
 * @brief ISP CCM meta data.
 */
typedef struct esp_ipa_ccm {
    float matrix[ISP_CCM_DIMENSION][ISP_CCM_DIMENSION]; /*!< Color correction matrix */
} esp_ipa_ccm_t;

/**
 * @brief Demosaic meta data.
 */
typedef struct esp_ipa_demosaic {
    float gradient_ratio;                   /*!< Demosaic gradient ratio */
} esp_ipa_demosaic_t;

/**
 * @brief LSC(lens shadow correction) meta data.
 */
typedef struct esp_ipa_lsc {
    const isp_lsc_gain_t *gain_r;               /*!< LSC gain array for R channel */
    const isp_lsc_gain_t *gain_gr;              /*!< LSC gain array for GR channel */
    const isp_lsc_gain_t *gain_gb;              /*!< LSC gain array for GB channel */
    const isp_lsc_gain_t *gain_b;               /*!< LSC gain array for B channel */
    uint32_t lsc_gain_array_size;               /*!< LSC gain array size */
} esp_ipa_lsc_t;

/**
 * @brief AWB(Auto white balance) statistics range meta data.
 */
typedef struct esp_ipa_awb_range {
    uint8_t green_max;                          /*!< Maximum green value */
    uint8_t green_min;                          /*!< Minimum green value */

    float rg_max;                               /*!< Maximum red/green ratio */
    float rg_min;                               /*!< Minimum red/green ratio */

    float bg_max;                               /*!< Maximum blue/green ratio */
    float bg_min;                               /*!< Minimum blue/green ratio */
} esp_ipa_awb_range_t;

/**
 * @brief AF(auto focus) meta data.
 */
typedef struct esp_ipa_af {
    isp_window_t windows[ISP_AF_WINDOW_NUM];    /*!< The sampling windows coordinate configuration of AF */

    uint32_t edge_thresh;                       /*!< AF edge threshold, definition higher than this value will be counted as a valid pixel for calculating AF result */
} esp_ipa_af_t;

/**
 * @brief IPA meta data, these data are calculated by IPA and configured to ISP hardware
 */
typedef struct esp_ipa_metadata {
    uint32_t flags;                         /*!< IPA meta data flags */

    float red_gain;                         /*!< Red gain */
    float blue_gain;                        /*!< Blue gain */

    uint32_t exposure;                      /*!< Exposure, unit is micro second */

    float gain;                             /*!< Pixel gain */

    esp_ipa_denoising_bf_t bf;              /*!< Bayer filter parameters */
    esp_ipa_demosaic_t demosaic;            /*!< Demosaic parameters */

    esp_ipa_sharpen_t sharpen;              /*!< Sharpen parameters */

    esp_ipa_gamma_t gamma;                  /*!< GAMMA parameters */

    esp_ipa_ccm_t ccm;                      /*!< CCM parameters */

    uint32_t brightness;                    /*!< Color brightness */
    uint32_t contrast;                      /*!< Color contrast */
    uint32_t saturation;                    /*!< Color saturation */
    uint32_t hue;                           /*!< Color hue */

    esp_ipa_lsc_t lsc;                      /*!< LSC parameters */

    esp_ipa_awb_range_t awb;                /*!< AWB statistics range parameters */

    uint32_t ae_target_level;               /*!< Sensor AE target level */

    esp_ipa_region_t stats_region;          /*!< Statistics region */

    esp_ipa_af_t af;                        /*!< AF parameters */

    uint32_t focus_pos;                     /*!< Focus position parameter */
} esp_ipa_metadata_t;

/**
 * @brief Auto gain control metering light luma priority config
 */
typedef struct esp_ipa_agc_meter_light_prior_config {
    uint8_t luma_low_threshold;                 /*!< Light luma low threshold */
    uint8_t luma_high_threshold;                /*!< Light luma high threshold */
    uint8_t weight_offset;                      /*!< Light luma weight offset */
    int8_t luma_offset;                         /*!< Light luma offset */
} esp_ipa_agc_meter_light_prior_config_t;

/**
 * @brief Auto gain control metering light threshold unit
 */
typedef struct esp_ipa_agc_meter_light_threshold {
    uint32_t luma_threshold;                    /*!< Light luma threshold */
    uint32_t weight_offset;                     /*!< Light luma weight offset */
} esp_ipa_agc_meter_light_threshold_t;

/**
 * @brief Auto gain control metering light threshold config
 */
typedef struct esp_ipa_agc_meter_light_threshold_config {
    const esp_ipa_agc_meter_light_threshold_t *table;   /*!< metering light threshold table */
    uint32_t table_size;                        /*!< metering light threshold table size */
} esp_ipa_agc_meter_light_threshold_config_t;

/**
 * @brief Color saturation and gain mapping data for auto color correction algorithm
 */
typedef struct esp_ipa_acc_sat {
    uint32_t color_temp;                        /*!< Color temperature */
    uint32_t saturation;                        /*!< Color saturation */
} esp_ipa_acc_sat_t;

/**
 * @brief Color temperature and color correction matrix mapping data for auto color correction algorithm
 */
typedef struct esp_ipa_acc_ccm_unit {
    uint32_t color_temp;                        /*!< Color temperature */
    esp_ipa_ccm_t ccm;                          /*!< ISP color correction matrix parameter */
} esp_ipa_acc_ccm_unit_t;

/**
 * @brief Color temperature and lens shadow correction parameters mapping data for auto color correction algorithm
 */
typedef struct esp_ipa_acc_lsc_lut {
    uint32_t color_temp;                        /* Color temperature */
    esp_ipa_lsc_t lsc;                          /* Lens shadow correction parameters */
} esp_ipa_acc_lsc_lut_t;

/**
 * @brief Color temperature and lens shadow correction parameters mapping data in specific resolution for auto color correction algorithm
 */
typedef struct esp_ipa_acc_lsc {
    uint32_t width;                             /* Picture width */
    uint32_t height;                            /* Picture height */
    const esp_ipa_acc_lsc_lut_t *lsc_gain_table;    /* Color temperature and lens shadow correction parameters mapping table */
    uint32_t lsc_gain_table_size;               /* Color temperature and lens shadow correction parameters mapping table size */
} esp_ipa_acc_lsc_t;

/**
 * @brief Bayer filter parameter and gain mapping data for auto denoising algorithm
 */
typedef struct esp_ipa_adn_bf {
    uint32_t gain;                              /*!< Camera sensor gain, unit is 0.001 */
    esp_ipa_denoising_bf_t bf;                  /*!< ISP bayer filter parameter  */
} esp_ipa_adn_bf_t;

/**
 * @brief Demosaic parameter and gain mapping data for auto denoising algorithm
 */
typedef struct esp_ipa_adn_dm {
    uint32_t gain;                              /*!< Camera sensor gain, unit is 0.001 */
    esp_ipa_demosaic_t dm;                      /*!< ISP demosaic parameter  */
} esp_ipa_adn_dm_t;

/**
 * @brief GAMMA value look-up table unit
 */
typedef struct esp_ipa_aen_gamma_unit {
    float luma;

    union {
        float gamma_param;                      /*!< Parameter to generate gamma mapping table */
        esp_ipa_gamma_t gamma;                  /*!< Gamma mapping table */
    };
} esp_ipa_aen_gamma_unit_t;

/**
 * @brief GAMMA parameter for auto enhancement algorithm
 */
typedef struct esp_ipa_aen_gamma_config {
    esp_ipa_aen_gamma_model_t model;            /*!< GAMMA model */

    bool use_gamma_param;                       /*!< true: use variable "gamma_param" to generate gamma mapping table; false: using variable "gamma" */

    const char *luma_env;                       /*!< Luma environment variable name */

    float luma_min_step;                        /*!< Luma minmium step value */

    const esp_ipa_aen_gamma_unit_t *gamma_table;    /*!< GAMMA value look-up table */
    uint32_t gamma_table_size;                  /*!< GAMMA value look-up table size */
} esp_ipa_aen_gamma_config_t;

/**
 * @brief Sharpen parameter and gain mapping data for auto enhancement algorithm
 */
typedef struct esp_ipa_aen_sharpen {
    uint32_t gain;                              /*!< Camera sensor gain, unit is 0.001 */
    esp_ipa_sharpen_t sharpen;                  /*!< ISP sharpen parameter */
} esp_ipa_aen_sharpen_t;

/**
 * @brief Color contrast and gain mapping data for auto enhancement algorithm
 */
typedef struct esp_ipa_aen_con {
    uint32_t gain;                              /*!< Camera sensor gain, unit is 0.001 */
    uint32_t contrast;                          /*!< Color contrast parameter */
} esp_ipa_aen_con_t;

/**
 * @brief Color temperature basic parameters.
 */
typedef struct esp_ipa_ian_ct_basic_param {
    float a0;                                   /*!< Basic parameter a0 */
    float a1;                                   /*!< Basic parameter a1 */
} esp_ipa_ian_ct_basic_param_t;

/**
 * @brief Color temperature analyze configuration
 */
typedef struct esp_ipa_ian_ct_config {
    esp_ipa_ian_ct_model_t model;               /*!< The source data module type */
    float m_a0, m_a1, m_a2;                     /*!< The source data module parameters */

    float f_n0;                                 /*!< Color temperature filter parameter n0 */
    const esp_ipa_ian_ct_basic_param_t *bp;     /*!< Color temperature basic parameters table */
    uint16_t bp_nums;                           /*!< Color temperature basic parameters table size */
    uint16_t min_step;                          /*!< Color temperature minimum step value */

    float g_a0, g_a1;                           /*!< Color temperature parameters g_a1 */
    const float *g_a2;                          /*!< Color temperature parameters g_a2 table */
    const uint16_t g_a2_nums;                   /*!< Color temperature parameters g_a2 table size */
} esp_ipa_ian_ct_config_t;

/**
 * @brief Light luma and scene histogram analyze configuration
 */
typedef struct esp_ipa_ian_luma_hist_config {
    const uint8_t mean[ISP_HIST_SEGMENT_NUMS];  /*!< Histogram segment mean table */

    uint8_t low_index_start;                    /*!< Low start index in histogram */
    uint8_t low_index_end;                      /*!< Low end index in histogram */

    uint8_t high_index_start;                   /*!< High start index in histogram */
    uint8_t high_index_end;                     /*!< High end index in histogram */

    float back_light_radio_threshold;           /*!< Back light radio threshold */
} esp_ipa_ian_luma_hist_config_t;

/**
 * @brief Light luma and scene AE analyze configuration
 */
typedef struct esp_ipa_ian_luma_ae_config {
    const uint8_t weight[ISP_AE_REGIONS];       /*!< AE luma weight table */
} esp_ipa_ian_luma_ae_config_t;

/**
 * @brief Environment light luma and scene analyze configuration
 */
typedef struct esp_ipa_ian_env_luma_config {
    float k;                                    /*!< Environment light luma and scene analyze configuration */

    const float *speed_param;                   /*!< Environment light luma speed parameter array */
    uint8_t speed_param_size;                   /*!< Environment light luma speed parameter array size */

    const uint8_t weight[ISP_AE_REGIONS];       /*!< Environment light luma weight table */
} esp_ipa_ian_luma_env_config;

/**
 * @brief Light luma and scene analyze configuration
 */
typedef struct esp_ipa_ian_luma_config {
    const esp_ipa_ian_luma_hist_config_t *hist; /*!< Light luma and scene histogram analyze configuration */
    const esp_ipa_ian_luma_ae_config_t *ae;     /*!< Light luma and scene AE analyze configuration */
    const esp_ipa_ian_luma_env_config *env;   /*!< Environment light luma and scene analyze configuration */
} esp_ipa_ian_luma_config_t;

/**
 * @brief Image analyze configuration
 */
typedef struct esp_ipa_ian_config {
    const esp_ipa_ian_ct_config_t *ct;          /*!< Color temperature analyze configuration */
    const esp_ipa_ian_luma_config_t *luma;      /*!< Light luma and scene analyze configuration */

    bool enable_log;                            /*!< Enable image analyze algorithm log */
} esp_ipa_ian_config_t;

/**
 * @brief AWB algorithm model.
 */
typedef enum esp_ipa_awb_model {
    ESP_IPA_AWB_MODEL_0 = 0,                    /*!< Use gray world*/
    ESP_IPA_AWB_MODEL_1                         /*!< Use color temperature indexing */
} esp_ipa_awb_model_t;

/**
 * @brief Auto white balance algorithm configuration
 */
typedef struct esp_ipa_awb_config {
    esp_ipa_awb_model_t model;                  /*!< AWB algorithm model */

    /* Common configuration parameters */

    float min_red_gain_step;                    /*!< Minimum red channel gain step, less value will not be set into hardware */
    float min_blue_gain_step;                   /*!< Minimum blue channel gain step, less value will not be set into hardware */

    bool enable_log;                            /*!< Enable auto white balance algorithm log */

    /* Configuration parameters used in model_0 */

    uint32_t min_counted;                       /*!< Minimum white point number, less value will not trigger process */
    esp_ipa_awb_range_t range;                  /*!< AWB statistics range data */

    /* Configuration parameters used in model_1 */

    const char *green_luma_env;                 /*!< Green luma environment variable name */
    float green_luma_init;                      /*!< Green luma initialization value */
    float green_luma_step_ratio;                /*!< Green luma step ratio, when changing ratio is larger then this value, update AWB statistics range */
} esp_ipa_awb_config_t;

/**
 * @brief Auto color correct matrix configuration
 */
typedef struct esp_ipa_acc_ccm_config {
    esp_ipa_acc_ccm_model_t model;              /*!< CCM data generation model */

    const char *luma_env;
    float luma_low_threshold;                   /*!< Luma low threshold */
    esp_ipa_ccm_t luma_low_ccm;                 /*!< ISP color correction matrix parameter */

    const esp_ipa_acc_ccm_unit_t *ccm_table;    /*!< Color correction matrix and color temperature mapping table */
    uint32_t ccm_table_size;                    /*!< Color correction matrix and color temperature mapping table size */

    /* Configuration parameters used in model_1 */

    uint32_t min_ct_step;                       /*!< Minmium color temperature step */
} esp_ipa_acc_ccm_config_t;

/**
 * @brief Auto color correct algorithm configuration
 */
typedef struct esp_ipa_acc_config {
    const esp_ipa_acc_sat_t *sat_table;         /*!< Saturation and gain mapping table */
    uint32_t sat_table_size;                    /*!< Saturation and gain mapping table size */

    const esp_ipa_acc_ccm_config_t *ccm;        /*!< Auto color correct matrix configuration */

    const esp_ipa_acc_lsc_t *lsc_table;         /* Lens shadow correction gain array, color temperature and resolution mapping table */
    uint32_t lsc_table_size;                    /* Lens shadow correction gain array, color temperature and resolution mapping table size */

    bool enable_log;                            /*!< Enable auto color correct algorithm log */
} esp_ipa_acc_config_t;

/**
 * @brief Auto denoising algorithm configuration
 */
typedef struct esp_ipa_adn_config {
    const esp_ipa_adn_bf_t *bf_table;           /*!< Bayer filter parameter and gain mapping table */
    uint32_t bf_table_size;                     /*!< Bayer filter parameter and gain mapping table size */

    const esp_ipa_adn_dm_t *dm_table;           /*!< Demosaic parameter and gain mapping table */
    uint32_t dm_table_size;                     /*!< Demosaic parameter and gain mapping table size */

    bool enable_log;                            /*!< Enable auto denoising algorithm log */
} esp_ipa_adn_config_t;

/**
 * @brief Auto enhancement algorithm configuration
 */
typedef struct esp_ipa_aen_config {
    const esp_ipa_aen_gamma_config_t *gamma;    /*!< GAMMA configuration */

    const  esp_ipa_aen_sharpen_t *sharpen_table;    /*!< Sharpen parameter and gain mapping table */
    uint16_t sharpen_table_size;                /*!< Sharpen parameter and gain mapping table size */

    const esp_ipa_aen_con_t *con_table;         /*!< Color contrast and gain mapping table */
    uint16_t con_table_size;                    /*!< Color contrast and gain mapping table size */
    bool enable_log;                            /*!< Enable auto enhancement algorithm log */
} esp_ipa_aen_config_t;

/**
 * @brief Auto gain control algorithm configuration
 */
typedef struct esp_ipa_agc_config {
    uint8_t exposure_frame_delay;               /*!< Exposure effective delay frames */
    uint8_t gain_frame_delay;                   /*!< Gain effective delay frames */

    uint16_t exposure_adjust_delay;             /*!< Exposure adjustment delay time in milliseconds */
    float min_gain_step;                        /*!< Minmium gain step */
    float inc_gain_ratio;                       /*!< Luma gain increasing ratio */
    float dec_gain_ratio;                       /*!< Luma gain decreasing ratio */

    esp_ipa_agc_anti_flicker_mode_t anti_flicker_mode;  /*!< Anti-flicker mode */
    uint8_t ac_freq;                            /*!< Alternating current frequency */

    uint8_t luma_low;                           /*!< Low luma */
    uint8_t luma_high;                          /*!< High luma */
    uint8_t luma_target;                        /*!< Target luma */

    uint8_t luma_low_threshold;                 /*!< Low luma threshold */
    uint8_t luma_low_regions;                   /*!< Low luma region numbers */
    uint8_t luma_high_threshold;                /*!< High luma threshold */
    uint8_t luma_high_regions;                  /*!< Low luma region numbers */

    uint8_t luma_weight_table[ISP_AE_REGIONS];  /*!< Luma weight */

    esp_ipa_agc_meter_mode_t meter_mode;        /*!< Metering mode */
    esp_ipa_agc_meter_light_prior_config_t low_light_prior_config;  /*!< Low light prior config */
    esp_ipa_agc_meter_light_prior_config_t high_light_prior_config; /*!< High light prior config */
    esp_ipa_agc_meter_light_threshold_config_t light_threshold_config;  /*!< Light threshold config */

    bool enable_log;                            /*!< Enable auto gain control algorithm log */
} esp_ipa_agc_config_t;

/**
 * @brief ATC luma lookup table
 */
typedef struct esp_ipa_atc_luma_lut {
    float luma;                                 /*!< ATC speed parameter array */
    int32_t ae_value;                           /*!< ATC speed parameter array number */
} esp_ipa_atc_luma_lut_t;

/**
 * @brief Auto sensor AE target level control algorithm configuration
 */
typedef struct esp_ipa_atc_config {
    esp_ipa_atc_model_t model;                  /*!< ATC data model type */

    uint32_t init_value;                        /*!< Sensor AE target level initialization value */
    uint8_t delay_frames;                       /*!< Sensor AE target level delay frames */

    const char *luma_env;                       /*!< Sensor AE target level luma environment variable name */

    /* Configuration parameters used in model_0 */

    uint32_t min_ae_value_step;                 /*!< ATC AE target level minimum step */
    const esp_ipa_atc_luma_lut_t *luma_lut;     /*!< ATC luma lookup table */
    uint32_t luma_lut_size;                     /*!< ATC luma lookup table size */

    bool enable_log;                            /*!< Enable Auto sensor AE target level control algorithm log */
} esp_ipa_atc_config_t;

/**
 * @brief IPA extended configuration
 */
typedef struct esp_ipa_ext_config {
    uint32_t hue;                               /*!< Color hue */
    uint32_t brightness;                        /*!< Color brightness */

    esp_ipa_region_t stats_region;              /*!< ISP statistics region */
} esp_ipa_ext_config_t;

/**
 * @brief AF(auto focus) algorithm configuration.
 */
typedef struct esp_ipa_af_config {
    esp_ipa_af_model_t model;                   /*!< AF data model type */

    isp_window_t windows[ISP_AF_WINDOW_NUM];    /*!< The sampling windows coordinate configuration of AF */

    uint8_t weight_table[ISP_AF_WINDOW_NUM];    /*!< The weight table of AF windows */

    uint32_t edge_thresh;                       /*!< AF edge threshold, definition higher than this value will be counted as a valid pixel for calculating AF result */

    uint32_t max_pos;                           /*!< Maximum focus position */
    uint32_t min_pos;                           /*!< Minimum focus position */

    uint8_t l1_scan_points_num;                 /*!< Number of 1st level scanning points */
    uint8_t l2_scan_points_num;                 /*!< Number of 2nd level scanning points */

    /* Model 0 parameters */

    float definition_high_threshold_ratio;      /*!< High definition threshold ratio, when the current definition increase rate is higher than this value, the autofocus will restart */
    float definition_low_threshold_ratio;       /*!< Low definition threshold ratio, when the current definition decrease rate is higher than this value, the autofocus will restart */
    float luminance_high_threshold_ratio;       /*!< High luminance threshold ratio, when the current luminance increase rate is higher than this value, the autofocus will restart */
    float luminance_low_threshold_ratio;        /*!< Low luminance threshold ratio, when the current luminance decrease rate is higher than this value, the autofocus will restart */
    uint32_t max_change_time;                   /*!< Maximum definition change time in microseconds, when the definition change time is longer than this value, the autofocus will restart */

    bool enable_log;                            /*!< Enable AF algorithm log */
} esp_ipa_af_config_t;

/**
 * @brief IPA initialize configuration
 */
typedef struct esp_ipa_config {
    const char **names;                         /*!< Image process algorithm name array */
    uint8_t nums;                               /*!< Image process algorithm name array */

    bool enable_log;                            /*!< Enable Image process algorithm core function log */

    uint32_t version;                           /*!< Image process algorithm configuration parameters version */
    const esp_ipa_ian_config_t *ian;            /*!< Image analyze configuration */
    const esp_ipa_agc_config_t *agc;            /*!< Auto gain control algorithm configuration */
    const esp_ipa_awb_config_t *awb;            /*!< Auto white balance algorithm configuration */
    const esp_ipa_acc_config_t *acc;            /*!< Auto color correct algorithm configuration */
    const esp_ipa_adn_config_t *adn;            /*!< Auto denoising algorithm configuration */
    const esp_ipa_aen_config_t *aen;            /*!< Auto enhancement algorithm configuration */
    const esp_ipa_af_config_t  *af;             /*!< Auto focus algorithm configuration */
    const esp_ipa_atc_config_t *atc;            /*!< Auto sensor AE target level control algorithm configuration */

    const esp_ipa_ext_config_t *ext;            /*!< Image extended configuration */
} esp_ipa_config_t;

/**
 * @brief Image process algorithm operations
 */
typedef struct esp_ipa_ops {
    /**
     * @brief Initialize IPA, this function generally contains the following steps:
     *
     *        1. load and initialize IPA parameter
     *        2. initialize IPA state and global variables
     *        3. generate metadata based on IPA and sensor, the metadata will be
     *           written into ISP or sensor
     */
    esp_err_t (*init)(struct esp_ipa *ipa, const esp_ipa_sensor_t *sensor, esp_ipa_metadata_t *metadata);

    /**
     * @brief Run IPA calculation function once to generate metadata, the metadata will
     *        be written into ISP or sensor. If the IPA just only has an initialization
     *        function and doesn't need a dynamical calculation function, the "process"
     *        can be set to be NULL.
     */
    void (*process)(struct esp_ipa *ipa, const esp_ipa_stats_t *stats, const esp_ipa_sensor_t *sensor, esp_ipa_metadata_t *metadata);

    /**
     * @brief Free all resource allocated by IAP detect function.
     */
    void (*destroy)(struct esp_ipa *ipa);
} esp_ipa_ops_t;

/**
 * @brief Image process algorithm object
 */
typedef struct esp_ipa {
    const char *name;                       /*!< IPA name */
    const esp_ipa_ops_t *ops;               /*!< IPA operations */
    struct esp_ipa_pipeline *pipeline;      /*!< IPA pipeline */
    void *priv;                             /*!< IPA private data */
} esp_ipa_t;

/**
 * @brief Image process algorithm pipeline object
 */
typedef struct esp_ipa_pipeline {
    const esp_ipa_config_t *config;         /*!< IPA numbers */
    esp_ipa_t **ipa_array;                  /*!< IPA array */
    void *map;                              /*!< IPA global map object pointer */
} esp_ipa_pipeline_t;

#ifdef __cplusplus
}
#endif
