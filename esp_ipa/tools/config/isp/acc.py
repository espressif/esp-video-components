# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object

class ipa_unit_acc_c(ipa_unit_c):
    @staticmethod
    def decode_acc(name, obj):
        def saturation_code(name, obj):
            saturation_text = str()
            for i in obj.saturation:
                saturation_text += (f'''
                    {{
                        .color_temp = {i.color_temp},
                        .saturation = {i.value}
                    }},''')
            return saturation_text

        def ccm_code(name, obj):
            ccm = obj.ccm
            ccm_table_sub_text = str()
            for i in ccm.table:
                m = i.matrix

                if len(m) != 9:
                    raise fatal_error(f'CCM matrix must have exactly 9 elements, got {len(m)}')

                ccm_table_sub_text += (f'''
                    {{
                        .color_temp = {i.color_temp},
                        .ccm = {{
                            .matrix = {{
                                {{ {m[0]}, {m[1]}, {m[2]} }},
                                {{ {m[3]}, {m[4]}, {m[5]} }},
                                {{ {m[6]}, {m[7]}, {m[8]} }}
                            }}
                        }}
                    }},''')

            m = ccm.low_luma.matrix
            if len(m) != 9:
                raise fatal_error(f'low_luma CCM matrix must have exactly 9 elements, got {len(m)}')

            ccm_low_table_text = cfmt_string(f'''
                    {{
                        .matrix = {{
                            {{ {m[0]}, {m[1]}, {m[2]} }},
                            {{ {m[3]}, {m[4]}, {m[5]} }},
                            {{ {m[6]}, {m[7]}, {m[8]} }}
                        }}
                    }}''')

            ccm_table_text = cfmt_string(f'''
                static const esp_ipa_acc_ccm_unit_t s_esp_ipa_acc_ccm_{name}_table[] = {{
                    {ccm_table_sub_text}
                }};
                ''')

            if not hasattr(ccm, 'model'):
                ccm.model = 0

            sub_param_text = str()
            if ccm.model == 1:
                sub_param_text = cfmt_string(f'''
                    .min_ct_step = {ccm.min_ct_step},
                    ''')

            ccm_text = cfmt_string(f'''
                static const esp_ipa_acc_ccm_config_t s_esp_ipa_acc_ccm_{name}_config = {{
                    .model = {ccm.model},
                    .luma_env = \"{ccm.low_luma.luma_env}\",
                    .luma_low_threshold = {ccm.low_luma.threshold},
                    .luma_low_ccm = {ccm_low_table_text},
                    .ccm_table = s_esp_ipa_acc_ccm_{name}_table,
                    .ccm_table_size = {len(ccm.table)},
                    {sub_param_text}
                }};
                ''')

            return ccm_table_text + ccm_text

        def lsc_code(name, obj):
            lsc = obj.lsc
            lsc_table_text = str()
            lsc_text = str()

            for i in lsc.table:
                lsc_text += cfmt_string(f'''
                    static const isp_lsc_gain_t s_esp_ipa_acc_lsc_gain_r_{name}_{lsc.img_w}_x_{lsc.img_h}_ct_{i.ct}_config[] = {{
                        {', '.join(f'{{.val = {round(u * 256)}}}' for u in i.calibrations_r_tbl)}
                    }};
                    ''')

                lsc_text += cfmt_string(f'''
                    static const isp_lsc_gain_t s_esp_ipa_acc_lsc_gain_gr_{name}_{lsc.img_w}_x_{lsc.img_h}_ct_{i.ct}_config[] = {{
                        {', '.join(f'{{.val = {round(u * 256)}}}' for u in i.calibrations_gr_tbl)}
                    }};
                    ''')

                lsc_text += cfmt_string(f'''
                    static const isp_lsc_gain_t s_esp_ipa_acc_lsc_gain_gb_{name}_{lsc.img_w}_x_{lsc.img_h}_ct_{i.ct}_config[] = {{
                        {', '.join(f'{{.val = {round(u * 256)}}}' for u in i.calibrations_gb_tbl)}
                    }};
                    ''')

                lsc_text += cfmt_string(f'''
                    static const isp_lsc_gain_t s_esp_ipa_acc_lsc_gain_b_{name}_{lsc.img_w}_x_{lsc.img_h}_ct_{i.ct}_config[] = {{
                        {', '.join(f'{{.val = {round(u * 256)}}}' for u in i.calibrations_b_tbl)}
                    }};
                    ''')

                lsc_table_text += cfmt_string(f'''
                    {{
                        .color_temp = {i.ct},
                        .lsc = {{
                            .gain_r  = s_esp_ipa_acc_lsc_gain_r_{name}_{lsc.img_w}_x_{lsc.img_h}_ct_{i.ct}_config,
                            .gain_gr = s_esp_ipa_acc_lsc_gain_gr_{name}_{lsc.img_w}_x_{lsc.img_h}_ct_{i.ct}_config,
                            .gain_gb = s_esp_ipa_acc_lsc_gain_gb_{name}_{lsc.img_w}_x_{lsc.img_h}_ct_{i.ct}_config,
                            .gain_b  = s_esp_ipa_acc_lsc_gain_b_{name}_{lsc.img_w}_x_{lsc.img_h}_ct_{i.ct}_config,
                            .lsc_gain_array_size = {lsc.lsc_tbl_size}
                        }},
                    }},
                    ''')

            lsc_text += cfmt_string(f'''
                static const esp_ipa_acc_lsc_lut_t s_esp_ipa_acc_lsc_{name}_{lsc.img_w}_x_{lsc.img_h}_config[] = {{
                    {lsc_table_text}
                }};
                ''')

            lsc_text += cfmt_string(f'''
                static const esp_ipa_acc_lsc_t s_esp_ipa_acc_lsc_{name}_config[] = {{
                    {{
                        .width = {lsc.img_w},
                        .height = {lsc.img_h},
                        .lsc_gain_table = s_esp_ipa_acc_lsc_{name}_{lsc.img_w}_x_{lsc.img_h}_config,
                        .lsc_gain_table_size = {len(lsc.table)}
                    }}
                }};
                ''')

            return lsc_text

        def blc_code(name, obj):
            blc = obj.blc
            blc_text = str()
            blc_table_text = str()

            if not hasattr(blc, 'model'):
                blc.model = 0

            for i in blc.blc_table:
                if blc.stretch == True:
                    stretch = '1'
                else:
                    stretch = '0'

                blc_table_text += cfmt_string(f'''
                    {{
                        .gain = {i.gain},
                        .blc_param = {{
                            .stretch = {stretch},
                            .top_left_chan_offset = {i.blc_param.blc_top_left},
                            .top_right_chan_offset = {i.blc_param.blc_top_right},
                            .bottom_left_chan_offset = {i.blc_param.blc_bottom_left},
                            .bottom_right_chan_offset = {i.blc_param.blc_bottom_right}
                        }}
                    }},
                    ''')

            blc_text = cfmt_string(f'''
                static const esp_ipa_acc_blc_param_t s_ipa_acc_blc_{name}_table[] = {{
                    {blc_table_text}
                }};
                ''')

            blc_text += cfmt_string(f'''
                static const esp_ipa_acc_blc_config_t s_ipa_acc_blc_{name}_config = {{
                    .model = {blc.model},
                    .blc_table = s_ipa_acc_blc_{name}_table,
                    .blc_table_size = ARRAY_SIZE(s_ipa_acc_blc_{name}_table)
                }};
                ''')

            return blc_text

        acc_text = str()
        acc_obj_text = str()
        if hasattr(obj, 'saturation'):
            acc_text += cfmt_string(f'''
                static const esp_ipa_acc_sat_t s_ipa_acc_sat_{name}_config[] = {{
                    {saturation_code(name, obj)}
                }};
                ''')
            acc_obj_text += cfmt_string(f'''
                .sat_table = s_ipa_acc_sat_{name}_config,
                .sat_table_size = ARRAY_SIZE(s_ipa_acc_sat_{name}_config),
                '''
            )

        if hasattr(obj, 'ccm'):
            acc_text += ccm_code(name, obj)
            acc_obj_text += cfmt_string(f'''
                .ccm = &s_esp_ipa_acc_ccm_{name}_config,
                '''
            )
        
        if hasattr(obj, 'lsc'):
            acc_text += lsc_code(name, obj)
            acc_obj_text += cfmt_string(f'''
                .lsc_table = s_esp_ipa_acc_lsc_{name}_config,
                .lsc_table_size = ARRAY_SIZE(s_esp_ipa_acc_lsc_{name}_config),
                '''
            )
        
        if hasattr(obj, 'blc'):
            acc_text += blc_code(name, obj)
            acc_obj_text += cfmt_string(f'''
                .blc = &s_ipa_acc_blc_{name}_config,
                '''
            )

        acc_text += cfmt_string(f'''
            static const esp_ipa_acc_config_t s_ipa_acc_{name}_config = {{
                {acc_obj_text}
            }};
            ''')

        return acc_text

    def decode(self, obj):
        self.text = self.decode_acc(self.name, obj)

