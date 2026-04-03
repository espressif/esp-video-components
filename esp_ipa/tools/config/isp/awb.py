# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, base_default, fatal_error, cfmt_string, dict_object

class awb_range_default(base_default):
    def __init__(self, name):
        self.green = base_default()
        self.rg = base_default()
        self.bg = base_default()

        self.green.max = 220
        self.green.min = 180
        self.rg.max = 0.8899
        self.rg.min = 0.5040
        self.bg.max = 0.7822
        self.bg.min = 0.4838

def awb_init_default(name, obj):
    if not hasattr(obj, 'model'):
        obj.model = 0
    if not hasattr(obj, 'min_counted'):
        obj.min_counted = 1000
    if not hasattr(obj, 'range'):
        obj.range = awb_range_default(name)
    if not hasattr(obj, 'green_luma_env'):
        obj.green_luma_env = 'ae.luma.avg'
    if not hasattr(obj, 'green_luma_init'):
        obj.green_luma_init = 200
    if not hasattr(obj, 'green_luma_step_ratio'):
        obj.green_luma_step_ratio = 0.3
    if not hasattr(obj, 'min_red_gain_step'):
        obj.min_red_gain_step = 0.5
    if not hasattr(obj, 'min_blue_gain_step'):
        obj.min_blue_gain_step = 0.5
    if not hasattr(obj, 'enable_sub_win'):
        obj.enable_sub_win = False
    if not hasattr(obj, 'min_subwin_wp_counted'):
        obj.min_subwin_wp_counted = 0
    if not hasattr(obj, 'min_subwin_participated'):
        obj.min_subwin_participated = 0

def _awb_resolve_subwin_weight(name, obj):
    """Same layout as ian.luma.ae.weight: prefer awb.sub_win.weight (25 numbers, int or float)."""
    if hasattr(obj, 'sub_win') and obj.sub_win is not None:
        sub = obj.sub_win
        if hasattr(sub, 'weight') and sub.weight is not None:
            w = [float(x) for x in list(sub.weight)]
            if len(w) != 25:
                raise fatal_error(
                    f'AWB config {name} sub_win.weight must have 25 elements (like ian.luma.ae.weight), got {len(w)}')
            return w
    # 未配置时默认全 1.0，生成 C 时为 256（24.8 定点）
    return [1.0] * 25

def _awb_resolve_min_subwin_counted(obj):
    if hasattr(obj, 'sub_win') and obj.sub_win is not None:
        sub = obj.sub_win
        if hasattr(sub, 'min_counted'):
            return int(sub.min_counted)
    return int(obj.min_subwin_wp_counted)

def _awb_resolve_min_subwin_participated(obj):
    """Min number of sub-window cells that must participate; 0 = no limit. Prefer sub_win.min_participated."""
    if hasattr(obj, 'sub_win') and obj.sub_win is not None:
        sub = obj.sub_win
        if hasattr(sub, 'min_participated'):
            return int(sub.min_participated)
    return int(obj.min_subwin_participated)

def _awb_subwin_green_three(obj):
    """Three values: dark / mid / bright (cell mean green). Reject < dark and > bright; linear weight peak at mid."""
    dark, mid, bright = 0, 100, 200
    if hasattr(obj, 'sub_win') and obj.sub_win is not None:
        s = obj.sub_win
        gr = getattr(s, 'green', None) or getattr(s, 'luma', None)  # prefer green, accept luma for backward compat
        if gr is not None:
            dark = int(getattr(gr, 'dark', 0))
            mid = int(getattr(gr, 'mid', 100))
            bright = int(getattr(gr, 'bright', 200))
    if dark > mid:
        mid = dark
    if mid > bright:
        bright = mid
    return dark, mid, bright

class ipa_unit_awb_c(ipa_unit_c):
    @staticmethod
    def decode_awb(name, obj):
        awb_init_default(name, obj)

        model_dict = {
            0: 'ESP_IPA_AWB_MODEL_0',
            1: 'ESP_IPA_AWB_MODEL_1'
        }

        if obj.model not in model_dict:
            raise fatal_error(f'AWB config {name} has invalid model value: {obj.model}. Expected 0 or 1.')

        subwin_table = _awb_resolve_subwin_weight(name, obj)
        min_subwin = _awb_resolve_min_subwin_counted(obj)
        min_participated = _awb_resolve_min_subwin_participated(obj)
        ldark, lmid, lbright = _awb_subwin_green_three(obj)

        # subwin_weight 2D [Y_NUM][X_NUM], direct float; row-major from JSON
        n_cols = 5  # ISP_AWB_WINDOW_X_NUM
        rows = [subwin_table[i:i + n_cols] for i in range(0, len(subwin_table), n_cols)]
        subwin_w = '\n                    '.join(
            '{ ' + ', '.join(f'{float(w):.6f}f' for w in row) + ' },'
            for row in rows
        )

        config_text = cfmt_string(f'''
            static const esp_ipa_awb_config_t s_ipa_awb_{name}_config = {{
                .model = {model_dict[obj.model]},
                .min_counted = {obj.min_counted},
                .min_red_gain_step = {obj.min_red_gain_step},
                .min_blue_gain_step = {obj.min_blue_gain_step},
                .range = {{
                    .green_max = {obj.range.green.max},
                    .green_min = {obj.range.green.min},
                    .rg_max = {obj.range.rg.max},
                    .rg_min = {obj.range.rg.min},
                    .bg_max = {obj.range.bg.max},
                    .bg_min = {obj.range.bg.min}
                }},
                .green_luma_env = \"{obj.green_luma_env}\",
                .green_luma_init = {obj.green_luma_init},
                .green_luma_step_ratio = {obj.green_luma_step_ratio},
                .enable_sub_win = {str(obj.enable_sub_win).lower()},
                .min_subwin_wp_counted = {min_subwin},
                .min_subwin_participated = {min_participated},
                .subwin_weight = {{
                    {subwin_w}
                }},
                .subwin_green_dark = {ldark},
                .subwin_green_mid = {lmid},
                .subwin_green_bright = {lbright}
            }};
            ''')

        return config_text

    def decode(self, obj):
        self.text = self.decode_awb(self.name, obj)

if __name__ == '__main__':
    json_obj = dict_object({
        'model': 0,
        'min_red_gain_step': 0.5,
        'min_blue_gain_step': 0.5,
        'min_counted': 1000,
        'range':
        {
            'green':
            {
                'max': 200,
                'min': 180
            },
            'rg':
            {
                'max': 1.2,
                'min': 0.8
            },
            'bg':
            {
                'max': 1.2,
                'min': 0.8
            }
        },
        'green_luma_env': 'dummy_awb_luma',
        'green_luma_init': 200,
        'green_luma_step_ratio': 0.3
    })

    c_code = ipa_unit_awb_c(json_obj, 'sc2336', 'awb').get_text()
    print(c_code)
