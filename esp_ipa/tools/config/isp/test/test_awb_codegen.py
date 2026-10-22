# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from awb import ipa_unit_awb_c, fatal_error  # noqa: E402
from common import dict_object  # noqa: E402


def _gen_awb(name, awb_obj):
    return ipa_unit_awb_c(dict_object(awb_obj), name, 'awb').get_text()


class TestAwbFixedCtCodegen(unittest.TestCase):
    def test_fixed_ct_presets(self):
        text = _gen_awb('ct', {
            'model': 3,
            'fixed_ct': {
                'default_ct': 5000,
                'presets': [
                    {'ct': 3000, 'rg': 0.52, 'bg': 0.58},
                    {'ct': 5000, 'rg': 0.5602, 'bg': 0.5390},
                ],
            },
        })
        self.assertIn('.default_ct = 5000', text)
        self.assertIn('.presets_count = 2', text)
        self.assertIn('.ct = 3000', text)
        self.assertIn('.ct = 5000', text)

    def test_deprecated_fixed_alias_presets(self):
        text = _gen_awb('alias', {
            'model': 3,
            'fixed': {
                'default_ct': 6500,
                'presets': [
                    {'ct': 5000, 'rg': 0.5602, 'bg': 0.5390},
                    {'ct': 6500, 'rg': 0.58, 'bg': 0.52},
                ],
            },
        })
        self.assertIn('.default_ct = 6500', text)
        self.assertIn('.presets_count = 2', text)
        self.assertIn('.ct = 5000', text)
        self.assertIn('.ct = 6500', text)

    def test_deprecated_fixed_alias_legacy_single(self):
        text = _gen_awb('legacy', {
            'model': 'fixed',
            'fixed': {'ct': 5000, 'rg': 0.5602, 'bg': 0.5390},
        })
        self.assertIn('.default_ct = 5000', text)
        self.assertIn('.presets_count = 1', text)
        self.assertIn('.ct = 5000', text)
        self.assertIn('.rg = 0.560200f', text)
        self.assertIn('.bg = 0.539000f', text)

    def test_fixed_ct_preferred_over_fixed(self):
        text = _gen_awb('pref', {
            'model': 3,
            'fixed_ct': {'ct': 4000, 'rg': 0.5, 'bg': 0.6},
            'fixed': {'ct': 6500, 'rg': 0.58, 'bg': 0.52},
        })
        self.assertIn('.default_ct = 4000', text)
        self.assertIn('.ct = 4000', text)
        self.assertNotIn('.ct = 6500', text)

    def test_model_3_without_fixed_raises(self):
        with self.assertRaises(fatal_error):
            _gen_awb('bad', {'model': 3})


if __name__ == '__main__':
    unittest.main(verbosity=2)
