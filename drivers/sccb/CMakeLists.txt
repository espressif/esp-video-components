set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_PRIV_INCLUDEDIRS "include")
set(COMPONENT_SRCS "i2c.c" "i3c.c" "sccb_init.c")

# set(COMPONENT_REQUIRES mipi_soc)
set(COMPONENT_REQUIRES "esp-video")

register_component()