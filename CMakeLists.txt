set(srcs "src/esp_camera.c"
         "src/esp_color_format.c"
         "src/esp_video.c"
         "src/esp_video_buffer.c"
         )

if(CONFIG_ESP_VIDEO_MEDIA_CONTROLLER)
    list(APPEND srcs "src/esp_media.c")
endif()

set(include_dirs "include")
set(priv_include_dirs "private_include")
set(priv_requires "esp_timer" "vfs" "json")
set(requires "driver")

include(${COMPONENT_DIR}/drivers/commons/sources.cmake)
include(${COMPONENT_DIR}/drivers/interfaces/sources.cmake)
include(${COMPONENT_DIR}/drivers/sccb/sources.cmake)
include(${COMPONENT_DIR}/drivers/xclk/sources.cmake)
include(${COMPONENT_DIR}/utils/sources.cmake)
include(${COMPONENT_DIR}/drivers/isp/sources.cmake)

if (NOT "${CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE}" STREQUAL "")
    if (EXISTS "${CMAKE_SOURCE_DIR}/${CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE}")
        set(embed_files "${CMAKE_SOURCE_DIR}/${CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE}")
    else()
        set(embed_files "${CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE}")
    endif()
    get_filename_component(embed_file_c_name ${embed_files} NAME)
    string(REPLACE "." "_" embed_file_c_name "${embed_file_c_name}")
endif()

if(CONFIG_ESP_VIDEO_API_LINUX)
    list(APPEND srcs "src/esp_video_vfs.c" "src/esp_video_mman.c" "src/esp_video_ioctl.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_INCLUDE_DIRS ${priv_include_dirs}
                       PRIV_REQUIRES ${priv_requires}
                       REQUIRES ${requires}
                       LDFRAGMENTS linker.lf
                       EMBED_TXTFILES ${embed_files})

if (NOT "${CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE}" STREQUAL "")
    add_definitions(-DCONFIG_ESP_VIDEO_MEDIA_EMBED_FILE_NAME="${embed_file_c_name}")
endif()

if (CONFIG_IDF_TARGET_ESP32P4)
    # AEG-1300, add macro "#define CONFIG_SOC_LCDCAM_SUPPORTED 1" only for this component to avoid ESP-IDF compiling error

    target_compile_options(${COMPONENT_LIB} PRIVATE "-DCONFIG_SOC_LCDCAM_SUPPORTED=1")
endif()
