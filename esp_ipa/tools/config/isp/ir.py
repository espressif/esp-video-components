# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object


class ipa_unit_ir_c(ipa_unit_c):
    def __init__(self, obj, name, unit_type, sensor=None):
        self.sensor = sensor
        super().__init__(obj, name, unit_type)

    @staticmethod
    def decode_ir(name, obj, sensor=None):
        del sensor

        if not hasattr(obj, 'model'):
            obj.model = 0
        elif obj.model not in (0, '0'):
            raise fatal_error(f'Invalid ir.model: {obj.model}. Expected 0')

        if obj.model in (0, '0'):
            if not hasattr(obj, 'model0_param'):
                raise fatal_error('ir.model0_param is required for model 0')
            param = obj.model0_param
        else:
            raise fatal_error(f'Invalid ir.model: {obj.model}. Expected 0')

        # A missing key uses IAN smoothed luma. JSON null and the string "NULL"
        # stay C NULL, and IR then has no luma for that frame.
        if not hasattr(param, 'luma_env'):
            param.luma_env = 'env.luma.avg'

        if not hasattr(param, 'night_ir_factor') or param.night_ir_factor is None:
            param.night_ir_factor = 1.0

        if not hasattr(param, 'enable_log'):
            param.enable_log = False

        if not hasattr(param, 'day_luma_threshold'):
            raise fatal_error('ir.model0_param.day_luma_threshold is required')
        if not hasattr(param, 'day_switch_night_time_us') or param.day_switch_night_time_us is None:
            param.day_switch_night_time_us = 0
        if not hasattr(param, 'day_keep_time_us'):
            raise fatal_error('ir.model0_param.day_keep_time_us is required')

        if not hasattr(param, 'night_luma_threshold'):
            raise fatal_error('ir.model0_param.night_luma_threshold is required')
        if not hasattr(param, 'night_switch_day_time_us') or param.night_switch_day_time_us is None:
            param.night_switch_day_time_us = 0
        if not hasattr(param, 'night_keep_time_us'):
            raise fatal_error('ir.model0_param.night_keep_time_us is required')

        if not hasattr(param, 'night_awb'):
            raise fatal_error('ir.model0_param.night_awb is required')
        if not hasattr(param, 'night_ccm'):
            raise fatal_error('ir.model0_param.night_ccm is required')
        if not hasattr(param, 'night_red_gain'):
            raise fatal_error('ir.model0_param.night_red_gain is required')
        if not hasattr(param, 'night_blue_gain'):
            raise fatal_error('ir.model0_param.night_blue_gain is required')

        awb = param.night_awb
        for group in ('green', 'rg', 'bg'):
            if not hasattr(awb, group):
                raise fatal_error(f'ir.model0_param.night_awb.{group} is required')
        ccm = param.night_ccm
        if not hasattr(ccm, 'matrix') or len(ccm.matrix) != 9:
            raise fatal_error('ir.model0_param.night_ccm.matrix must have exactly 9 elements')
        m = ccm.matrix

        if param.luma_env is None or param.luma_env == 'NULL':
            luma_env_rhs = 'NULL'
        else:
            luma_env_rhs = f'"{param.luma_env}"'

        enable_log = 'true' if param.enable_log else 'false'

        ir_code = cfmt_string(f'''
            static esp_ipa_ir_model0_config_t s_ipa_ir_{name}_model0_config = {{
                .luma_env = {luma_env_rhs},
                .day_luma_threshold = {param.day_luma_threshold},
                .day_switch_night_time_us = {param.day_switch_night_time_us},
                .day_keep_time_us = {param.day_keep_time_us},
                .night_ir_factor = {param.night_ir_factor},
                .night_luma_threshold = {param.night_luma_threshold},
                .night_switch_day_time_us = {param.night_switch_day_time_us},
                .night_keep_time_us = {param.night_keep_time_us},
                .night_awb = {{
                    .green_max = {awb.green.max},
                    .green_min = {awb.green.min},
                    .rg_max = {awb.rg.max},
                    .rg_min = {awb.rg.min},
                    .bg_max = {awb.bg.max},
                    .bg_min = {awb.bg.min}
                }},
                .night_ccm = {{
                    .matrix = {{
                        {{ {m[0]}, {m[1]}, {m[2]} }},
                        {{ {m[3]}, {m[4]}, {m[5]} }},
                        {{ {m[6]}, {m[7]}, {m[8]} }}
                    }}
                }},
                .night_red_gain = {param.night_red_gain},
                .night_blue_gain = {param.night_blue_gain},
                .enable_log = {enable_log}
            }};
            static const esp_ipa_ir_config_t s_ipa_ir_{name}_config = {{
                .model = ESP_IPA_IR_MODEL_{obj.model},
                .model0_param = &s_ipa_ir_{name}_model0_config
            }};
            ''')

        return ir_code

    def decode(self, obj):
        self.text = self.decode_ir(self.name, obj, self.sensor)


if __name__ == '__main__':
    json_obj = dict_object({
        'model': 0,
        'model0_param': {
            'luma_env': 'env.luma.avg',
            'day_luma_threshold': 0.5,
            'day_switch_night_time_us': 500000,
            'day_keep_time_us': 2000000,
            'night_ir_factor': 10.0,
            'night_luma_threshold': 0.8,
            'night_switch_day_time_us': 500000,
            'night_keep_time_us': 3000000,
            'night_awb': {
                'green': {'max': 255, 'min': 0},
                'rg': {'max': 1.5, 'min': 0.5},
                'bg': {'max': 1.5, 'min': 0.5}
            },
            'night_ccm': {
                'matrix': [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
            },
            'night_red_gain': 1.0,
            'night_blue_gain': 1.0
        }
    })

    c_code = ipa_unit_ir_c(json_obj, 'sc2336', 'ir').get_text()
    print(c_code)
