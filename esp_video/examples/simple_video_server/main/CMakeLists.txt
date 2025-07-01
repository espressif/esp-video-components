set(srcs "simple_video_server_example.c")
set(html_file "html/single_camera_web.html" "html/dual_camera_web.html")

idf_component_register(SRCS "${srcs}"
                       PRIV_INCLUDE_DIRS .
                       EMBED_TXTFILES ${html_file})
