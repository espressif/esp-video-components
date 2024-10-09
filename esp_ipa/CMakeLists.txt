idf_build_get_property(idf_target IDF_TARGET)

set(include_dirs "include")

idf_component_register(SRCS "src/version.c"
                       INCLUDE_DIRS ${include_dirs}
                       LDFRAGMENTS linker.lf)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

add_prebuilt_library(prebuilt "lib/${idf_target}/libesp_ipa.a"
                     PRIV_REQUIRES esp_timer)
target_link_libraries(${COMPONENT_LIB} PRIVATE prebuilt)

if(CONFIG_ESP_IPA_AWB_GRAY_WORLD)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_awb_gray_world")
endif()

if(CONFIG_ESP_IPA_AGC_THRESHOLD)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_agc_threshold")
endif()

if(CONFIG_ESP_IPA_DENOISING_GAIN_FEEDBACK)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_denoising_gain_feedback")
endif()

if(CONFIG_ESP_IPA_SHARPEN_FREQUENCY_FEEDBACK)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_sharpen_freq_feedback")
endif()

if(CONFIG_ESP_IPA_GAMMA_LUMA)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_gamma_lumma_feedback")
endif()

if(CONFIG_ESP_IPA_CC_LINEAR)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_cc_linear")
endif()
