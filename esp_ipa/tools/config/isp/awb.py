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
    if not hasattr(obj, 'red_gain_scale'):
        obj.red_gain_scale = 1.0
    if not hasattr(obj, 'blue_gain_scale'):
        obj.blue_gain_scale = 1.0
    if not hasattr(obj, 'enable_sub_win'):
        obj.enable_sub_win = False
    if not hasattr(obj, 'min_subwin_wp_counted'):
        obj.min_subwin_wp_counted = 0
    if not hasattr(obj, 'min_subwin_participated'):
        obj.min_subwin_participated = 0
    if not hasattr(obj, 'new_w'):
        obj.new_w = 0.3
    if not hasattr(obj, 'prev_w'):
        obj.prev_w = 0.7
    if not hasattr(obj, 'export_ct'):
        obj.export_ct = False
    # Model_2 temporal stabilization knobs (0.0 disables each feature)
    if not hasattr(obj, 'outlier_rg'):
        obj.outlier_rg = 0.0
    if not hasattr(obj, 'outlier_bg'):
        obj.outlier_bg = 0.0
    if not hasattr(obj, 'zone_hysteresis_ratio'):
        obj.zone_hysteresis_ratio = 0.0
    if not hasattr(obj, 'zone_switch_count'):
        obj.zone_switch_count = 0
    if not hasattr(obj, 'type_counter_max'):
        obj.type_counter_max = 20000

# Map enum-name strings (e.g. "uhct") to C enum identifiers. Accepts int too.
_ZONE_TYPE_C = {
    'uhct':  'ESP_IPA_AWB_ZONE_UHCT',
    'hct':   'ESP_IPA_AWB_ZONE_HCT',
    'mct':   'ESP_IPA_AWB_ZONE_MCT',
    'lct':   'ESP_IPA_AWB_ZONE_LCT',
    'ulct':  'ESP_IPA_AWB_ZONE_ULCT',
    'green': 'ESP_IPA_AWB_ZONE_GREEN',
    'skin':  'ESP_IPA_AWB_ZONE_SKIN',
}

def _awb_zone_type_to_c(val):
    if isinstance(val, str):
        key = val.strip().lower()
        if key in _ZONE_TYPE_C:
            return _ZONE_TYPE_C[key]
        raise fatal_error(f'AWB zone type "{val}" is not recognised; expected one of {list(_ZONE_TYPE_C.keys())}')
    if isinstance(val, int):
        return f'(esp_ipa_awb_zone_type_t){int(val)}'
    raise fatal_error(f'AWB zone type must be string or int, got {type(val).__name__}')

def _awb_collect_zones(obj):
    """Return list of zones. Prefer awb.zones; fall back to awb.hybrid.ct2.zones for backward compat."""
    if hasattr(obj, 'zones') and obj.zones is not None:
        return list(obj.zones)
    if hasattr(obj, 'hybrid') and obj.hybrid is not None:
        h = obj.hybrid
        if hasattr(h, 'ct2') and h.ct2 is not None and hasattr(h.ct2, 'zones') and h.ct2.zones is not None:
            return list(h.ct2.zones)
    return []

def _awb_collect_ref_points(obj):
    """Return list of ref_points. Prefer awb.ref_points; fall back to awb.hybrid.ref_points."""
    if hasattr(obj, 'ref_points') and obj.ref_points is not None:
        return list(obj.ref_points)
    if hasattr(obj, 'hybrid') and obj.hybrid is not None:
        h = obj.hybrid
        if hasattr(h, 'ref_points') and h.ref_points is not None:
            return list(h.ref_points)
    return []

def _awb_collect_smooth_weights(obj):
    """Return (new_w, prev_w). Prefer awb.new_w/prev_w; fall back to awb.hybrid.ct2.new_w/prev_w."""
    new_w, prev_w = obj.new_w, obj.prev_w
    if hasattr(obj, 'hybrid') and obj.hybrid is not None:
        h = obj.hybrid
        if hasattr(h, 'ct2') and h.ct2 is not None:
            c = h.ct2
            if hasattr(c, 'new_w'):
                new_w = c.new_w
            if hasattr(c, 'prev_w'):
                prev_w = c.prev_w
    return float(new_w), float(prev_w)

