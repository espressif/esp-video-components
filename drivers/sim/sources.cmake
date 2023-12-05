if(CONFIG_SIMULATED_INTF)
    list(APPEND srcs "drivers/sim/sim_init.c"
                     "drivers/sim/sim_video.c")

    list(APPEND include_dirs "drivers/sim/include")
endif()
