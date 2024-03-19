set(srcs test_app_main.c
         test_interface.c)

list(APPEND srcs)

idf_component_register(SRCS ${srcs}
                       )
