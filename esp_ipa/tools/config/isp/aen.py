# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object

class ipa_unit_aen_c(ipa_unit_c):
    @staticmethod
    def decode_aen(name, obj):
        def gamma_code(name, obj):
            gamma = obj.gamma
            gamma_table_text = str()
            use_gamma_param_str = str()

            gamma_text = str()
            if gamma.use_gamma_param == False:
                gamma_table_str = str()
                use_gamma_param_str = 'false'

                for u in gamma.table:
                    gamma_x = str()
                    for i in range(0, 16): gamma_x += f'{min((i + 1) * 16, 255)}, '
                    
                    gamma_y = str()
                    for i in u.y: gamma_y += f'{i}, '

                    gamma_table_str += cfmt_string(f'''
                        {{
                            .luma = {u.luma},
                            .gamma = {{
                                .x = {{ {gamma_x} }},
                                .y = {{ {gamma_y} }}
                            }}
                        }},
                    ''')
                
                gamma_table_text = cfmt_string(f'''
                    static const esp_ipa_aen_gamma_unit_t s_esp_ipa_aen_gamma_{name}_table[] = {{
                        {gamma_table_str}
                    }};
                    ''')
            else:
                gamma_table_str = str()
                use_gamma_param_str = 'true'

                for u in gamma.table:
                    gamma_table_str += cfmt_string(f'''
                        {{
                            .luma = {u.luma},
                            .gamma_param = {u.gamma_param},
                        }},
                        ''')
                
                gamma_table_text = cfmt_string(f'''
                    static const esp_ipa_aen_gamma_unit_t s_esp_ipa_aen_gamma_{name}_table[] = {{
                        {gamma_table_str}
                    }};
                    ''')

            if not hasattr(gamma, 'model'):
                gamma.model = 0

            gamma_text = cfmt_string(f'''
                .model = {gamma.model},
                .use_gamma_param = {use_gamma_param_str},
                .luma_env = \"{gamma.luma_env}\",
                .luma_min_step = {gamma.luma_min_step},
                .gamma_table = s_esp_ipa_aen_gamma_{name}_table,
                .gamma_table_size = {len(gamma.table)},
                ''')

            return (gamma_text, gamma_table_text)

        def sharpen_code(name, obj):
            sharpen_text = str()
            for i in obj.sharpen:
                p = i.param
                m = p.matrix

                if len(m) != 9:
                    raise fatal_error(f'sharpen matrix must have exactly 9 elements, got {len(m)}')

                sharpen_text += (f'''
                    {{
                        .gain = {int(i.gain * 1000)},
                        .sharpen = {{
                            .h_thresh = {p.h_thresh},
                            .l_thresh = {p.l_thresh},
                            .h_coeff = {p.h_coeff},
                            .m_coeff = {p.m_coeff},
                            .matrix = {{
                                {{{m[0]}, {m[1]}, {m[2]}}},
                                {{{m[3]}, {m[4]}, {m[5]}}},
                                {{{m[6]}, {m[7]}, {m[8]}}}
                            }}
                        }}
                    }},'''
                )

            return sharpen_text

        def contrast_code(name, obj):
            contrast_text = str()
            for i in obj.contrast:
                contrast_text += cfmt_string(f'''
                    {{
                        .gain = {int(i.gain * 1000)},
                        .contrast = {i.value}
                    }},
                    ''')
            return contrast_text

        aen_code = str()
        aen_obj_code = str()
        if hasattr(obj, 'gamma'):
            text = gamma_code(name, obj)
            aen_code += text[1]
            aen_code += cfmt_string(f'''
                static const esp_ipa_aen_gamma_config_t s_ipa_aen_gamma_{name}_config = {{
                    {text[0]}
                }};
                ''')
            aen_obj_code += cfmt_string(f'''
                .gamma = &s_ipa_aen_gamma_{name}_config,
                ''')

        if hasattr(obj, 'sharpen'):
            aen_code += cfmt_string(f'''
                static const esp_ipa_aen_sharpen_t s_ipa_aen_sharpen_{name}_config[] = {{
                    {sharpen_code(name, obj)}
                }};
                ''')
            aen_obj_code += cfmt_string(f'''
                .sharpen_table = s_ipa_aen_sharpen_{name}_config,
                .sharpen_table_size = ARRAY_SIZE(s_ipa_aen_sharpen_{name}_config),
                ''')

        if hasattr(obj, 'contrast'):
            aen_code += cfmt_string(f'''
                static const esp_ipa_aen_con_t s_ipa_aen_con_{name}_config[] = {{
                    {contrast_code(name, obj)}
                }};
                ''')
            aen_obj_code += cfmt_string(f'''
                .con_table = s_ipa_aen_con_{name}_config,
                .con_table_size = ARRAY_SIZE(s_ipa_aen_con_{name}_config),
                ''')

        aen_code += cfmt_string(f'''
            static const esp_ipa_aen_config_t s_ipa_aen_{name}_config = {{
                {aen_obj_code}
            }};
            ''')

        return aen_code

    def decode(self, obj):
        self.text = self.decode_aen(self.name, obj)


