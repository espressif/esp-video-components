
set(srcs "src/esp_video.c" "src/esp_video_buffer.c" "src/esp_media.c")
set(include_dirs "include" "include/drivers" "include/soc")
# set(priv_requires "esp_timer" "vfs")
set(priv_requires "esp_timer" "vfs" "sccb" "isp")
set(requires "")

list(APPEND srcs "drivers/csi/${IDF_TARGET}/mipi_csi.c" "drivers/csi/${IDF_TARGET}/mipi_csi_hal.c")
list(APPEND srcs "src/esp_color_format.c")

list(APPEND srcs "src/esp_camera.c")

list(APPEND srcs "drivers/cameras/sc2336/sc2336.c")

if(CONFIG_ESP_VIDEO_BOARD_SIMULATION)
    list(APPEND srcs "boards/sim/sim_init.c" "boards/sim/sim_camera.c")
endif()

if(CONFIG_ESP_VIDEO_API_LINUX)
    list(APPEND srcs "src/esp_video_vfs.c" "src/esp_video_mman.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES ${priv_requires}
                       REQUIRES ${requires}
                       LDFRAGMENTS linker.lf)
