## 2.2.0~1

- Fix `Failed to resolve component 'esp_ipa'` at configure time for component-registry consumers (namespaced `espressif__esp_ipa`): route the prebuilt's component-lib dependency through the `${COMPONENT_LIB}` target instead of the component name.

## 2.2.0

- Added information about the document system to README.md.
- Fixed linking issue on the Arduino platform.
    - Added `libesp_ipa_newlib.a` to support builds using the Newlib C library.
- Added a static linking method that prevents unused functions from being included in the final firmware, reducing its size.
- ACC CCM: optional `gain_lut` blends the CT-selected CCM toward identity by sensor gain: `output = (1-s)*I + s*ccm`, where `s` is linearly interpolated from the LUT (`enable` + `table` with `gain` / `strength`)
- AGC: fix exposure and gain not being updated when average luma is zero
- IAN: fix `env.luma.avg` stuck at zero in very dark scenes

## 2.1.0

- AGC: optional JSON `agc.gain.max` maps to `esp_ipa_agc_config_t::max_gain`; when greater than zero, clamps the gain chosen by AGC after applying the sensor `min_gain` / `max_gain` limits; zero keeps previous behaviour (no extra cap)
- AWB **model_2 (zone classifier)**: classify each AWB by chromaticity against a **zone** table; neutral CT bands vote for illuminant selection, while **GREEN** / **SKIN** zones exclude those cells from the vote.
- Fix a compilation issue that occurred when no input configuration file was provided
- Fix issues in Model 1: did not check for the minimum required number of white points, and incorrectly cached previous RG and BG values.

## 2.0.0

- AGC adds environment-luma-driven target luma shift via a PWL (piecewise linear) curve
    - New `luma_pwl` object in AGC JSON config with `enable` flag and `table` array
    - When `enable` is `true`, the AGC reads `env.luma.avg` each frame and linearly interpolates `luma_shift` from the table, shifting `luma_target` accordingly
    - `enable: false` disables the feature with zero overhead; other AGC logic is unaffected
- AWB sub-window: add green-based filter and linear tent weight (dark / mid / bright)
- AWB `update_rg_bg`: refactor to one metadata update path when red or blue step threshold is met.
- AGC supports the sensor without gain control
- Refactor esp_ipa_config Python code
    - Separate algorithm modules into individual files to improve code maintainability
    - Use f-strings to improve code readability
- Add environmental luminance mode to the AGC light metering algorithm
    - **BREAKING CHANGE:** The type of "luma_threshold" in the "esp_ipa_agc_meter_light_threshold" structure has been changed from "uint32_t" to "uint8_t". Valid range is now 0-255. If your code uses values greater than 255, you must update them accordingly.
- Gamma controller now supports configuring each channel individually (R, G, and B)

- Fix issue where color temperature could not recover from excessively low values in certain cases
- Fix issue where RG and BG were calculated directly from statistics instead of using the current configuration's CT module
- Fix IAN hist low_value_radio and high_value_radio: sum all segment counts in [low_index_start..low_index_end] and [high_index_start..high_index_end] instead of using only the last segment (fixes back_light and AGC highlight logic)

## 1.3.1

- Fix ATC Json parameter compatibility
- Fix ISP header file compilation issue when esp-idf version >= 6.0

## 1.3.0

- Add auto BLC(black level correction) algorithm

## 1.2.0

- Add ATC model 0 algorithm
- Package the libesp_ipa.a library that compiled based on esp-idf master(v6.0.0)

## 1.1.0

- Modify the exposure adjustment algorithm of the part-anti-flicker mode
    - If the target gain exceeds the maximum or minimum value, the AGC ignores the anti-flicker and uses all exposure time

## 1.0.1

- Fix the light flicker issue in the part-anti-flicker mode

## 1.0.0

- Add extended configuration to set hue, brightness and statistics window
- Add an autofocus control algorithm to set the sensor focus position by controlling the sensor motor

## 0.3.0~1

- Fix the compiling error with esp-idf v5.4-

## 0.3.0

- Add ATC to configure sensor AE target level by JSON configuration
- AGC different directions of gain and exposure adjustment support different speed parameters
- AGC supports the all exposure range in the part anti-flicker mode
- ACC supports LSC JSON configuration
- ACC adds CCM model 1 to calculate CCM by interpolation algorithm
- ADN JSON configuration supports to generate Gaussian matrix by sigma parameter
- AWB supports to configure statistics parameters
- Pipeline global variables support buffer pointer
- Pipeline supports to add customized IPA node

## 0.2.0

- Added auto color correction algorithm for ISP image color management module
- Added auto denoising algorithm for ISP image denoising module
- Added auto enhancement algorithm for ISP image enhancement module
- Added auto exposure and gain control algorithm for sensor exposure and gain control module
- Added image analysis algorithm to analyze image color temperature and luma
- Added pipeline global variable management functions
- Added script to transform JSON configuration file to C source code for algorithms
- Modified ESP-IDF version from v5.3 to v5.4 to support full ISP modules

## 0.1.0

- Initial version for esp_ipa component

### Enhancements

- Gray world algorithm for auto white balance
- Luma threshold algorithm for auto gain control

