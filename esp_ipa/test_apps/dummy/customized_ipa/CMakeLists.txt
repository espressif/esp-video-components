idf_component_register(SRCS "src/esp_ipa_customized_0.c"
                            "src/esp_ipa_customized_1.c"
                       PRIV_REQUIRES esp_ipa)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_customized_0")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __esp_ipa_detect_fn_esp_ipa_customized_1")
