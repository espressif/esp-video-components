# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object

ISP_GAMMA_CURVE_POINTS_NUM = 16

def round_half_up(x):
    if x < 0:
        raise fatal_error(f'x must be greater than 0, got {x}')
    return int(x + 0.5)

class gamma_c:
    def gen_x_y_data(self, use_gamma_param, gamma_param, u):
        if use_gamma_param == True:
            if not hasattr(u, 'x'):
                gamma_x_data = []
                for i in range(0, ISP_GAMMA_CURVE_POINTS_NUM):
                    gamma_x_data.append(min((i + 1) * ISP_GAMMA_CURVE_POINTS_NUM, 255))
                u.x = gamma_x_data
            else:
                if len(u.x) != ISP_GAMMA_CURVE_POINTS_NUM:
                    raise fatal_error(f'gamma x must have exactly {ISP_GAMMA_CURVE_POINTS_NUM} elements, got {len(u.x)}')

            gamma_y_data = []
            linear_gain = 1.0 / pow((256 - (255 / ISP_GAMMA_CURVE_POINTS_NUM)) / 255, gamma_param)
            for i in u.x:
                v = round_half_up(min(pow(i / 255, gamma_param) * 255 * linear_gain, 255))
                gamma_y_data.append(v)
            u.y = gamma_y_data
        else:
            if not hasattr(u, 'x'):
                gamma_x_data = []
                for i in range(0, ISP_GAMMA_CURVE_POINTS_NUM):
                    gamma_x_data.append(min((i + 1) * ISP_GAMMA_CURVE_POINTS_NUM, 255))
                u.x = gamma_x_data
            else:
                if len(u.x) != ISP_GAMMA_CURVE_POINTS_NUM:
                    raise fatal_error(f'gamma x must have exactly {ISP_GAMMA_CURVE_POINTS_NUM} elements, got {len(u.x)}')
            
            if not hasattr(u, 'y'):
                raise fatal_error('gamma y is required when use_gamma_param is False')
            if len(u.y) != ISP_GAMMA_CURVE_POINTS_NUM:
                raise fatal_error(f'gamma y must have exactly {ISP_GAMMA_CURVE_POINTS_NUM} elements, got {len(u.y)}')

        return u

    def gen_x_y_str(self, u):
        x_str = str()
        y_str = str()

        for i in u.x: x_str += f'{i}, '
        for i in u.y: y_str += f'{i}, '

        return (x_str, y_str)

    def gen_gamma_data_str(self):
        for i in range(0, len(self.gamma.table)):
            if not hasattr(self.gamma.table[i], 'red'):
                self.gamma.table[i].red = dict_object({})
                if hasattr(self.gamma.table[i], 'x'):
                    self.gamma.table[i].red.x = self.gamma.table[i].x
                if hasattr(self.gamma.table[i], 'y'):
                    self.gamma.table[i].red.y = self.gamma.table[i].y
            if not hasattr(self.gamma.table[i], 'green'):
                self.gamma.table[i].green = dict_object({})
                if hasattr(self.gamma.table[i], 'x'):
                    self.gamma.table[i].green.x = self.gamma.table[i].x
                if hasattr(self.gamma.table[i], 'y'):
                    self.gamma.table[i].green.y = self.gamma.table[i].y
            if not hasattr(self.gamma.table[i], 'blue'):
                self.gamma.table[i].blue = dict_object({})
                if hasattr(self.gamma.table[i], 'x'):
                    self.gamma.table[i].blue.x = self.gamma.table[i].x
                if hasattr(self.gamma.table[i], 'y'):
                    self.gamma.table[i].blue.y = self.gamma.table[i].y

            red_data = self.gen_x_y_data(self.gamma.use_gamma_param, self.gamma.table[i].gamma_param, self.gamma.table[i].red)
            green_data = self.gen_x_y_data(self.gamma.use_gamma_param, self.gamma.table[i].gamma_param, self.gamma.table[i].green)
            blue_data = self.gen_x_y_data(self.gamma.use_gamma_param, self.gamma.table[i].gamma_param, self.gamma.table[i].blue)

            red_x_str, red_y_str = self.gen_x_y_str(red_data)
            green_x_str, green_y_str = self.gen_x_y_str(green_data)
            blue_x_str, blue_y_str = self.gen_x_y_str(blue_data)

            self.gamma.table[i].red_x_str = red_x_str
            self.gamma.table[i].red_y_str = red_y_str
            self.gamma.table[i].green_x_str = green_x_str
            self.gamma.table[i].green_y_str = green_y_str
            self.gamma.table[i].blue_x_str = blue_x_str
            self.gamma.table[i].blue_y_str = blue_y_str

    def __init__(self, gamma, name):
        self.gamma = gamma
        self.name = name
        
        if not hasattr(self.gamma, 'model'):
            self.gamma.model = 0
        elif self.gamma.model not in (0, 1):
            raise fatal_error(f'gamma model must be 0 or 1, got {self.gamma.model}')

        self.gen_gamma_data_str()

    def get_gamma_table_text(self):
        gamma_table_str = str()

        for u in self.gamma.table:
            gamma_table_str += cfmt_string(f'''
                {{
                    .luma = {u.luma},
                    .gamma = {{
                        .red = {{
                            .x = {{ {u.red_x_str} }},
                            .y = {{ {u.red_y_str} }}
                        }},
                        .green = {{
                            .x = {{ {u.green_x_str} }},
                            .y = {{ {u.green_y_str} }}
                        }},
                        .blue = {{
                            .x = {{ {u.blue_x_str} }},
                            .y = {{ {u.blue_y_str} }}
                        }}
                    }}
                }},
            ''')

        gamma_table_text = cfmt_string(f'''
            static const esp_ipa_aen_gamma_unit_t s_esp_ipa_aen_gamma_{self.name}_table[] = {{
                {gamma_table_str}
            }};
            ''')

        return gamma_table_text

