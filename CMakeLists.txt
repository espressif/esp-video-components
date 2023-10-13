
set(srcs)
set(include_dirs)
set(priv_requires)
set(requires)

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES ${priv_requires}
                       REQUIRES ${requires}
                       LDFRAGMENTS linker.lf)