if __name__ == '__main__':
    json_obj = dict_object({
        'saturation':
        [
            {
                'color_temp': 1000,
                'value': 1
            },
            {
                'color_temp': 2000,
                'value': 2
            },
            {
                'color_temp': 3000,
                'value': 3
            },
            {
                'color_temp': 4000,
                'value': 4
            },
            {
                'color_temp': 5000,
                'value': 5
            }
        ],
        'ccm':
        {
            'low_luma':
            {
                'luma_env': 'dummy_gamma_luma',
                'threshold': 15.5,
                'matrix':
                [
                    1.0, 0, 0,
                    0, 1.0, 0,
                    0, 0, 1.0
                ]
            },
            'table':
            [
                {
                    'color_temp': 1000,
                    'matrix':
                    [
                        1.1, 1.1, 1.1,
                        1.1, 1.1, 1.1,
                        1.1, 1.1, 1.1
                    ]
                },
                {
                    'color_temp': 2000,
                    'matrix':
                    [
                        1.2, 1.2, 1.2,
                        1.2, 1.2, 1.2,
                        1.2, 1.2, 1.2
                    ]
                },
                {
                    'color_temp': 3000,
                    'matrix':
                    [
                        1.3, 1.3, 1.3,
                        1.3, 1.3, 1.3,
                        1.3, 1.3, 1.3
                    ]
                },
                {
                    'color_temp': 4000,
                    'matrix':
                    [
                        1.4, 1.4, 1.4,
                        1.4, 1.4, 1.4,
                        1.4, 1.4, 1.4
                    ]
                },
                {
                    'color_temp': 5000,
                    'matrix':
                    [
                        1.5, 1.5, 1.5,
                        1.5, 1.5, 1.5,
                        1.5, 1.5, 1.5
                    ]
                }
            ]
        },
        'lsc':
        {
            'model': 0,
            'img_w': 1080,
            'img_h': 720,
            'lsc_tbl_size': 128,
            'table':
            [
                {
                    'ct': 3000,
                    'calibrations_r_tbl':
                    [
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
                    ],
                    'calibrations_gr_tbl':
                    [
                        1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5,
                        1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5,
                        1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5,
                        1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5
                    ],
                    'calibrations_gb_tbl':
                    [
                        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
                    ],
                    'calibrations_b_tbl':
                    [
                        2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5,
                        2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5,
                        2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5,
                        2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5
                    ]
                },
                {
                    'ct': 5000,
                    'calibrations_r_tbl':
                    [
                        0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
                        0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
                        0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
                        0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1
                    ],
                    'calibrations_gr_tbl':
                    [
                        0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3,
                        0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3,
                        0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3,
                        0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3
                    ],
                    'calibrations_gb_tbl':
                    [
                        0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5,
                        0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5,
                        0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5,
                        0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5
                    ],
                    'calibrations_b_tbl':
                    [
                        0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
                        0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
                        0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
                        0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8
                    ]
                }
            ]
        },
        'blc':
        {
            'model': 0,
            'stretch': True,
            'blc_table':
            [
                {
                    'gain': 1,
                    'blc_param': {
                        'blc_top_left': 16,
                        'blc_top_right': 16,
                        'blc_bottom_left': 16,
                        'blc_bottom_right': 16
                    }
                },
                {
                    'gain': 17,
                    'blc_param': {
                        'blc_top_left': 32,
                        'blc_top_right': 32,
                        'blc_bottom_left': 32,
                        'blc_bottom_right': 32
                    }
                },
                {
                    'gain': 33,
                    'blc_param': {
                        'blc_top_left': 48,
                        'blc_top_right': 48,
                        'blc_bottom_left': 48,
                        'blc_bottom_right': 48
                    }
                },
                {
                    'gain': 49,
                    'blc_param': {
                        'blc_top_left': 64,
                        'blc_top_right': 64,
                        'blc_bottom_left': 64,
                        'blc_bottom_right': 64
                    }
                }
            ]
        }
    })

    c_code = ipa_unit_acc_c(json_obj, 'sc2336', 'acc').get_text()
    print(c_code)
