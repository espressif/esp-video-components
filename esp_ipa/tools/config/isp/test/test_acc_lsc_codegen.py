# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import os
import re
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from acc import ipa_unit_acc_c, fatal_error  # noqa: E402
from common import dict_object  # noqa: E402


def _gen_acc(name, acc_obj):
    return ipa_unit_acc_c(dict_object(acc_obj), name, 'acc').get_text()


def _ct_node(ct, r, gr, gb, b):
    return {
        'ct': ct,
        'calibrations_r_tbl': r,
        'calibrations_gr_tbl': gr,
        'calibrations_gb_tbl': gb,
        'calibrations_b_tbl': b,
    }


def _extract_array_vals(text, symbol):
    match = re.search(
        rf'static const isp_lsc_gain_t {re.escape(symbol)}\[\] = \{{(.*?)\}};',
        text,
        re.S,
    )
    assert match, f'symbol not found: {symbol}'
    return [int(x) for x in re.findall(r'\{\.val = (-?\d+)\}', match.group(1))]


class TestAccLscCodegen(unittest.TestCase):
    def test_legacy_flat_ct_table(self):
        text = _gen_acc('legacy', {
            'lsc': {
                'model': 0,
                'img_w': 100,
                'img_h': 80,
                'lsc_tbl_size': 2,
                'table': [
                    _ct_node(3000, [1.0, 2.0], [1.5, 2.5], [2.0, 3.0], [2.5, 3.5]),
                    _ct_node(5000, [0.5, 1.0], [0.6, 1.1], [0.7, 1.2], [0.8, 1.3]),
                ],
            }
        })
        self.assertIn('.lsc_gain_table =', text)
        self.assertIn('.lsc_gain_table_size = 2', text)
        self.assertNotIn('.gain_table =', text)
        self.assertEqual(
            _extract_array_vals(text, 's_esp_ipa_acc_lsc_gain_r_legacy_100_x_80_ct_3000_config'),
            [256, 512],
        )

    def test_two_level_full_tables(self):
        text = _gen_acc('full', {
            'lsc': {
                'model': 0,
                'img_w': 100,
                'img_h': 80,
                'lsc_tbl_size': 2,
                'table': [
                    {
                        'gain': 1.0,
                        'table': [_ct_node(3000, [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0])],
                    },
                    {
                        'gain': 8.0,
                        'table': [_ct_node(3000, [0.5, 1.0], [0.5, 1.0], [0.5, 1.0], [0.5, 1.0])],
                    },
                ],
            }
        })
        self.assertIn('.gain_table =', text)
        self.assertIn('.gain_table_size = 2', text)
        self.assertNotIn('.lsc_gain_table =', text)
        self.assertEqual(
            _extract_array_vals(text, 's_esp_ipa_acc_lsc_gain_r_full_100_x_80_gain_1p0_ct_3000_config'),
            [256, 512],
        )
        self.assertEqual(
            _extract_array_vals(text, 's_esp_ipa_acc_lsc_gain_r_full_100_x_80_gain_8p0_ct_3000_config'),
            [128, 256],
        )

    def test_scale_from_single_base(self):
        # Contract: out = 1 + (base - 1) * scale, then round(out * 256).
        # base=[1.0, 2.0]: scale=0.5 → [256, 384]; scale=0.25 → [256, 320]
        text = _gen_acc('scale', {
            'lsc': {
                'model': 0,
                'img_w': 64,
                'img_h': 48,
                'lsc_tbl_size': 2,
                'table': [
                    {
                        'gain': 1.0,
                        'table': [_ct_node(3000, [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0])],
                    },
                    {'gain': 4.0, 'scale': 0.5},
                    {'gain': 16.0, 'scale': 0.25},
                ],
            }
        })
        self.assertIn('.gain_table_size = 3', text)
        self.assertEqual(
            _extract_array_vals(text, 's_esp_ipa_acc_lsc_gain_r_scale_64_x_48_gain_1p0_ct_3000_config'),
            [256, 512],
        )
        self.assertEqual(
            _extract_array_vals(text, 's_esp_ipa_acc_lsc_gain_r_scale_64_x_48_gain_4p0_s0p5_ct_3000_config'),
            [256, 384],
        )
        self.assertEqual(
            _extract_array_vals(text, 's_esp_ipa_acc_lsc_gain_r_scale_64_x_48_gain_16p0_s0p25_ct_3000_config'),
            [256, 320],
        )

    def test_scale_with_base_gain(self):
        # base=2.0, scale=0.5 → 1 + (2-1)*0.5 = 1.5 → 384 (not 2.0*0.5=1.0 → 256)
        text = _gen_acc('base', {
            'lsc': {
                'model': 0,
                'img_w': 32,
                'img_h': 24,
                'lsc_tbl_size': 1,
                'table': [
                    {
                        'gain': 1.0,
                        'table': [_ct_node(3000, [1.0], [1.0], [1.0], [1.0])],
                    },
                    {
                        'gain': 2.0,
                        'table': [_ct_node(3000, [2.0], [2.0], [2.0], [2.0])],
                    },
                    {'gain': 8.0, 'scale': 0.5, 'base_gain': 2.0},
                ],
            }
        })
        self.assertEqual(
            _extract_array_vals(text, 's_esp_ipa_acc_lsc_gain_r_base_32_x_24_gain_8p0_s0p5_ct_3000_config'),
            [384],
        )

    def test_scale_without_base_raises(self):
        with self.assertRaises(fatal_error):
            _gen_acc('bad', {
                'lsc': {
                    'model': 0,
                    'img_w': 16,
                    'img_h': 16,
                    'lsc_tbl_size': 1,
                    'table': [{'gain': 1.0, 'scale': 0.5}],
                }
            })


if __name__ == '__main__':
    unittest.main(verbosity=2)
