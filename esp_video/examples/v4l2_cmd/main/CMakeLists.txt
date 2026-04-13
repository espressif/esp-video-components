set(srcs "v4l2_cmd_main.c" "v4l2_cmd_common.c")

if(CONFIG_EXAMPLE_V4L2_CMD_CTL)
    list(APPEND srcs v4l2_cmd_ctl.c)
endif()

if(CONFIG_EXAMPLE_V4L2_CMD_BF)
    list(APPEND srcs v4l2_cmd_bf.c)
endif()

if(CONFIG_EXAMPLE_V4L2_CMD_CCM)
    list(APPEND srcs v4l2_cmd_ccm.c)
endif()

if(CONFIG_EXAMPLE_V4L2_CMD_GAMMA)
    list(APPEND srcs v4l2_cmd_gamma.c)
endif()

idf_component_register(SRCS "${srcs}"
                       PRIV_INCLUDE_DIRS .)