def _awb_render_zones(name, zones):
    """Render the zones C array literal + table variable. Returns (decl_text, ptr_expr, count)."""
    if not zones:
        return '', 'NULL', 0
    items = []
    for idx, z in enumerate(zones):
        if not hasattr(z, 'type'):
            raise fatal_error(f'AWB zone #{idx} in {name} has no "type"')
        ztype = _awb_zone_type_to_c(z.type)
        if not (hasattr(z, 'rg') and hasattr(z.rg, 'min') and hasattr(z.rg, 'max')):
            raise fatal_error(f'AWB zone #{idx} in {name} missing rg.min/max')
        if not (hasattr(z, 'bg') and hasattr(z.bg, 'min') and hasattr(z.bg, 'max')):
            raise fatal_error(f'AWB zone #{idx} in {name} missing bg.min/max')
        enabled = getattr(z, 'enabled', True)
        items.append(
            f'    {{ .type = {ztype}, '
            f'.rg_min = {float(z.rg.min):.6f}f, .rg_max = {float(z.rg.max):.6f}f, '
            f'.bg_min = {float(z.bg.min):.6f}f, .bg_max = {float(z.bg.max):.6f}f, '
            f'.enabled = {str(bool(enabled)).lower()} }},'
        )
    decl = (
        f'static const esp_ipa_awb_zone_t s_ipa_awb_{name}_zones[] = {{\n'
        + '\n'.join(items)
        + '\n};\n'
    )
    return decl, f's_ipa_awb_{name}_zones', len(zones)

def _awb_render_ref_points(name, pts):
    if not pts:
        return '', 'NULL', 0
    items = []
    for idx, p in enumerate(pts):
        if not hasattr(p, 'ct') or not hasattr(p, 'rg') or not hasattr(p, 'bg'):
            raise fatal_error(f'AWB ref_point #{idx} in {name} missing ct/rg/bg')
        radius = getattr(p, 'radius', 0.0)
        items.append(
            f'    {{ .ct = {int(p.ct)}, '
            f'.rg = {float(p.rg):.6f}f, .bg = {float(p.bg):.6f}f, '
            f'.radius = {float(radius):.6f}f }},'
        )
    decl = (
        f'static const esp_ipa_awb_ct_point_t s_ipa_awb_{name}_ref_points[] = {{\n'
        + '\n'.join(items)
        + '\n};\n'
    )
    return decl, f's_ipa_awb_{name}_ref_points', len(pts)

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
            1: 'ESP_IPA_AWB_MODEL_1',
            2: 'ESP_IPA_AWB_MODEL_2',
        }

        # Accept string aliases too; 'zone'/'hybrid' both map to the new classifier (model 2).
        model_val = obj.model
        if isinstance(model_val, str):
            alias = model_val.strip().lower()
            str_to_int = {
                'gray_world': 0, 'model_0': 0, 'gw': 0,
                'ct_index':   1, 'model_1': 1,
                'zone': 2, 'model_2': 2, 'hybrid': 2, 'ct2': 2,
            }
            if alias not in str_to_int:
                raise fatal_error(
                    f'AWB config {name} has unknown model string: "{obj.model}". '
                    f'Expected int 0/1/2 or one of {list(str_to_int.keys())}.')
            model_val = str_to_int[alias]

        if model_val not in model_dict:
            raise fatal_error(
                f'AWB config {name} has invalid model value: {obj.model}. Expected 0, 1 or 2.')

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

        # Model 2 extras: zones + ref_points + smoothing weights + CT export flag.
        zones = _awb_collect_zones(obj)
        refs = _awb_collect_ref_points(obj)
        new_w, prev_w = _awb_collect_smooth_weights(obj)
        export_ct = bool(getattr(obj, 'export_ct', False))
        zones_decl, zones_ptr, zones_cnt = _awb_render_zones(name, zones)
        refs_decl, refs_ptr, refs_cnt = _awb_render_ref_points(name, refs)

        if model_val == 2 and zones_cnt == 0:
            raise fatal_error(
                f'AWB config {name} uses model 2 (zone) but no zones were provided '
                f'(expected awb.zones[] or awb.hybrid.ct2.zones[]).')

        prefix = (zones_decl + ('\n' if zones_decl else '') + refs_decl + ('\n' if refs_decl else ''))

        config_text = cfmt_string(f'''
            {prefix}static const esp_ipa_awb_config_t s_ipa_awb_{name}_config = {{
                .model = {model_dict[model_val]},
                .min_counted = {obj.min_counted},
                .min_red_gain_step = {obj.min_red_gain_step},
                .min_blue_gain_step = {obj.min_blue_gain_step},
                .red_gain_scale = {float(obj.red_gain_scale):.6f}f,
                .blue_gain_scale = {float(obj.blue_gain_scale):.6f}f,
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
                .subwin_green_bright = {lbright},
                .zones = {zones_ptr},
                .zones_count = {zones_cnt},
                .ref_points = {refs_ptr},
                .ref_points_count = {refs_cnt},
                .new_w = {float(new_w):.6f}f,
                .prev_w = {float(prev_w):.6f}f,
                .export_ct = {str(export_ct).lower()},
                .outlier_rg = {float(obj.outlier_rg):.6f}f,
                .outlier_bg = {float(obj.outlier_bg):.6f}f,
                .zone_hysteresis_ratio = {float(obj.zone_hysteresis_ratio):.6f}f,
                .zone_switch_count = {int(obj.zone_switch_count)},
                .type_counter_max = {int(obj.type_counter_max)}
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
