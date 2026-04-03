# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, base_default, cfmt_string, dict_object

class ext_default():
    def __init__(self, name):
        self.hue = 0
        self.brightness = 0
        self.stats_region = base_default()
        self.stats_region.left = 0
        self.stats_region.top = 0
        self.stats_region.width = 0
        self.stats_region.height = 0

class ipa_unit_ext_c(ipa_unit_c):
    @staticmethod
    def decode_ext(name, obj):
        ext = ext_default(name)
        if hasattr(obj, 'hue'):
            ext.hue = obj.hue
        if hasattr(obj, 'brightness'):
            ext.brightness = obj.brightness
        if hasattr(obj, 'stats_region'):
            sr = obj.stats_region
            ext.stats_region = dict_object(sr) if isinstance(sr, dict) else sr

        config_text = cfmt_string(f'''
            static const esp_ipa_ext_config_t s_ipa_ext_{name}_config = {{
                .hue = {ext.hue},
                .brightness = {ext.brightness},
                .stats_region = {{
                    .top = {ext.stats_region.top},
                    .left = {ext.stats_region.left},
                    .width = {ext.stats_region.width},
                    .height = {ext.stats_region.height}
                }}
            }};
            ''')

        return config_text

    def decode(self, obj):
        self.text = self.decode_ext(self.name, obj)

if __name__ == '__main__':
    json_obj = dict_object({
        'hue': 1,
        'brightness': 2,
        'stats_region':
        {
            'left': 3,
            'top': 4,
            'width': 5,
            'height': 6
        }
    })

    c_code = ipa_unit_ext_c(json_obj, 'sc2336', 'ext').get_text()
    print(c_code)
