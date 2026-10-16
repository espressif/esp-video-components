/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_ipa.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Schema version stored in esp_ipa_state_t::version.
 */
#define ESP_IPA_STATE_VERSION 1

/** @brief AWB reference records kept in esp_ipa_awb_ref_scan_state_t::rec. */
#define ESP_IPA_AWB_REF_MAX     64
/** @brief Scan points kept in each AF level. */
#define ESP_IPA_AF_POINT_MAX    255
/** @brief AF search levels stored in esp_ipa_af_state_t::focus. */
#define ESP_IPA_AF_LEVEL_NUM    2
/** @brief Environment-luma samples kept in esp_ipa_ian_state_t::luma. */
#define ESP_IPA_IAN_LUMA_MAX    255

/** @brief esp_ipa_state_t::modules bit: AGC state is present. */
#define ESP_IPA_STATE_MOD_AGC   (1u << 0)
/** @brief esp_ipa_state_t::modules bit: ATC state is present. */
#define ESP_IPA_STATE_MOD_ATC   (1u << 1)
/** @brief esp_ipa_state_t::modules bit: ADN state is present. */
#define ESP_IPA_STATE_MOD_ADN   (1u << 2)
/** @brief esp_ipa_state_t::modules bit: DPC state is present. */
#define ESP_IPA_STATE_MOD_DPC   (1u << 3)
/** @brief esp_ipa_state_t::modules bit: IR state is present. */
#define ESP_IPA_STATE_MOD_IR    (1u << 4)
/** @brief esp_ipa_state_t::modules bit: AEN state is present. */
#define ESP_IPA_STATE_MOD_AEN   (1u << 5)
/** @brief esp_ipa_state_t::modules bit: ACC state is present. */
#define ESP_IPA_STATE_MOD_ACC   (1u << 6)
/** @brief esp_ipa_state_t::modules bit: AF state is present. */
#define ESP_IPA_STATE_MOD_AF    (1u << 7)
/** @brief esp_ipa_state_t::modules bit: IAN state is present. */
#define ESP_IPA_STATE_MOD_IAN   (1u << 8)
/** @brief esp_ipa_state_t::modules bit: AWB state is present. */
#define ESP_IPA_STATE_MOD_AWB   (1u << 9)

/**
 * @brief AGC (`esp_ipa_agc`) computed state.
 *
 * Frame counters and the exposure-change clock are cleared by set.
 */
typedef struct {
    uint32_t applied_exposure;      /*!< Exposure last written to metadata */
    uint32_t max_exposure;          /*!< Current maximum exposure */
    float prev_gain;                /*!< Previous sensor gain */
    float prev_total_gain;          /*!< Previous total gain */
    uint8_t enable;                 /*!< AGC enable */
} esp_ipa_agc_state_t;

/**
 * @brief ATC (`esp_ipa_atc`) computed state.
 */
typedef struct {
    int32_t ae_value_last;          /*!< Previous auto-exposure value */
} esp_ipa_atc_state_t;

/**
 * @brief ADN Bayer-filter computed state.
 *
 * An index of -1 means the Bayer-filter table has not been applied.
 */
typedef struct {
    int32_t index;                  /*!< Bayer filter table index */
} esp_ipa_adn_bf_state_t;

/**
 * @brief ADN demosaic computed state.
 *
 * An index of -1 means the demosaic table has not been applied.
 */
typedef struct {
    int32_t index;                  /*!< Demosaic table index */
} esp_ipa_adn_dm_state_t;

/**
 * @brief ADN (`esp_ipa_adn`) computed state.
 */
typedef struct {
    esp_ipa_adn_bf_state_t bf;
    esp_ipa_adn_dm_state_t dm;
} esp_ipa_adn_state_t;

/**
 * @brief DPC (`esp_ipa_dpc`) computed state.
 *
 * index is a node of the tuning table that produced this state. -1 means no
 * node has been applied. tune_crc is that tune: model, debounce_gain, table
 * length, and each node's gain and DPC parameters. config_version is the
 * schema version, so another table under the same version is a different tune.
 * The same length with different gains or DPC parameters is not compatible,
 * and set rejects it. A non-negative index must also fall inside the current
 * table, because model 0 debounce reads table[index] on the next frame.
 */
