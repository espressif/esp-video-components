# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object

class ipa_unit_ian_c(ipa_unit_c):
    @staticmethod
    def decode_ian(name, obj):
        def ct_code(name, obj):
            ct_bp_text = str()
            for i in obj.bp:
                ct_bp_text += (f'''
                    {{
                        .a0 = {i.a0},
                        .a1 = {i.a1}
                    }},'''
                )

            ct_text = cfmt_string(f'''
                static const esp_ipa_ian_ct_basic_param_t s_ipa_ian_ct_{name}_basic_param[] = {{
                    {ct_bp_text}
                }};''')

            g_a2_obj_text = str()
            for i in obj.g.a2:
                g_a2_obj_text += f'{i}, '

            ct_text += cfmt_string(f'''
                static const float s_esp_ipa_ian_ct_{name}_g_a2[] = {{
                    {g_a2_obj_text}
                }};'''
            )

            # Model 0: no curve params (or from m_0); model 1/2: use m_1/m_2. Reject other values.
            m_a0 = 0
            m_a1 = 0
            m_a2 = 0
            min_step = 0
            if obj.model == 0:
                pass # No curve params (or from m_0)
            elif obj.model == 1:
                m_a0 = obj.m_1.a0
                m_a1 = obj.m_1.a1
                min_step = obj.min_step
            elif obj.model == 2:
                m_a0 = obj.m_2.a0
                m_a1 = obj.m_2.a1
                m_a2 = obj.m_2.a2
                min_step = obj.min_step
            else:
                raise fatal_error(
                    f'IAN color_temp config {name} has unsupported model value: {obj.model}. Expected 0, 1, or 2.'
                )

            ct_text += cfmt_string(f'''
                static const esp_ipa_ian_ct_config_t s_esp_ipa_ian_ct_{name}_config = {{
                    .model = {obj.model},
                    .m_a0 = {m_a0},
                    .m_a1 = {m_a1},
                    .m_a2 = {m_a2},
                    .f_n0 = {obj.f_n0},
                    .bp = s_ipa_ian_ct_{name}_basic_param,
                    .bp_nums = ARRAY_SIZE(s_ipa_ian_ct_{name}_basic_param),
                    .min_step = {min_step},
                    .g_a0 = {obj.g.a0},
                    .g_a1 = {obj.g.a1},
                    .g_a2 = s_esp_ipa_ian_ct_{name}_g_a2,
                    .g_a2_nums = ARRAY_SIZE(s_esp_ipa_ian_ct_{name}_g_a2)       
                }};
                '''
            )

            return ct_text
        
        def luma_code(name, obj):
            luma_text = ''
            luma_sub_text = ''

            if hasattr(obj, 'histogram'):
                hist = obj.histogram

                if len(hist.mean) != 16:
                    raise fatal_error(f'hist.mean size must be 16, but got {len(hist.mean)}')

                mean_text = ''
                for m in hist.mean:
                    mean_text += f'{m},'

                luma_text += cfmt_string(f'''
                    static const esp_ipa_ian_luma_hist_config_t s_esp_ipa_ian_luma_hist_{name}_config = {{                 
                        .mean = {{
                            {mean_text}
                        }},
                        .low_index_start = {hist.low_index.start},
                        .low_index_end = {hist.low_index.end},
                        .high_index_start = {hist.high_index.start},
                        .high_index_end = {hist.high_index.end},
                        .back_light_radio_threshold = {hist.back_light_radio_threshold},
                    }};
                    '''
                )

                luma_sub_text += cfmt_string(f'''
                   .hist = &s_esp_ipa_ian_luma_hist_{name}_config,
                    '''
                )
            
            if hasattr(obj, 'ae'):
                ae = obj.ae

                if len(ae.weight) != 25:
                    raise fatal_error(f'ae.weight size must be 25, but got {len(ae.weight)}')

                weight_text = ''
                for w in ae.weight:
                    weight_text += f'{w},'
                
                luma_text += cfmt_string(f'''
                    static const esp_ipa_ian_luma_ae_config_t s_esp_ipa_ian_luma_ae_{name}_config = {{                 
                        .weight = {{
                            {weight_text}
                        }},
                    }};'''
                )

                luma_sub_text += cfmt_string(f'''
                   .ae = &s_esp_ipa_ian_luma_ae_{name}_config,
                    '''
                )

            if hasattr(obj, 'env'):
                env = obj.env

                if len(env.weight) != 25:
                    raise fatal_error(f'env.weight size must be 25, but got {len(env.weight)}')

                weight_text = ''
                for w in env.weight:
                    weight_text += f'{w},'

                def speed_param_code(name, obj):
                    speed_param_text = str()
                    for i in obj.speed_param:
                        speed_param_text += f'{i}, '
                    return speed_param_text

                luma_text += cfmt_string(f'''
                    static const float s_esp_ipa_ian_luma_env_speed_param_{name}_config[] = {{
                        {speed_param_code(name, env)}
                    }};'''
                )

                luma_text += cfmt_string(f'''
                    static const esp_ipa_ian_luma_env_config_t s_esp_ipa_ian_luma_env_{name}_config = {{
                        .k = {env.k},
                        .speed_param = s_esp_ipa_ian_luma_env_speed_param_{name}_config,
                        .speed_param_size = ARRAY_SIZE(s_esp_ipa_ian_luma_env_speed_param_{name}_config),
                        .weight = {{
                            {weight_text}
                        }},
                    }};'''
                )

                luma_sub_text += cfmt_string(f'''
                   .env = &s_esp_ipa_ian_luma_env_{name}_config,
                    '''
                )

            luma_text += cfmt_string(f'''
                static const esp_ipa_ian_luma_config_t s_esp_ipa_ian_luma_{name}_config = {{
                    {luma_sub_text}
                }};'''
            )

            return luma_text

        ian_text = str()
        ian_obj_text = str()
        if hasattr(obj,'color_temp'):
            ian_text += ct_code(name, obj.color_temp)
            ian_obj_text += cfmt_string(f'''
                .ct = &s_esp_ipa_ian_ct_{name}_config,
                '''
            )

        if hasattr(obj, 'luma'):
            ian_text += luma_code(name, obj.luma)
            ian_obj_text += cfmt_string(f'''
                .luma = &s_esp_ipa_ian_luma_{name}_config,
                '''
            )

        ian_text += cfmt_string(f'''
            static const esp_ipa_ian_config_t s_ipa_ian_{name}_config = {{
                {ian_obj_text}
            }};'''
        )

        return ian_text

    def decode(self, obj):
        self.text = self.decode_ian(self.name, obj)

