set(srcs "test_app_main.c" "test_posix_v4l2.c" "test_storage.c")
set(includes "." "../../../private_include")
set(requires "unity" "test_utils" "esp_video" "esp_timer")

if (CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE)
    list(APPEND srcs "test_format_convert.c")
endif()

if (CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE)
    list(APPEND srcs "test_isp.c")
endif()

if (CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE_RISCV)
    list(APPEND srcs "test_data_reprocessing.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${includes}
                       REQUIRES ${requires}
                       WHOLE_ARCHIVE)
