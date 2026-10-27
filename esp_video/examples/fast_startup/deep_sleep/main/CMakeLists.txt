idf_component_register(SRCS "deep_sleep_main.c")

if(NOT CONFIG_SOC_SDMMC_HOST_SUPPORTED)
    fail_at_build_time(sdmmc ""
                             "SDMMC host is not supported."
                             "Please refer README.md for more details")
endif()