typedef struct {
    int32_t index;                  /*!< DPC gain-table index; -1 if none */
    uint32_t tune_crc;              /*!< Identity of the DPC tuning table */
    uint8_t prev_valid;             /*!< prev holds a computed DPC parameter */
    esp_ipa_dpc_t prev;             /*!< Last DPC parameter written to metadata */
} esp_ipa_dpc_state_t;

/**
 * @brief IR (`esp_ipa_ir`) computed state.
 *
 * The switch-delay clock is cleared by set. state is the IR switch state machine.
 */
typedef struct {
    uint32_t prev_saturation;       /*!< Saturation saved before IR was enabled */
    uint8_t sat_saved;              /*!< prev_saturation is valid */
    uint8_t state;                  /*!< IR switch state */
    uint8_t ir_enable;              /*!< IR cut / illuminator enable */
} esp_ipa_ir_state_t;

/**
 * @brief AEN luma computed state.
 */
typedef struct {
    float prev;                     /*!< Previous environment luma */
} esp_ipa_aen_luma_state_t;

/**
 * @brief AEN backlight computed state.
 */
typedef struct {
    float low;                      /*!< Backlight filter low side */
    float high;                     /*!< Backlight filter high side */
    uint8_t active;                 /*!< Backlight scene is active */
    uint8_t filter_inited;          /*!< Backlight filter has a sample */
    uint16_t count;                 /*!< Backlight detect counter */
} esp_ipa_aen_backlight_state_t;

/**
 * @brief AEN gamma computed state.
 *
 * index is the greatest node with luma <= the current value, clamped to the
 * table in use (the backlight table when backlight is active). -1 means no
 * node has been applied. Model 0 uses that node. Model 1 interpolates toward
 * the next node between knots, so the applied curve is stored in curve.
 * curve is meaningful when valid is non-zero.
 */
typedef struct {
    int32_t index;                  /*!< Gamma table node; -1 if none */
    uint8_t valid;                  /*!< curve holds the previous gamma */
    esp_ipa_gamma_t curve;          /*!< Previous gamma */
} esp_ipa_aen_gamma_state_t;

/**
 * @brief AEN sharpen computed state.
 *
 * An index of -1 means the sharpen table has not been applied.
 */
typedef struct {
    int32_t index;                  /*!< Sharpen table index */
} esp_ipa_aen_sharpen_state_t;

/**
 * @brief AEN contrast computed state.
 *
 * An index of -1 means the contrast table has not been applied.
 */
typedef struct {
    int32_t index;                  /*!< Contrast table index */
} esp_ipa_aen_con_state_t;

/**
 * @brief AEN (`esp_ipa_aen`) computed state.
 */
typedef struct {
    esp_ipa_aen_luma_state_t luma;
    esp_ipa_aen_backlight_state_t backlight;
    esp_ipa_aen_gamma_state_t gamma;
    esp_ipa_aen_sharpen_state_t sharpen;
    esp_ipa_aen_con_state_t con;
} esp_ipa_aen_state_t;

/**
 * @brief ACC saturation computed state.
 *
 * An index of -1 means the saturation table has not been applied.
 */
typedef struct {
    int32_t index;                  /*!< Saturation table index */
} esp_ipa_acc_sat_state_t;

/**
 * @brief ACC CCM computed state.
 *
 * An index of -1 means the CCM table has not been applied. index may also be -2.
 */
typedef struct {
    uint32_t prev_ct;               /*!< Previous color temperature */
    int32_t index;                  /*!< CCM table index */
    uint8_t base_valid;             /*!< base holds a CCM */
    uint8_t out_valid;              /*!< out is the CCM written to metadata */
    esp_ipa_ccm_t base;             /*!< CCM before gain blend */
    esp_ipa_ccm_t out;              /*!< CCM written to metadata */
} esp_ipa_acc_ccm_state_t;

/**
 * @brief ACC LSC computed state.
 *
 * An index of -1 means that table has not been applied.
 */
typedef struct {
    int32_t gain_index;             /*!< LSC gain-table index */
    int32_t ct_index;               /*!< LSC color-temperature index */
    uint8_t disabled;               /*!< LSC was disabled */
} esp_ipa_acc_lsc_state_t;

/**
 * @brief ACC BLC computed state.
 *
 * An index of -1 means the BLC table has not been applied.
 */
typedef struct {
    int32_t index;                  /*!< BLC table index */
} esp_ipa_acc_blc_state_t;

/**
 * @brief ACC (`esp_ipa_acc`) computed state.
 */
