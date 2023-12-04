if(CONFIG_IDF_TARGET_ESP32P4)
    # Current I2C and I3C SCCB driver are only for P4.

    list(APPEND srcs "drivers/sccb/i2c.c" "drivers/sccb/i3c.c" "drivers/sccb/sccb_init.c")
    list(APPEND include_dirs "drivers/sccb/include")
endif()