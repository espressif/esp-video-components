#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from __future__ import print_function, unicode_literals

import os.path

#ESP_DOCS_PATH = os.environ['ESP_DOCS_PATH']

try:
    from esp_docs.conf_docs import *  # noqa: F403,F401
except ImportError:
    import os
    import sys
    sys.path.insert(0, os.path.abspath(ESP_DOCS_PATH))
    from conf_docs import *  # noqa: F403,F401

ESP32P4_DOCS = []

# format: {tag needed to include: documents to included}, tags are parsed from sdkconfig and peripheral_caps.h headers
conditional_include_dict = {
                            'esp32p4':ESP32P4_DOCS,
                            }

extensions += ['sphinx_copybutton',
               # Note: order is important here, events must
               # be registered by one extension before they can be
               # connected to another extension
               'esp_docs.esp_extensions.dummy_build_system',
               'esp_docs.esp_extensions.run_doxygen',
               ]

# link roles config
github_repo = 'espressif/esp-video-components'

# context used by sphinx_idf_theme
html_context['github_user'] = 'espressif'
html_context['github_repo'] = 'esp-video-components'

idf_targets = ['esp32p4']
languages = ['en', 'zh_CN']

google_analytics_id = 'G-R6EJ3YJP4G'

project_homepage = 'https://github.com/espressif/esp-video-components'

html_static_path = ['../_static']

# Extra options required by sphinx_idf_theme
project_slug = 'esp-video-components development reference'

versions_url = './_static/js/esp_video_guide_versions.js'

# Final PDF filename will contains target and version
pdf_file_prefix = u'esp-video-components development reference'