typedef struct {
    esp_ipa_acc_sat_state_t sat;
    esp_ipa_acc_ccm_state_t ccm;
    esp_ipa_acc_lsc_state_t lsc;
    esp_ipa_acc_blc_state_t blc;
} esp_ipa_acc_state_t;

/**
 * @brief One AF scan point.
 */
typedef struct {
    uint64_t definition;            /*!< Focus definition */
    uint64_t luminance;             /*!< Focus luminance */
} esp_ipa_af_point_state_t;

/**
 * @brief One AF focus level, without the scan-point array.
 *
 * points[0 .. points_count) of the enclosing level are valid.
 */
typedef struct {
    uint32_t step_pos;              /*!< Position step of this level */
    uint32_t cur_pos;               /*!< Current focus position */
    uint32_t base_pos;              /*!< Base position of this scan */
    uint8_t prev_point;             /*!< Previous scan point */
    uint8_t cur_point;              /*!< Current scan point */
    uint8_t max_point;              /*!< Point with the best definition */
    uint8_t points_count;           /*!< Number of valid points */
} esp_ipa_af_focus_hdr_t;

/**
 * @brief One AF focus level. points[0 .. hdr.points_count) are valid.
 */
typedef struct {
    esp_ipa_af_focus_hdr_t hdr;
    esp_ipa_af_point_state_t points[ESP_IPA_AF_POINT_MAX];
} esp_ipa_af_focus_state_t;

/**
 * @brief AF search computed state.
 *
 * target_time is a duration. The focus-change clock is cleared by set.
 */
typedef struct {
    uint32_t state;                 /*!< AF search state */
    uint32_t target_time;           /*!< Focus move duration */
    uint64_t cur_definition;        /*!< Current definition */
    uint64_t cur_luminance;         /*!< Current luminance */
    uint32_t applied_focus_pos;     /*!< Focus position last written to metadata */
    uint8_t is_ready;               /*!< AF search is ready */
    uint8_t focus_valid;            /*!< applied_focus_pos was written */
} esp_ipa_af_search_state_t;

/**
 * @brief AF (`esp_ipa_af`) computed state.
 *
 * focus[0] is level 1 and focus[1] is level 2.
 */
typedef struct {
    esp_ipa_af_search_state_t search;
    esp_ipa_af_focus_state_t focus[ESP_IPA_AF_LEVEL_NUM];
} esp_ipa_af_state_t;

/** @brief IAN global flag: color temperature `ct` is stored. */
#define ESP_IPA_IAN_STATE_CT           (1u << 0)
/** @brief IAN global flag: `rg` is stored. */
#define ESP_IPA_IAN_STATE_RG           (1u << 1)
/** @brief IAN global flag: `bg` is stored. */
#define ESP_IPA_IAN_STATE_BG           (1u << 2)
/** @brief IAN global flag: `hist.luma.avg` is stored. */
#define ESP_IPA_IAN_STATE_HIST_LUMA    (1u << 3)
/** @brief IAN global flag: `hist.value.ds` is stored. */
#define ESP_IPA_IAN_STATE_HIST_DS      (1u << 4)
/** @brief IAN global flag: `backlight` is stored. */
#define ESP_IPA_IAN_STATE_BACKLIGHT    (1u << 5)
/** @brief IAN global flag: `ae.luma.avg` is stored. */
#define ESP_IPA_IAN_STATE_AE_LUMA      (1u << 6)
/** @brief IAN global flag: `ae.value.ds` is stored. */
#define ESP_IPA_IAN_STATE_AE_DS        (1u << 7)
/** @brief IAN global flag: `env.luma.avg` is stored. */
#define ESP_IPA_IAN_STATE_ENV_LUMA     (1u << 8)

/**
 * @brief IAN color-temperature smoothing state.
 *
 * rg_min/rg_max/bg_min/bg_max come from configuration and are not stored.
 */
typedef struct {
    int32_t index;                  /*!< Statistics window index */
    double prev_rg;                 /*!< Previous R/G */
    double prev_bg;                 /*!< Previous B/G */
    int32_t prev_ct;                /*!< Previous color temperature */
    uint8_t smooth_inited;          /*!< CT smoothing has a sample */
} esp_ipa_ian_ct_state_t;

/**
 * @brief IAN environment-luma ring header.
 *
 * sample[0 .. size) of the enclosing luma state is valid.
 */
typedef struct {
    uint8_t index;                  /*!< Next write index in the luma ring */
    uint32_t flags;                 /*!< Luma environment state flags */
} esp_ipa_ian_luma_hdr_t;

