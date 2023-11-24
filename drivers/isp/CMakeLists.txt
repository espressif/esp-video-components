set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_SRCS "camera_isp.c")

# set(COMPONENT_REQUIRES mipi_soc)
set(COMPONENT_REQUIRES esp-video)

register_component()