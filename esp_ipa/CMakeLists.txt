idf_build_get_property(idf_target IDF_TARGET)

set(include_dirs "include")

set(ipa_config_source "${CMAKE_CURRENT_BINARY_DIR}/esp_video_ipa_config.c")
set(srcs ${ipa_config_source} "src/version.c" "src/esp_ipa_detect.c")

if(CONFIG_ESP_IPA_DETECT_METHOD_DYNAMIC_LINK)
    set(ldfragments "linker.lf")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES esp_driver_isp
                       LDFRAGMENTS ${ldfragments})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

set(lib_name "libesp_ipa.a")

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    set(ipa_lib_version "v6.0+")

    if (CONFIG_LIBC_NEWLIB)
        set(lib_name "libesp_ipa_newlib.a")
    endif()
elseif("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_EQUAL "5.5")
    set(ipa_lib_version "v5.5")
else()
    set(ipa_lib_version "v5.4-")
endif()

add_prebuilt_library(prebuilt "lib/${idf_target}/${ipa_lib_version}/${lib_name}"
                     PRIV_REQUIRES esp_timer)
target_link_libraries(${COMPONENT_LIB} PRIVATE prebuilt)
# prebuilt blob needs __wrap_esp_ipa_detect_get_array from this component's src.
# Use the ${COMPONENT_LIB} target, not the name, so it resolves when namespaced
# (espressif__esp_ipa) for registry consumers.
target_link_libraries(prebuilt INTERFACE ${COMPONENT_LIB})

set_property(TARGET prebuilt APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-Wl,--wrap=esp_ipa_detect_get_array")

if(CONFIG_ESP_IPA_DETECT_METHOD_DYNAMIC_LINK)
    if(CONFIG_ESP_IPA_IAN_ALGORITHM)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_ian")
    endif()

    if(CONFIG_ESP_IPA_ADN_ALGORITHM)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_adn")
    endif()

    if(CONFIG_ESP_IPA_AWB_ALGORITHM)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_awb")
    endif()

    if(CONFIG_ESP_IPA_ACC_ALGORITHM)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_acc")
    endif()

    if(CONFIG_ESP_IPA_AGC_ALGORITHM)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_agc")
    endif()

    if(CONFIG_ESP_IPA_AEN_ALGORITHM)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_aen")
    endif()

    if(CONFIG_ESP_IPA_ATC_ALGORITHM)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_atc")
    endif()

    if(CONFIG_ESP_IPA_EXT_CONFIG)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_ext")
    endif()

    if(CONFIG_ESP_IPA_AF_ALGORITHM)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_af")
    endif()
endif()

idf_build_get_property(python PYTHON)
idf_build_get_property(sdkconfig_header SDKCONFIG_HEADER)
set(ipa_config_py_script ${COMPONENT_DIR}/tools/config/esp_ipa_config.py)

idf_build_get_property(esp_ipa_json_config_file_path ESP_IPA_JSON_CONFIG_FILE_PATH)
if(esp_ipa_json_config_file_path)
    foreach(file_path IN LISTS esp_ipa_json_config_file_path)
        get_filename_component(file_abs_path "${file_path}" ABSOLUTE BASE_DIR "${project_dir}")
        if(NOT EXISTS "${file_abs_path}")
            message(FETAL_ERROR  "IPA JSON configuration file ${file_abs_path} doesn't exist")
        endif()

        list(APPEND ipa_json_config ${file_abs_path})
    endforeach()

    list(JOIN ipa_json_config " " ipa_json_config_args)
    set(ipa_config_args -i ${ipa_json_config_args})
endif()

list(APPEND ipa_config_args -o ${ipa_config_source} -v ${CONFIG_ESP_IPA_CONFIG_PARAM_VERSION})
set(ipa_config_cmd ${python} -B ${ipa_config_py_script} ${ipa_config_args})

add_custom_command(
    OUTPUT ${ipa_config_source}
    COMMAND ${ipa_config_cmd}
    DEPENDS ${sdkconfig_header} ${ipa_json_config} ${ipa_config_py_script}
    COMMENT "Generating ${script_out} IPA configuration file..."
    VERBATIM
)