/**
 * @brief IAN environment-luma ring.
 */
typedef struct {
    uint8_t size;                   /*!< Valid length of sample */
    esp_ipa_ian_luma_hdr_t hdr;
    float sample[ESP_IPA_IAN_LUMA_MAX]; /*!< Environment-luma ring */
} esp_ipa_ian_luma_state_t;

/**
 * @brief IAN values published to the pipeline variable map.
 *
 * flags selects which of ct, rg, bg, hist_luma, hist_ds, backlight, ae_luma,
 * ae_ds and env_luma were published.
 */
typedef struct {
    uint32_t flags;                 /*!< ESP_IPA_IAN_STATE_* bits */
    int32_t ct;                     /*!< Published color temperature */
    int32_t backlight;              /*!< Published backlight flag */
    float rg;                       /*!< Published R/G */
    float bg;                       /*!< Published B/G */
    float hist_luma;                /*!< Published histogram luma average */
    float hist_ds;                  /*!< Published histogram value deviation */
    float ae_luma;                  /*!< Published AE luma average */
    float ae_ds;                    /*!< Published AE value deviation */
    float env_luma;                 /*!< Published environment luma */
} esp_ipa_ian_global_state_t;

/**
 * @brief IAN (`esp_ipa_ian`) computed state.
 */
typedef struct {
    esp_ipa_ian_ct_state_t ct;
    esp_ipa_ian_luma_state_t luma;
    esp_ipa_ian_global_state_t global;
} esp_ipa_ian_state_t;

/**
 * @brief One AWB model-2 reference statistic.
 */
typedef struct {
    uint32_t counter;               /*!< IIR-filtered hit count */
    double rg;                      /*!< IIR-filtered R/G */
    double bg;                      /*!< IIR-filtered B/G */
} esp_ipa_awb_ref_state_t;

/**
 * @brief AWB gain computed state.
 */
typedef struct {
    float prev_red;                 /*!< Previous red gain */
    float prev_blue;                /*!< Previous blue gain */
    float prev_rg;                  /*!< Previous R/G */
    float prev_bg;                  /*!< Previous B/G */
    uint8_t valid;                  /*!< Red/blue gain fields are computed */
} esp_ipa_awb_gain_state_t;

/**
 * @brief AWB statistics-window computed state.
 *
 * These are the computed AWB window. The configured limits are rebuilt by init
 * and are not these fields.
 */
typedef struct {
    uint8_t valid;                  /*!< window is computed */
    esp_ipa_awb_range_t window;     /*!< Computed rg/bg/green window */
} esp_ipa_awb_range_state_t;

/**
 * @brief AWB model-2 reference scan.
 *
 * rec[0 .. n) is valid.
 */
typedef struct {
    uint32_t idx;                   /*!< Current reference scan index */
    uint32_t count;                 /*!< Samples taken at idx */
    uint8_t ready;                  /*!< Every reference point has been scanned */
    uint32_t n;                     /*!< Number of valid reference records */
    esp_ipa_awb_ref_state_t rec[ESP_IPA_AWB_REF_MAX];
} esp_ipa_awb_ref_scan_state_t;

/**
 * @brief AWB model-3 fixed color temperature.
 */
typedef struct {
    uint8_t valid;                  /*!< ct selects a model 3 preset */
    uint32_t ct;                    /*!< Model 3 preset color temperature */
} esp_ipa_awb_fixed_state_t;

/**
 * @brief AWB zone switch computed state.
 *
 * An index of -1 means no zone has been accepted. counter[] entries are all valid.
 */
typedef struct {
    int32_t current;                /*!< Accepted AWB zone */
    uint32_t counter[ESP_IPA_AWB_ZONE_MAX];
} esp_ipa_awb_zone_state_t;

/**
 * @brief AWB color temperature computed state.
 */
typedef struct {
    float prev;                     /*!< Previous color temperature */
    uint8_t valid;                  /*!< ct is published */
    int32_t ct;                     /*!< Published color temperature */
} esp_ipa_awb_ct_state_t;

/**
 * @brief AWB (`esp_ipa_awb`) computed state.
 *
 * The startup-hold clock is cleared by set and is not stored.
 */
