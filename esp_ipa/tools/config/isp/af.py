# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, cfmt_string, dict_object

class ipa_unit_af_c(ipa_unit_c):
    @staticmethod
    def decode_af(name, obj):
        class af_window():
            def __init__(self, left, top, width, height, weight):
                self.top_left_x = left
                self.top_left_y = top
                self.btm_right_x = left + width - 1
                self.btm_right_y = top + height - 1
                self.weight = weight
        if not hasattr(obj, 'max_pos'):
            obj.max_pos = 0
        if not hasattr(obj, 'min_pos'):
            obj.min_pos = 0
        if not hasattr(obj, 'l1_scan_points_num'):
            obj.l1_scan_points_num = 10
        if not hasattr(obj, 'l2_scan_points_num'):
            obj.l2_scan_points_num = 10
        if not hasattr(obj, 'definition_high_threshold_ratio'):
            obj.definition_high_threshold_ratio = 1.5
        if not hasattr(obj, 'definition_low_threshold_ratio'):
            obj.definition_low_threshold_ratio = 0.5
        if not hasattr(obj, 'luminance_high_threshold_ratio'):
            obj.luminance_high_threshold_ratio = 1.5
        if not hasattr(obj, 'luminance_low_threshold_ratio'):
            obj.luminance_low_threshold_ratio = 0.5
        if not hasattr(obj, 'max_change_time'):
            obj.max_change_time = 1000000
        if not hasattr(obj, 'edge_thresh'):
            obj.edge_thresh = 10

        af_windows = list()
        if not hasattr(obj, 'windows'):
            raise fatal_error('AF has no windows')
        else:
            if len(obj.windows) > 3 or len(obj.windows) == 0:
                raise fatal_error('AF windows number must <= 3 and not 0')
            else:
                for w in obj.windows:
                    required = ['left', 'top', 'width', 'height']
                    for attr in required:
                        if not hasattr(w, attr):
                            raise fatal_error(f'AF window missing required attribute: {attr}')
                    if not hasattr(w, 'weight'):
                        w.weight = 1
                    af_windows.append(af_window(w.left, w.top, w.width, w.height, w.weight))

                null_windows = 3 - len(obj.windows)
                for i in range(0, null_windows):
                    af_windows.append(af_window(2, 2, 4, 4, 0))

        af_code = cfmt_string(f'''
            static const esp_ipa_af_config_t s_ipa_af_{name}_config = {{
                .windows = {{
                    [0] = {{
                        .top_left = {{
                            .x = {af_windows[0].top_left_x},
                            .y = {af_windows[0].top_left_y},
                        }},
                        .btm_right = {{
                            .x = {af_windows[0].btm_right_x},
                            .y = {af_windows[0].btm_right_y},
                        }},
                    }},
                    [1] = {{
                        .top_left = {{
                            .x = {af_windows[1].top_left_x},
                            .y = {af_windows[1].top_left_y},
                        }},
                        .btm_right = {{
                            .x = {af_windows[1].btm_right_x},
                            .y = {af_windows[1].btm_right_y},
                        }},
                    }},
                    [2] = {{
                        .top_left = {{
                            .x = {af_windows[2].top_left_x},
                            .y = {af_windows[2].top_left_y},
                        }},
                        .btm_right = {{
                            .x = {af_windows[2].btm_right_x},
                            .y = {af_windows[2].btm_right_y},
                        }},
                    }}
                }},
                .weight_table = {{
                    {af_windows[0].weight}, {af_windows[1].weight}, {af_windows[2].weight}
                }},
                .edge_thresh = {obj.edge_thresh},
                .max_pos = {obj.max_pos},
                .min_pos = {obj.min_pos},
                .l1_scan_points_num = {obj.l1_scan_points_num},
                .l2_scan_points_num = {obj.l2_scan_points_num},
                .definition_high_threshold_ratio = {obj.definition_high_threshold_ratio},
                .definition_low_threshold_ratio = {obj.definition_low_threshold_ratio},
                .luminance_high_threshold_ratio = {obj.luminance_high_threshold_ratio},
                .luminance_low_threshold_ratio = {obj.luminance_low_threshold_ratio},
                .max_change_time = {obj.max_change_time},
            }};
            '''
        )

        return af_code

    def decode(self, obj):
        self.text = self.decode_af(self.name, obj)

if __name__ == '__main__':
    json_obj = dict_object({
        'windows':
        [
            {
                'left': 50,
                'top': 100,
                'width': 100,
                'height': 100,
                'weight': 1
            },
            {
                'left': 150,
                'top': 200,
                'width': 100,
                'height': 100,
                'weight': 10
            },
            {
                'left': 250,
                'top': 300,
                'width': 100,
                'height': 100,
                'weight': 100
            }
        ],
        'edge_thresh': 11,
        'definition_high_threshold_ratio': 1.6,
        'definition_low_threshold_ratio': 0.6,
        'luminance_high_threshold_ratio': 1.6,
        'luminance_low_threshold_ratio': 0.6,
        'l1_scan_points_num': 11,
        'l2_scan_points_num': 12,
        'max_pos': 500,
        'max_change_time': 500
    })

    c_code = ipa_unit_af_c(json_obj, 'sc2336', 'af').get_text()
    print(c_code)
