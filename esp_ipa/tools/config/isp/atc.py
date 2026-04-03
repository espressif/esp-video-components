# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, cfmt_string, dict_object

class ipa_unit_atc_c(ipa_unit_c):
    @staticmethod
    def decode_atc(name, obj):
        if not hasattr(obj, 'model'):
            obj.model = 0
        
        if not hasattr(obj, 'delay_frames'):
            obj.delay_frames = 0
        
        if not hasattr(obj, 'luma_env'):
            obj.luma_env = 'NULL'
        
        if not hasattr(obj, 'min_ae_value_step'):
            obj.min_ae_value_step = 1

        if not hasattr(obj, 'init_value'):
            obj.init_value = 30

        def luma_lut_code(name, obj):
            luma_lut_text = str()
            for i in obj.luma_lut:
                luma_lut_text += (f'''
                    {{
                        .luma = {i.luma},
                        .ae_value = {i.ae_value}
                    }},''')

            return luma_lut_text

        atc_code = str()
        atc_obj_code = str()
        atc_luma_lut = 'NULL'
        atc_luma_lut_size = '0'
        if hasattr(obj, 'luma_lut'):
            atc_code += cfmt_string(f'''
                static const esp_ipa_atc_luma_lut_t s_ipa_atc_luma_lut_{name}_config[] = {{
                    {luma_lut_code(name, obj)}
                }};'''
            )

            atc_luma_lut = f's_ipa_atc_luma_lut_{name}_config'
            atc_luma_lut_size = f'{len(obj.luma_lut)}'

        if obj.luma_env is None or obj.luma_env == 'NULL':
            luma_env_rhs = 'NULL'
        else:
            luma_env_rhs = f'"{obj.luma_env}"'

        atc_code += cfmt_string(f'''
            static const esp_ipa_atc_config_t s_ipa_atc_{name}_config = {{
                .model = {obj.model},
                .init_value = {obj.init_value},
                .delay_frames = {obj.delay_frames},
                .luma_env = {luma_env_rhs},
                .min_ae_value_step = {obj.min_ae_value_step},
                .luma_lut = {atc_luma_lut},
                .luma_lut_size = {atc_luma_lut_size}
            }};
            ''')

        return atc_code

    def decode(self, obj):
        self.text = self.decode_atc(self.name, obj)


if __name__ == '__main__':
    json_obj = dict_object({
        'model': 0,
        'init_value': 100,
        'delay_frames': 5,
        'luma_env': 'etc_env_luma',
        'min_ae_value_step': 10,
        'luma_lut':
        [
            {
                'luma': 100,
                'ae_value': 100
            },
            {
                'luma': 200,
                'ae_value': 200
            },
            {
                'luma': 300,
                'ae_value': 300
            }
        ]
    })

    c_code = ipa_unit_atc_c(json_obj, 'sc2336', 'atc').get_text()
    print(c_code)
