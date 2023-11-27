idf_component_register(SRCS "sc2336.c"
                       PRIV_INCLUDE_DIRS "private_include"
                       PRIV_REQUIRES "esp-video")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})   