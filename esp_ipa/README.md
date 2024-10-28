# Espressif Image Process Algorithm for ISP

Espressif image process algorithm component provides a suit of image process algorithms.[![Component Registry](https://components.espressif.com/components/espressif/esp_ipa/badge.svg)](https://components.espressif.com/components/espressif/esp_ipa)

## Algorithm

| Algorithm Name(1) | Function | 
|:-:|:-:|
| "awb.gray" | Gray world algorithm for auto white balance |
| "agc.threshold" | Gain threshold algorithm for auto gain control |

- (1): The algorithm index name is used by initialize function `esp_ipa_pipeline_create`, for example:

    ```c
        esp_ipa_pipeline_handle_t handle;
        static const char *s_ipa_names[] = {
            "esp_ipa_adn",
            "esp_ipa_awb",
            "esp_ipa_acc",
            "esp_ipa_agc",
            "esp_ipa_aen",
        };

        ...

        static const esp_ipa_config_t s_ipa_config = {
            .names = s_ipa_names,
            .nums = ARRAY_SIZE(s_ipa_names),
            ...
        }

        ...

        ret = esp_ipa_pipeline_create(&s_ipa_config, &handle);

        ...
    ```
