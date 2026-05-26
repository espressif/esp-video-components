set(srcs
    "m2m_main.c"
    "example_v4l2.c"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "."
)
