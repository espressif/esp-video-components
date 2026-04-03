# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from common import ipa_unit_c, fatal_error, dict_object

class ipa_unit_customized_c(ipa_unit_c):
    def customized_name(self):
        return self.name

    def decode(self, obj):
        if not hasattr(obj, 'name'):
            raise fatal_error(f'Missing name attribute in input object')

        self.name = obj.name
        self.text = self.name

if __name__ == '__main__':
    json_obj = dict_object({
        'name': 'esp_ipa_customized_0'
    })

    c_code = ipa_unit_customized_c(json_obj, 'sc2336', 'customized').get_text()
    print(c_code)