if __name__ == '__main__':
    json_obj = dict_object({
        'gamma':
        {
            'use_gamma_param': False,
            'luma_env': 'dummy_gamma_luma',
            'luma_min_step': 3.0,
            'table':
            [
                {
                    'luma': 10.1,
                    'gamma_param': 1.0,
                    'y': 
                    [
                        0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 255
                    ]
                },
                {
                    'luma': 20.1,
                    'gamma_param': 1.3,
                    'y':
                    [
                        16, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 240, 255
                    ]
                },
                {
                    'luma': 30.1,
                    'gamma_param': 1.6,
                    'y':
                    [
                        32, 48, 56, 64, 72, 80, 96, 112, 128, 144, 160, 176, 192, 240, 248, 255
                    ]
                }
            ]
        },
        'sharpen':
        [
            {
                'gain': 1,
                'param': {
                    'h_thresh': 1,
                    'l_thresh': 1,
                    'h_coeff': 1,
                    'm_coeff': 1,
                    'matrix':
                    [
                        1, 1, 1,
                        1, 1, 1,
                        1, 1, 1
                    ]
                }
            },
            {
                'gain': 2,
                'param': {
                    'h_thresh': 2,
                    'l_thresh': 2,
                    'h_coeff': 2,
                    'm_coeff': 2,
                    'matrix':
                    [
                        2, 2, 2,
                        2, 2, 2,
                        2, 2, 2
                    ]
                }
            },
            {
                'gain': 3,
                'param': {
                    'h_thresh': 3,
                    'l_thresh': 3,
                    'h_coeff': 3,
                    'm_coeff': 3,
                    'matrix':
                    [
                        3, 3, 3,
                        3, 3, 3,
                        3, 3, 3
                    ]
                }
            },
            {
                'gain': 4,
                'param': {
                    'h_thresh': 4,
                    'l_thresh': 4,
                    'h_coeff': 4,
                    'm_coeff': 4,
                    'matrix':
                    [
                        4, 4, 4,
                        4, 4, 4,
                        4, 4, 4
                    ]
                }
            },
            {
                'gain': 5,
                'param': {
                    'h_thresh': 5,
                    'l_thresh': 5,
                    'h_coeff': 5,
                    'm_coeff': 5,
                    'matrix':
                    [
                        5, 5, 5,
                        5, 5, 5,
                        5, 5, 5
                    ]
                }
            }
        ],
        'contrast':
        [
            {
                'gain': 1,
                'value': 1
            },
            {
                'gain': 2,
                'value': 2
            },
            {
                'gain': 3,
                'value': 3
            },
            {
                'gain': 4,
                'value': 4
            },
            {
                'gain': 5,
                'value': 5
            }
        ]
    })

    c_code = ipa_unit_aen_c(json_obj, 'sc2336', 'aen').get_text()
    print(c_code)
