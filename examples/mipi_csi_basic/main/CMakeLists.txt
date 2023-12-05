set(srcs "main.c")

list(APPEND includes ".")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS ${includes}
                    ) #cannot totally hide soc headers, since there are a lot arguments in the driver are chip-dependent
