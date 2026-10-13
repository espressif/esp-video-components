set(srcs "src/sccb.c")
set(include "include" "interface")

if(CONFIG_SOC_I2C_SUPPORTED)
    list(APPEND srcs "sccb_i2c/src/sccb_i2c.c")
endif()

list(APPEND include "sccb_i2c/include")

set(requires esp_driver_i2c)
set(priv_requires)

# `CONFIG_SOC_I3C_MASTER_SUPPORTED` cannot be used during the dependency expansion phase.
idf_build_get_property(idf_target IDF_TARGET)
if(idf_target STREQUAL "esp32p4"
   AND "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}" VERSION_GREATER_EQUAL "6.1.0")
    list(APPEND requires esp_driver_i3c)
    list(APPEND priv_requires esp_mm)
endif()

# esp_driver_i3c is available starting with ESP-IDF v6.1.0.
set(sccb_i3c_supported 0)
if(CONFIG_SOC_I3C_MASTER_SUPPORTED AND "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}" VERSION_GREATER_EQUAL "6.1.0")
    set(sccb_i3c_supported 1)
endif()

if(sccb_i3c_supported)
    list(APPEND srcs "sccb_i3c/src/sccb_i3c.c")
    list(APPEND include "sccb_i3c/include")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include}
                       REQUIRES ${requires}
                       PRIV_REQUIRES ${priv_requires}
                      )

if(sccb_i3c_supported)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ESP_SCCB_I3C_SUPPORTED=1)
endif()