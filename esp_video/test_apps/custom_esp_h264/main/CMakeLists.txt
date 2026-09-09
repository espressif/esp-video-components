set(srcs "test_app_main.c")

idf_component_register(SRCS ${srcs}
                       REQUIRES "esp_video_wrap")
