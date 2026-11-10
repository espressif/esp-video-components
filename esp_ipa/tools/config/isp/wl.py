# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, cfmt_string, dict_object, fatal_error

WL_DEFAULTS = {
    'enable': True,
    'verify_ratio': 1.5,
    'k_tolerance': 0.1,
    'enter_frames': 30,
    'exit_frames': 30,
    'settle_frames': 5,
    'settle_max_frames': 60,
    'settle_tolerance': 0.02,
    'probe_period_s': 600,
    'enable_log': False,
}

WL_LIGHT_DEFAULTS = {
    'ct': 0,
}

WL_GATE_DEFAULTS = {
    'enable': False,
    'min_gain_of_max': 0.8,
    'min_exposure_of_max': 0.9,
}

def cbool(v):
    return 'true' if v else 'false'

def cfloat(v):
    return f'{float(v)}f'

def fill_defaults(obj, defaults):
    for k, v in defaults.items():
        if not hasattr(obj, k):
            setattr(obj, k, v)

UINT16_MAX = 0xFFFF
UINT32_MAX = 0xFFFFFFFF

def check_bool(path, v):
    if not isinstance(v, bool):
        raise fatal_error(f'IPA wl configuration "{path}" must be true or false, got {v!r}')

def check_number(path, v):
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        raise fatal_error(f'IPA wl configuration "{path}" must be a number, got {v!r}')

def check_int(path, v, lo, hi):
    if isinstance(v, bool) or not isinstance(v, int) or v < lo or v > hi:
        raise fatal_error(f'IPA wl configuration "{path}" must be an integer in [{lo}, {hi}], got {v!r}')

def check_fraction(path, v, hi_open):
    check_number(path, v)
    if v < 0 or (v >= 1 if hi_open else v > 1):
        raise fatal_error(f'IPA wl configuration "{path}" must be in [0, 1{")" if hi_open else "]"}, got {v!r}')

def check_wl(obj):
    """Same checks as wl_check_config() in esp_ipa_wl.c, plus the types and the C type ranges"""
    l = obj.light
    g = obj.gate

    for k in ('enable', 'enable_log'):
        check_bool(k, getattr(obj, k))
    check_bool('gate.enable', g.enable)

    for k in ('enter_luma', 'exit_luma', 'verify_ratio'):
        check_number(k, getattr(obj, k))
    if obj.enter_luma <= 0 or obj.exit_luma <= obj.enter_luma:
        raise fatal_error(f'IPA wl configuration requires 0 < enter_luma < exit_luma, '
                          f'got enter_luma={obj.enter_luma}, exit_luma={obj.exit_luma}')
    if obj.verify_ratio < 1:
        raise fatal_error(f'IPA wl configuration "verify_ratio" must be >= 1, got {obj.verify_ratio}')

    check_fraction('k_tolerance', obj.k_tolerance, True)
    check_fraction('settle_tolerance', obj.settle_tolerance, True)
    check_fraction('gate.min_gain_of_max', g.min_gain_of_max, False)
    check_fraction('gate.min_exposure_of_max', g.min_exposure_of_max, False)

    for k in ('enter_frames', 'exit_frames', 'settle_max_frames'):
        check_int(k, getattr(obj, k), 1, UINT16_MAX)
    check_int('settle_frames', obj.settle_frames, 0, UINT16_MAX)
    check_int('probe_period_s', obj.probe_period_s, 0, UINT32_MAX)
    check_int('light.ct', l.ct, 0, UINT32_MAX)

class ipa_unit_wl_c(ipa_unit_c):
    @staticmethod
    def decode_wl(name, obj):
        for k in ('enter_luma', 'exit_luma'):
            if not hasattr(obj, k):
                raise fatal_error(f'IPA wl configuration "{k}" is required')

        fill_defaults(obj, WL_DEFAULTS)
        if not hasattr(obj, 'light'):
            obj.light = dict_object({})
        fill_defaults(obj.light, WL_LIGHT_DEFAULTS)
        if not hasattr(obj, 'gate'):
            obj.gate = dict_object({})
        fill_defaults(obj.gate, WL_GATE_DEFAULTS)

        check_wl(obj)

        l = obj.light
        g = obj.gate

        return cfmt_string(f'''
            static const esp_ipa_wl_config_t s_ipa_wl_{name}_config = {{
                .enable = {cbool(obj.enable)},
                .enter_luma = {cfloat(obj.enter_luma)},
                .exit_luma = {cfloat(obj.exit_luma)},
                .verify_ratio = {cfloat(obj.verify_ratio)},
                .k_tolerance = {cfloat(obj.k_tolerance)},
                .enter_frames = {int(obj.enter_frames)},
                .exit_frames = {int(obj.exit_frames)},
                .settle_frames = {int(obj.settle_frames)},
                .settle_max_frames = {int(obj.settle_max_frames)},
                .settle_tolerance = {cfloat(obj.settle_tolerance)},
                .probe_period_s = {int(obj.probe_period_s)},
                .light = {{
                    .ct = {int(l.ct)},
                }},
                .gate = {{
                    .enable = {cbool(g.enable)},
                    .min_gain_of_max = {cfloat(g.min_gain_of_max)},
                    .min_exposure_of_max = {cfloat(g.min_exposure_of_max)},
                }},
                .enable_log = {cbool(obj.enable_log)},
            }};
            ''')

    def decode(self, obj):
        self.text = self.decode_wl(self.name, obj)


if __name__ == '__main__':
    json_obj = dict_object({
        'enter_luma': 4.0,
        'exit_luma': 16.0,
        'light': {
            'ct': 5000
        },
        'enable_log': True
    })

    c_code = ipa_unit_wl_c(json_obj, 'sc2337p', 'wl').get_text()
    print(c_code)
