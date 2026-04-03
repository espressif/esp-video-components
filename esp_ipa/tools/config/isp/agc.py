# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object

class ipa_unit_agc_c(ipa_unit_c):
    @staticmethod
    def decode_agc(name, obj): 
        def light_threshold_config_code(name, obj):
            light_threshold_config_text = str()
            for i in obj.light_threshold_priority:
                light_threshold_config_text += (f'''
                    {{
                        .luma_threshold = {i.luma_threshold},
                        .weight_offset = {i.weight_offset},
                    }},'''
                )
            return light_threshold_config_text

        def get_anti_flicker_param(obj):
            mode_desc_dic = {
                'full': { 'mode': 'ESP_IPA_AGC_ANTI_FLICKER_FULL', 'ac_freq': True },
                'part': { 'mode': 'ESP_IPA_AGC_ANTI_FLICKER_PART', 'ac_freq': True },
                'none': { 'mode': 'ESP_IPA_AGC_ANTI_FLICKER_NONE', 'ac_freq': False },
            }
            ac_freq = 0
            agc_af = obj.anti_flicker

            if agc_af.mode not in mode_desc_dic:
                raise fatal_error(f'Invalid anti_flicker mode: {agc_af.mode}. Expected one of: {list(mode_desc_dic.keys())}')

            mode_desc = mode_desc_dic[agc_af.mode]['mode']
            if mode_desc_dic[agc_af.mode]['ac_freq']:
                ac_freq = agc_af.ac_freq
            
            return (mode_desc, ac_freq)

        def exposure_code(name, obj):
            if not hasattr(obj, 'f_m0'):
                obj.f_m0 = obj.f_n0

            exp = obj.exposure
            gain = obj.gain
            anti_flicker_mode, af_freq = get_anti_flicker_param(obj)
            exposure_text = (f'''
                .exposure_frame_delay = {exp.frame_delay},
                .exposure_adjust_delay = {exp.adjust_delay},
                .gain_frame_delay = {gain.frame_delay},
                .min_gain_step = {gain.min_step},
                .inc_gain_ratio = {obj.f_n0},
                .dec_gain_ratio = {obj.f_m0},
                .anti_flicker_mode = {anti_flicker_mode},
                .ac_freq = {af_freq},
                ''')

            return exposure_text
        
        def luma_code(name, obj):
            luma_weight_text = str()
            luma = obj.luma_adjust
            for i in luma.weight: luma_weight_text += f'{i}, '
            luma_text = (f'''
                .luma_low = {luma.target_low},
                .luma_high = {luma.target_high},
                .luma_target = {luma.target},
                .luma_low_threshold = {luma.low_threshold},
                .luma_low_regions = {luma.low_regions},
                .luma_high_threshold = {luma.high_threshold},
                .luma_high_regions = {luma.high_regions},
                .luma_weight_table = {{
                    {luma_weight_text}
                }},
                '''
            )
            return luma_text
        
        def light_priority_code(name, obj):
            METER_MODE_DICT = {
                'high_light_priority': 'ESP_IPA_AGC_METER_HIGHLIGHT_PRIOR',
                'low_light_priority': 'ESP_IPA_AGC_METER_LOWLIGHT_PRIOR',
                'light_threshold_priority': 'ESP_IPA_AGC_METER_LIGHT_THRESHOLD'
            }

            if obj.mode not in METER_MODE_DICT:
                raise fatal_error(f'Invalid mode: {obj.mode}. Expected one of: {list(METER_MODE_DICT.keys())}')

            # Validate mode matches attribute presence
            if obj.mode == 'light_threshold_priority' and not hasattr(obj, 'light_threshold_priority'):
                raise fatal_error(f'Mode is light_threshold_priority but light_threshold_priority attribute is missing')

            light_priority_text = (f'''
                .meter_mode = {METER_MODE_DICT[obj.mode]},
                '''
            )

            if hasattr(obj, 'high_light_priority'):
                hlp = obj.high_light_priority
                light_priority_text += (f'''
                    .high_light_prior_config = {{
                        .luma_high_threshold = {hlp.high_threshold},
                        .luma_low_threshold = {hlp.low_threshold},
                        .weight_offset = {hlp.weight_offset},
                        .luma_offset = {hlp.luma_offset}
                    }},''')

            if hasattr(obj, 'low_light_priority'):
                llp = obj.low_light_priority
                light_priority_text += (f'''
                    .low_light_prior_config = {{
                        .luma_high_threshold = {llp.high_threshold},
                        .luma_low_threshold = {llp.low_threshold},
                        .weight_offset = {llp.weight_offset},
                        .luma_offset = {llp.luma_offset}
                    }},''')
            
            if hasattr(obj, 'light_threshold_priority'):
                light_threshold_priority_obj_text = (f'''
                    .light_threshold_config = {{
                        .table = s_ipa_agc_meter_light_thresholds_{name},
                        .table_size = ARRAY_SIZE(s_ipa_agc_meter_light_thresholds_{name})
                    }},''')

                light_priority_text += light_threshold_priority_obj_text
        
            return light_priority_text

        agc_code = str()

        if hasattr(obj, 'light_threshold_priority'):
            agc_code += cfmt_string(f'''
                static const esp_ipa_agc_meter_light_threshold_t s_ipa_agc_meter_light_thresholds_{name}[] = {{
                    {light_threshold_config_code(name, obj)}
                }};''')

        agc_code += cfmt_string(f'''
            static const esp_ipa_agc_config_t s_ipa_agc_{name}_config = {{
                {exposure_code(name, obj)}
                {luma_code(name, obj)}
                {light_priority_code(name, obj)}
            }};''')

        return agc_code

    def decode(self, obj):
        self.text = self.decode_agc(self.name, obj)

if __name__ == '__main__':
    json_obj = dict_object({
        'exposure':
        {
            'frame_delay': 2,
            'adjust_delay': 0
        },
        'gain':
        {
            'min_step': 0.0001,
            'frame_delay': 2
        },
        'anti_flicker':
        {
            'mode': 'full',
            'ac_freq': 50
        },
        'f_n0': 1,
        'luma_adjust':
        {
            'target_low': 50,
            'target_high': 150,
            'target': 100,
            'low_threshold': 30,
            'low_regions': 10,
            'high_threshold': 220,
            'high_regions': 10,
            'weight':
            [
                1, 1, 1, 1, 1,
                1, 1, 1, 1, 1,
                1, 1, 1, 1, 1,
                1, 1, 1, 1, 1,
                1, 1, 1, 1, 1
            ]
        },
        'mode': 'high_light_priority',
        'high_light_priority':
        {
            'low_threshold': 120,
            'high_threshold': 180,
            'weight_offset': 5,
            'luma_offset': 5
        },
        'low_light_priority':
        {
            'low_threshold': 40,
            'high_threshold': 80,
            'weight_offset': 10,
            'luma_offset': 10
        },
        'light_threshold_priority':
        [
            {
                'luma_threshold': 60,
                'weight_offset': 5
            },
            {
                'luma_threshold': 80,
                'weight_offset': 10
            },
            {
                'luma_threshold': 100,
                'weight_offset': 15
            },
            {
                'luma_threshold': 120,
                'weight_offset': 20
            },
            {
                'luma_threshold': 140,
                'weight_offset': 25
            }
        ]
    })

    c_code = ipa_unit_agc_c(json_obj, 'agc', 'agc').get_text()
    print(c_code)
