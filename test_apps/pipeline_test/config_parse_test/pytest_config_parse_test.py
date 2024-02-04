# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps==1.1.4
  - python tools/build_apps.py test_apps/pipeline_test/config_parse_test -t esp32p4
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest test_apps/pipeline_test/config_parse_test --target esp32p4
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32p4')
@pytest.mark.env('quad_psram')
def test_esp_video(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[video]')
    dut.expect_unity_test_output(timeout = 60)
