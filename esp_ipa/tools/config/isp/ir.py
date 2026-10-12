# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object

class ipa_unit_ir_c(ipa_unit_c):
    @staticmethod
    def decode_ir(name, obj):
        if not hasattr(obj, 'model'):
            obj.model = 0
        elif obj.model not in (0, '0'):
            raise fatal_error(f'Invalid ir.model: {obj.model}. Expected 0')

        if not hasattr(obj, 'luma_env'):
            obj.luma_env = None

        if not hasattr(obj, 'k'):
            obj.k = 1.0
        elif obj.k == 0:
            raise fatal_error('ir.k must not be 0')

        if not hasattr(obj, 'ir_factor'):
            obj.ir_factor = 1.0

        if not hasattr(obj, 'enter_delay_us'):
            obj.enter_delay_us = 0

        if not hasattr(obj, 'exit_delay_us'):
            obj.exit_delay_us = 0

        if not hasattr(obj, 'enable_log'):
            obj.enable_log = False

        if not hasattr(obj, 'weight'):
            obj.weight = [1] * 25
        elif len(obj.weight) != 25:
            raise fatal_error(f'ir.weight size must be 25, but got {len(obj.weight)}')

        if not hasattr(obj, 'normal'):
            raise fatal_error('ir.normal is required')
        if not hasattr(obj, 'ir_mode'):
            raise fatal_error('ir.ir_mode is required')

        def detect_fields(d):
            if not hasattr(d, 'luma_threshold'):
                raise fatal_error('IR detect config requires luma_threshold')
            if not hasattr(d, 'awb_threshold'):
                raise fatal_error('IR detect config requires awb_threshold')
            if not hasattr(d, 'hold_time_us'):
                raise fatal_error('IR detect config requires hold_time_us')
            return d

        normal = detect_fields(obj.normal)
        ir_mode = detect_fields(obj.ir_mode)

        if obj.luma_env is None or obj.luma_env == 'NULL':
            luma_env_rhs = 'NULL'
        else:
            luma_env_rhs = f'"{obj.luma_env}"'

        enable_log = 'true' if obj.enable_log else 'false'

        weight_text = ''
        for w in obj.weight:
            weight_text += f'{w},'

        ir_code = cfmt_string(f'''
            static const esp_ipa_ir_config_t s_ipa_ir_{name}_config = {{
                .model = ESP_IPA_IR_MODEL_{obj.model},
                .luma_env = {luma_env_rhs},
                .k = {obj.k},
                .weight = {{
                    {weight_text}
                }},
                .ir_factor = {obj.ir_factor},
                .enter_delay_us = {obj.enter_delay_us},
                .exit_delay_us = {obj.exit_delay_us},
                .normal = {{
                    .luma_threshold = {normal.luma_threshold},
                    .awb_threshold = {normal.awb_threshold},
                    .hold_time_us = {normal.hold_time_us}
                }},
                .ir_mode = {{
                    .luma_threshold = {ir_mode.luma_threshold},
                    .awb_threshold = {ir_mode.awb_threshold},
                    .hold_time_us = {ir_mode.hold_time_us}
                }},
                .enable_log = {enable_log}
            }};
            ''')

        return ir_code

    def decode(self, obj):
        self.text = self.decode_ir(self.name, obj)


if __name__ == '__main__':
    json_obj = dict_object({
        'model': 0,
        'luma_env': 'env.luma.avg',
        'k': 1000000,
        'ir_factor': 10.0,
        'enter_delay_us': 500000,
        'exit_delay_us': 500000,
        'weight': [1] * 25,
        'normal': {
            'luma_threshold': 0.5,
            'awb_threshold': 1.0,
            'hold_time_us': 2000000
        },
        'ir_mode': {
            'luma_threshold': 0.8,
            'awb_threshold': 0.8,
            'hold_time_us': 3000000
        }
    })

    c_code = ipa_unit_ir_c(json_obj, 'sc2336', 'ir').get_text()
    print(c_code)
