if(CONFIG_ESP_VIDEO_SW_CODEC)
    list(APPEND srcs "utils/codec/esp_jpg_decode.c"
                     "utils/codec/jpge.cpp"
                     "utils/codec/to_bmp.c"
                     "utils/codec/to_jpg.cpp"
                     "utils/codec/yuv.c")

    list(APPEND include_dirs "utils/codec/include")
    list(APPEND priv_include_dirs "utils/codec/private_include")
endif()