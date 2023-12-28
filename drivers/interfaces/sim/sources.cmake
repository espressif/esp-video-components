if(CONFIG_SIMULATED_INTF)
    list(APPEND srcs "drivers/interfaces/sim/sim_video.c")

    list(APPEND include_dirs "drivers/interfaces/sim/include")
endif()
