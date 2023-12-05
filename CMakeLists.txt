set(srcs "src/esp_camera.c"
         "src/esp_color_format.c"
         "src/esp_media.c"
         "src/esp_video.c"
         "src/esp_video_buffer.c"
         )
set(include_dirs "include" "include/drivers" "include/soc")
set(priv_include_dirs "private_include")
set(priv_requires "esp_timer" "vfs")
set(requires "")

include(${COMPONENT_DIR}/drivers/isp/sources.cmake)
include(${COMPONENT_DIR}/drivers/mipi/csi/sources.cmake)
include(${COMPONENT_DIR}/drivers/sccb/sources.cmake)
include(${COMPONENT_DIR}/drivers/sim/sources.cmake)

if(CONFIG_ESP_VIDEO_API_LINUX)
    list(APPEND srcs "src/esp_video_vfs.c" "src/esp_video_mman.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_INCLUDE_DIRS ${priv_include_dirs}
                       PRIV_REQUIRES ${priv_requires}
                       REQUIRES ${requires}
                       LDFRAGMENTS linker.lf)
