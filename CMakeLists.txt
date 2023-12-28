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
set(priv_requires "esp_timer" "vfs" "driver" "json")
set(requires "")

include(${COMPONENT_DIR}/drivers/commons/sources.cmake)
include(${COMPONENT_DIR}/drivers/interfaces/sources.cmake)
include(${COMPONENT_DIR}/drivers/sccb/sources.cmake)

if (NOT "${CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE}" STREQUAL "")
    set(embed_files "${CONFIG_ESP_VIDEO_MEDIA_EMBED_FILE}")
    cmake_path(GET embed_files FILENAME embed_file_c_name)
    string(REPLACE "." "_" embed_file_c_name "${embed_file_c_name}")
endif()

if(CONFIG_ESP_VIDEO_API_LINUX)
    list(APPEND srcs "src/esp_video_vfs.c" "src/esp_video_mman.c")
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