typedef struct {
    uint32_t model;                 /*!< esp_ipa_awb_model_t */
    float prev_green_luma;          /*!< Previous green luma */
    esp_ipa_awb_gain_state_t gain;
    esp_ipa_awb_range_state_t range;
    esp_ipa_awb_ct_state_t ct;
    esp_ipa_awb_ref_scan_state_t ref;
    esp_ipa_awb_fixed_state_t fixed;
    esp_ipa_awb_zone_state_t zone;
} esp_ipa_awb_state_t;

/**
 * @brief Computed state of the whole IPA pipeline.
 *
 * The application stores this struct and passes the same object to set.
 * modules is the OR of ESP_IPA_STATE_MOD_* for the modules that filled a member.
 * Members whose bit is clear are not used. Configuration is not included;
 * create/init rebuilds it, so the config at get and set must be identical.
 * version is ESP_IPA_STATE_VERSION.
 * crc32 covers the whole struct with that field treated as zero.
 */
typedef struct esp_ipa_state {
    uint32_t version;               /*!< ESP_IPA_STATE_VERSION */
    uint32_t crc32;                 /*!< CRC-32 of this struct; zero while computing */
    uint32_t config_version;        /*!< esp_ipa_config_t::version */
    uint32_t width;                 /*!< Sensor width at get */
    uint32_t height;                /*!< Sensor height at get */
    uint32_t modules;               /*!< ESP_IPA_STATE_MOD_* bits */
    esp_ipa_agc_state_t agc;
    esp_ipa_atc_state_t atc;
    esp_ipa_adn_state_t adn;
    esp_ipa_dpc_state_t dpc;
    esp_ipa_ir_state_t ir;
    esp_ipa_aen_state_t aen;
    esp_ipa_acc_state_t acc;
    esp_ipa_af_state_t af;
    esp_ipa_ian_state_t ian;
    esp_ipa_awb_state_t awb;
} esp_ipa_state_t;

/**
 * @brief Read computed IPA state for the application to store.
 *
 * Call after esp_ipa_pipeline_init(). @p state receives the computed results and
 * the intermediate values later process() calls depend on.
 *
 * The pipeline configuration must stay the same until esp_ipa_pipeline_set_state().
 * Do not call esp_ipa_pipeline_set_config() to change parameters in between.
 * State does not store the configuration. A different config makes the saved
 * results inconsistent with the modules, and set_state then writes wrong ISP
 * parameters.
 *
 * @param handle  Pipeline handle
 * @param sensor  Current sensor; width and height are stored in @p state
 * @param state   State filled on success
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on NULL handle, sensor, or state
 *      - ESP_ERR_INVALID_STATE if init has not completed
 */
esp_err_t esp_ipa_pipeline_get_state(esp_ipa_pipeline_handle_t handle, const esp_ipa_sensor_t *sensor,
                                     esp_ipa_state_t *state);

/**
 * @brief Restore computed IPA state and write the stored ISP parameters.
 *
 * Call order is create, init, then set. init rebuilds configuration, including
 * config-derived global variables. set copies @p state into the modules and
 * writes the stored ISP parameters into @p metadata. It does not read ISP
 * statistics and does not recompute metadata from them.
 *
 * The configuration must be the same as it was at esp_ipa_pipeline_get_state().
 * Do not call esp_ipa_pipeline_set_config() to change parameters between get
 * and set. State does not include the configuration; modules apply the stored
 * results against the live config. A different config makes the restored state
 * inconsistent and writes wrong ISP parameters. Checks here only cover config
 * version, loaded modules, and each module tuning table.
 *
 * Pass the metadata filled by init. set overwrites the fields that were computed.
 * Frame counters and elapsed-time clocks are cleared, so delay checks start again.
 *
 * @param handle    Pipeline handle
 * @param state     State previously returned by get
 * @param sensor    Current sensor; width and height must match the sensor passed to get
 * @param metadata  ISP parameters. Computed fields are written here
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on NULL arguments
 *      - ESP_ERR_INVALID_STATE if init has not completed, @p state does not
 *        match this pipeline (config version, modules, or a module tuning
 *        table), or @p sensor width and height do not match @p state
 *      - ESP_ERR_INVALID_VERSION if @p state->version does not match
 *      - ESP_ERR_INVALID_CRC if @p state->crc32 does not match
 */
esp_err_t esp_ipa_pipeline_set_state(esp_ipa_pipeline_handle_t handle, const esp_ipa_state_t *state,
                                     const esp_ipa_sensor_t *sensor, esp_ipa_metadata_t *metadata);

#ifdef __cplusplus
}
#endif
