
set(srcs "core/esp_video.c" "core/esp_video_buffer.c" "core/esp_media.c")
set(include_dirs "include")
set(priv_requires "esp_timer" "vfs")
set(requires "")

if(CONFIG_ESP_VIDEO_BOARD_SIMULATION)
    list(APPEND srcs "boards/sim/sim_init.c" "boards/sim/sim_camera.c")
endif()

if(CONFIG_ESP_VIDEO_API_LINUX)
    list(APPEND srcs "apis/linux/esp_video_vfs.c" "apis/linux/esp_video_mman.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES ${priv_requires}
                       REQUIRES ${requires}
                       LDFRAGMENTS linker.lf)
