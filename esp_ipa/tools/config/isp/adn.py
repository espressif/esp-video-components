# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import math
from common import ipa_unit_c, fatal_error, cfmt_string, dict_object

class ipa_unit_adn_c(ipa_unit_c):
    @staticmethod
    def decode_adn(name, obj):
        def bf_code(name, obj):
            adn_text = str()
            for i in obj.bf:
                p = i.param
                m = list()
                if hasattr(p, 'matrix'):
                    m = p.matrix
                elif hasattr(p, 'sigma'):
                    sigma = p.sigma
                    if sigma <= 0:
                        raise fatal_error(f'Invalid sigma value: {sigma}, must be positive')
                    num = 3
                    for x in range(0, num):
                        for y in range(0, num):
                            _x = y - (num // 2)
                            _y = (num // 2) - x
                            k = math.exp(-(_x ** 2 + _y ** 2) / (2 * sigma ** 2)) / (2 * math.pi * sigma ** 2)
                            m.append(k)
                    k_max = max(m)
                    for n in range(0, num ** 2):
                        m[n] = min(math.floor(m[n] / k_max * 15 + 0.5), 15)

                if len(m) != 9:
                    raise fatal_error(f'BF matrix must have exactly 9 elements, got {len(m)}')

                adn_text += (f'''
                    {{
                        .gain = {int(i.gain * 1000)},
                        .bf = {{
                            .level = {p.level},
                            .matrix = {{
                                {{{m[0]}, {m[1]}, {m[2]}}},
                                {{{m[3]}, {m[4]}, {m[5]}}},
                                {{{m[6]}, {m[7]}, {m[8]}}}
                            }}
                        }}
                    }},''')

            return adn_text

        def dm_code(name, obj):
            dm_text = str()
            for i in obj.demosaic:
                dm_text += (f'''
                    {{
                        .gain = {int(i.gain * 1000)},
                        .dm = {{
                            .gradient_ratio = {i.gradient_ratio}
                        }}
                    }},''')

            return dm_text

        adn_code = str()
        adn_obj_code = str()
        if hasattr(obj, 'bf'):
            adn_code += cfmt_string(f'''
                static const esp_ipa_adn_bf_t s_ipa_adn_bf_{name}_config[] = {{
                    {bf_code(name, obj)}
                }};
                ''')
            adn_obj_code += (f'''
                .bf_table = s_ipa_adn_bf_{name}_config,
                .bf_table_size = ARRAY_SIZE(s_ipa_adn_bf_{name}_config),
                ''')

        if hasattr(obj, 'demosaic'):
            adn_code += cfmt_string(f'''
                static const esp_ipa_adn_dm_t s_ipa_adn_dm_{name}_config[] = {{
                    {dm_code(name, obj)}
                }};
                ''')

            adn_obj_code += (f'''
                .dm_table = s_ipa_adn_dm_{name}_config,
                .dm_table_size = ARRAY_SIZE(s_ipa_adn_dm_{name}_config),
                ''')

        adn_code += cfmt_string(f'''
            static const esp_ipa_adn_config_t s_ipa_adn_{name}_config = {{
                {adn_obj_code}
            }};
            ''')

        return adn_code

    def decode(self, obj):
        self.text = self.decode_adn(self.name, obj)


if __name__ == '__main__':
    json_obj = dict_object({
    'bf':
    [
        {
            'gain': 1,
            'param': {
                'level': 1,
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
                'level': 2,
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
                'level': 3,
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
                'level': 4,
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
                'level': 5,
                'matrix':
                [
                    5, 5, 5,
                    5, 5, 5,
                    5, 5, 5
                ]
            }
        },
        {
            'gain': 128,
            'param': {
                'level': 5,
                'sigma': 1
            }
        },
        {
            'gain': 144,
            'param': {
                'level': 5,
                'sigma': 2
            }
        }
    ],
    'demosaic':
    [
        {
            'gain': 1,
            'gradient_ratio': 1
        },
        {
            'gain': 2,
            'gradient_ratio': 2
        },
        {
            'gain': 3,
            'gradient_ratio': 3
        },
        {
            'gain': 4,
            'gradient_ratio': 4
        },
        {
            'gain': 5,
            'gradient_ratio': 5
        }
    ]
    })

    c_code = ipa_unit_adn_c(json_obj, 'sc2336', 'adn').get_text()
    print(c_code)