class ipa_unit_aen_c(ipa_unit_c):
    @staticmethod
    def decode_aen(name, obj):
        def gamma_code(name, obj):
            gamma = obj.gamma

            gamma_generator = gamma_c(gamma, name)
            gamma_table_text = gamma_generator.get_gamma_table_text()

            gamma_text = cfmt_string(f'''
                .model = {gamma.model},
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

    print('step 1')
    c_code = ipa_unit_aen_c(json_obj, 'sc2336', 'aen').get_text()
    print(c_code)

    json_obj = dict_object({
        'gamma':
        {
            'model': 1,
            'use_gamma_param': False,
            'luma_env': 'dummy_gamma_luma',
            'luma_min_step': 3.0,
            'table':
            [
                {
                    'luma': 10.1,
                    'gamma_param': 1.0,
                    'x':
                    [
                        0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30
                    ],
                    'y': 
                    [
                        0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 255
                    ]
                },
                {
                    'luma': 20.1,
                    'gamma_param': 1.3,
                    'x':
                    [
                        0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60
                    ],
                    'y':
                    [
                        16, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 240, 255
                    ]
                }
            ]
        }
    })

    print('step 2')
    c_code = ipa_unit_aen_c(json_obj, 'sc2336', 'aen').get_text()
    print(c_code)

    json_obj = dict_object({
        'gamma':
        {
            'model': 1,
            'use_gamma_param': True,
            'luma_env': 'dummy_gamma_luma',
            'luma_min_step': 3.0,
            'table':
            [
                {
                    'luma': 10.1,
                    'gamma_param': 1.2,
                    'x':
                    [
                        0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30
                    ]
                },
                {
                    'luma': 20.1,
                    'gamma_param': 1.3,
                }
            ]
        }
    })

    print('step 3')
    c_code = ipa_unit_aen_c(json_obj, 'sc2336', 'aen').get_text()
    print(c_code)

    json_obj = dict_object({
        'gamma':
        {
            'model': 1,
            'use_gamma_param': False,
            'luma_env': 'dummy_gamma_luma',
            'luma_min_step': 3.0,
            'table':
            [
                {
                    'luma': 10.1,
                    'gamma_param': 1.0,
                    'red':
                    {
                        'x':
                        [
                            0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30
                        ],
                        'y':
                        [
                            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
                        ]
                    },
                    'green':
                    {
                        'x':
                        [
                            1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
                        ],
                        'y':
                        [
                            2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17
                        ]
                    },
                    'blue':
                    {
                        'x':
                        [
                            2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32
                        ],
                        'y':
                        [
                            3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18
                        ]
                    }
                },
                {
                    'luma': 20.1,
                    'gamma_param': 1.3,
                    'red':
                    {
                        'x':
                        [
                            1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
                        ],
                        'y':
                        [
                            4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19
                        ]
                    },
                    'green':
                    {
                        'x':
                        [
                            2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32
                        ],
                        'y':
                        [
                            5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20
                        ]
                    },
                    'blue':
                    {
                        'x':
                        [
                            3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33
                        ],
                        'y':
                        [
                            6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
                        ]
                    }
                }
            ]
        }
    })

    print('step 4')
    c_code = ipa_unit_aen_c(json_obj, 'sc2336', 'aen').get_text()
    print(c_code)

    json_obj = dict_object({
        'gamma':
        {
            'model': 1,
            'use_gamma_param': True,
            'luma_env': 'dummy_gamma_luma',
            'luma_min_step': 3.0,
            'table':
            [
                {
                    'luma': 10.1,
                    'gamma_param': 1.1,
                },
                {
                    'luma': 20.1,
                    'gamma_param': 1.3,
                }
            ]
        }
    })

    print('step 5')
    c_code = ipa_unit_aen_c(json_obj, 'sc2336', 'aen').get_text()
    print(c_code)