if __name__ == '__main__':
    json_obj = dict_object({
        'color_temp': {
            'bp':
            [
                {
                    'a0': 10,
                    'a1': 1
                },
                {
                    'a0': 9,
                    'a1': 2
                },
                {
                    'a0': 8,
                    'a1': 3
                },
                {
                    'a0': 7,
                    'a1': 4
                },
                {
                    'a0': 6,
                    'a1': 5
                },
                {
                    'a0': 5,
                    'a1': 6
                },
                {
                    'a0': 4,
                    'a1': 7
                },
                {
                    'a0': 3,
                    'a1': 8
                },
                {
                    'a0': 2,
                    'a1': 9
                },
                {
                    'a0': 1,
                    'a1': 10
                }
            ],
            'model': 0,
            'm_0':
            {
                
            },
            'm_1':
            {
                'a0': -1,
                'a1': 11
            },
            'm_2':
            {
                'a0': -1,
                'a1': 11,
                'a2': 10
            },
            'g':
            {
                'a0': -1,
                'a1': -1,
                'a2':
                [
                    -1,
                    1,
                    -1,
                    1
                ]
            },
            'f_n0': 1
        },
        'luma':
        {
            'histogram':
            {
                'mean':
                [
                    8,  24,  40,  56,
                    72,  88, 104, 120,
                    136, 152, 168, 184,
                    200, 216, 232, 248
                ],
                'low_index':
                {
                    'start': 0,
                    'end': 1   
                },
                'high_index':
                {
                    'start': 14,
                    'end': 15   
                },
                'back_light_radio_threshold': 0.45
            },
            'ae':
            {
                'weight':
                [
                    1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1
                ]
            }
        }
    })

    c_code = ipa_unit_ian_c(json_obj, 'sc2336', 'ian').get_text()
    print(c_code)
