set(srcs "src/esp_cam_sensor.c")

set(include_dirs "include")
set(requires "driver" "esp_sccb_intf")

if(CONFIG_CAMERA_OV2640)
    list(APPEND srcs "sensors/ov2640/ov2640.c")
    list(APPEND include_dirs "sensors/ov2640/include")
    list(APPEND priv_include_dirs "sensors/ov2640/private_include")
endif()

if(CONFIG_CAMERA_OV5645)
    list(APPEND srcs "sensors/ov5645/ov5645.c")
    list(APPEND include_dirs "sensors/ov5645/include")
    list(APPEND priv_include_dirs "sensors/ov5645/private_include")
endif()

if(CONFIG_CAMERA_OV5647)
    list(APPEND srcs "sensors/ov5647/ov5647.c")
    list(APPEND include_dirs "sensors/ov5647/include")
    list(APPEND priv_include_dirs "sensors/ov5647/private_include")
endif()

if(CONFIG_CAMERA_SC2336)
    list(APPEND srcs "sensors/sc2336/sc2336.c")
    list(APPEND include_dirs "sensors/sc2336/include")
    list(APPEND priv_include_dirs "sensors/sc2336/private_include")
endif()

if(CONFIG_CAMERA_SC202CS)
    list(APPEND srcs "sensors/sc202cs/sc202cs.c")
    list(APPEND include_dirs "sensors/sc202cs/include")
    list(APPEND priv_include_dirs "sensors/sc202cs/private_include")
endif()

if(CONFIG_CAMERA_GC0308)
    list(APPEND srcs "sensors/gc0308/gc0308.c")
    list(APPEND include_dirs "sensors/gc0308/include")
    list(APPEND priv_include_dirs "sensors/gc0308/private_include")
endif()

if(CONFIG_CAMERA_SC101IOT)
    list(APPEND srcs "sensors/sc101iot/sc101iot.c")
    list(APPEND include_dirs "sensors/sc101iot/include")
    list(APPEND priv_include_dirs "sensors/sc101iot/private_include")
endif()

if(CONFIG_CAMERA_SC030IOT)
    list(APPEND srcs "sensors/sc030iot/sc030iot.c")
    list(APPEND include_dirs "sensors/sc030iot/include")
    list(APPEND priv_include_dirs "sensors/sc030iot/private_include")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_INCLUDE_DIRS ${priv_include_dirs}
                       REQUIRES ${requires}
                       LDFRAGMENTS linker.lf)

if(CONFIG_CAMERA_OV2640_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov2640_detect")
endif()

if(CONFIG_CAMERA_OV5645_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov5645_detect")
endif()

if(CONFIG_CAMERA_OV5647_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov5647_detect")
endif()

if(CONFIG_CAMERA_SC2336_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc2336_detect")
endif()

if(CONFIG_CAMERA_SC202CS_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc202cs_detect")
endif()

if(CONFIG_CAMERA_GC0308_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u gc0308_detect")
endif()

if(CONFIG_CAMERA_SC101IOT_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc101iot_detect")
endif()

if(CONFIG_CAMERA_SC030IOT_AUTO_DETECT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u sc030iot_detect")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
