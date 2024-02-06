if(CONFIG_ESP_VIDEO_SW_CODEC)
    list(APPEND srcs "utils/codec/esp_jpg_decode.c"
                     "utils/codec/jpge.cpp"
                     "utils/codec/to_bmp.c"
                     "utils/codec/to_jpg.cpp"
                     "utils/codec/yuv.c")

    list(APPEND include_dirs "utils/codec/include")
    list(APPEND priv_include_dirs "utils/codec/private_include")

    if (NOT CONFIG_ESP_ROM_HAS_JPEG_DECODE)
        list(APPEND srcs "utils/codec/tjpgd.c")
        list(APPEND include_dirs "utils/codec/jpeg_include")
    endif()

    if (CONFIG_ESP_VIDEO_SW_CODEC_JPEG_DEVICE)
        list(APPEND srcs "utils/codec/jpeg_video.c")
    endif()
endif()