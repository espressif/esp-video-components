# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object


class ipa_unit_dpc_c(ipa_unit_c):
    @staticmethod
    def unit_float(value, name):
        if value < 0 or value > 1:
            raise fatal_error(f'{name} must be in range [0, 1], got {value}')
        return float(value)

    @staticmethod
    def decode_dpc(name, obj):
        def model_code(obj):
            if not hasattr(obj, 'model'):
                obj.model = 0

            model_lut = {
                0: 'ESP_IPA_DPC_MODEL_0',
                1: 'ESP_IPA_DPC_MODEL_1',
                '0': 'ESP_IPA_DPC_MODEL_0',
                '1': 'ESP_IPA_DPC_MODEL_1',
                'nearest': 'ESP_IPA_DPC_MODEL_0',
                'linear': 'ESP_IPA_DPC_MODEL_1',
                'model_0': 'ESP_IPA_DPC_MODEL_0',
                'model_1': 'ESP_IPA_DPC_MODEL_1',
            }
            if obj.model not in model_lut:
                raise fatal_error(f'Invalid dpc model: {obj.model}. Expected 0 or 1')
            return model_lut[obj.model]

        def method_code(method):
            method_lut = {
                1: 'ESP_IPA_DPC_DYNAMIC_METHOD_1',
                2: 'ESP_IPA_DPC_DYNAMIC_METHOD_2',
                '1': 'ESP_IPA_DPC_DYNAMIC_METHOD_1',
                '2': 'ESP_IPA_DPC_DYNAMIC_METHOD_2',
                'method_1': 'ESP_IPA_DPC_DYNAMIC_METHOD_1',
                'method_2': 'ESP_IPA_DPC_DYNAMIC_METHOD_2',
            }
            if method not in method_lut:
                raise fatal_error(f'Invalid dpc method: {method}. Expected 1 or 2')
            return method_lut[method]

        def param_code(param):
            method = getattr(param, 'method', 1)
            method_enum = method_code(method)

            if method_enum == 'ESP_IPA_DPC_DYNAMIC_METHOD_1':
                if not hasattr(param, 'high_threshold') or not hasattr(param, 'low_threshold'):
                    raise fatal_error('method 1 requires high_threshold and low_threshold')
                return f'''
                    .method = {method_enum},
                    .method_1 = {{
                        .high_threshold = {int(param.high_threshold)},
                        .low_threshold = {int(param.low_threshold)},
                    }}'''

            required = (
                'first_stage_upper_ratio',
                'first_stage_lower_ratio',
                'bright_deviation_factor',
                'dark_deviation_factor',
            )
            for key in required:
                if not hasattr(param, key):
                    raise fatal_error(f'method 2 requires {key}')

            upper = ipa_unit_dpc_c.unit_float(param.first_stage_upper_ratio, 'first_stage_upper_ratio')
            lower = ipa_unit_dpc_c.unit_float(param.first_stage_lower_ratio, 'first_stage_lower_ratio')
            if upper <= lower:
                raise fatal_error('first_stage_upper_ratio must be greater than first_stage_lower_ratio')
            bright = ipa_unit_dpc_c.unit_float(param.bright_deviation_factor, 'bright_deviation_factor')
            dark = ipa_unit_dpc_c.unit_float(param.dark_deviation_factor, 'dark_deviation_factor')

            return f'''
                .method = {method_enum},
                .method_2 = {{
                    .first_stage_upper_ratio = {upper}f,
                    .first_stage_lower_ratio = {lower}f,
                    .bright_deviation_factor = {bright}f,
                    .dark_deviation_factor = {dark}f,
                }}'''

        def table_code(obj):
            text = str()
            for i in obj.table:
                param = i.param if hasattr(i, 'param') else i
                text += (f'''
                    {{
                        .gain = {int(i.gain * 1000)},
                        .dpc = {{
                            {param_code(param)}
                        }}
                    }},''')
            return text

        if not hasattr(obj, 'table') or len(obj.table) == 0:
            raise fatal_error('dpc.table must contain at least one entry')

        debounce_gain = getattr(obj, 'debounce_gain', 0)
        dpc_code = cfmt_string(f'''
            static const esp_ipa_dpc_unit_t s_ipa_dpc_table_{name}_config[] = {{
                {table_code(obj)}
            }};
            ''')

        dpc_code += cfmt_string(f'''
            static const esp_ipa_dpc_config_t s_ipa_dpc_{name}_config = {{
                .model = {model_code(obj)},
                .debounce_gain = {int(float(debounce_gain) * 1000)},
                .table = s_ipa_dpc_table_{name}_config,
                .table_size = ARRAY_SIZE(s_ipa_dpc_table_{name}_config),
            }};
            ''')

        return dpc_code

    def decode(self, obj):
        self.text = self.decode_dpc(self.name, obj)


if __name__ == '__main__':
    json_obj = dict_object({
        'model': 0,
        'debounce_gain': 0.1,
        'table': [
            {
                'gain': 1.0,
                'param': {
                    'method': 1,
                    'high_threshold': 8,
                    'low_threshold': 8,
                }
            },
            {
                'gain': 4.0,
                'param': {
                    'method': 1,
                    'high_threshold': 16,
                    'low_threshold': 16,
                }
            },
            {
                'gain': 8.0,
                'param': {
                    'method': 2,
                    'first_stage_upper_ratio': 1.0,
                    'first_stage_lower_ratio': 0.5,
                    'bright_deviation_factor': 0.5,
                    'dark_deviation_factor': 0.5,
                }
            }
        ]
    })

    c_code = ipa_unit_dpc_c(json_obj, 'sc2336', 'dpc').get_text()
    print(c_code)
