/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include "hal/isp_types.h"

#ifdef __cplusplus
extern "C" {
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

/**
 * @brief IPA meta data flags.
 */
#define IPA_METADATA_FLAGS_CT       (1 << 0)    /*!< Meta data has color temperature */
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

struct esp_ipa;

/**
 * @brief Camera sensor information.
 */
typedef struct esp_ipa_sensor {
    uint32_t width;                         /*!< Current output picture width */
    uint32_t height;                        /*!< Current output picture height */

    uint32_t max_exposure;                  /*!< Maximum exposure, unit is micro second */
    uint32_t min_exposure;                  /*!< Minmium exposure, unit is micro second */
    uint32_t cur_exposure;                  /*!< Current exposure, unit is micro second us */
    uint32_t step_exposure;                 /*!< Step exposure, unit is micro second us; if step_exposure == 0, step size is uneven */

    float max_gain;                         /*!< Maximum gain */
    float min_gain;                         /*!< Minimum gain */
    float cur_gain;                         /*!< Current gain */
    float step_gain;                        /*!< Step gain; if step_gain == 0.0, step size is uneven */
} esp_ipa_sensor_t;

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
 * @brief IPA meta data, these data are calculated by IPA and configured to ISP hardware
 */
typedef struct esp_ipa_metadata {
    uint32_t flags;                         /*!< IPA meta data flags */

    uint32_t color_temp;                    /*!< Color temperature in Kelvin */

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
} esp_ipa_metadata_t;

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
    void *priv;                             /*!< IPA private data */
} esp_ipa_t;

/**
 * @brief Image process algorithm pipeline object
 */
typedef struct esp_ipa_pipeline {
    uint8_t ipa_nums;                       /*!< IPA numbers */
    esp_ipa_t **ipa_array;                  /*!< IPA array */
} esp_ipa_pipeline_t;
