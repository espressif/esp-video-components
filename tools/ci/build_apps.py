# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import subprocess, sys
from pathlib import Path
from idf_build_apps.constants import SUPPORTED_TARGETS

if __name__ == '__main__':
    preview_targets = ['esp32p4']
    root = '.'

    args = [
        'build',
        # Find args
        '-p',
        root,
        '-t',
        'all',
        '--build-dir',
        'build_@t_@w',
        '--build-log',
        'build_log.txt',
        '--size-file',
        'size.json',
        '--recursive',
        '--check-warnings',
        # Build args
        '--collect-size-info',
        'size_info.txt',
        '--keep-going',
        '--copy-sdkconfig',
        '-v',
    ]

    args += ['--default-build-targets'] + SUPPORTED_TARGETS + preview_targets

    manifests = [str(p) for p in Path(root).glob('**/.build-test-rules.yml')]
    if manifests:
        args += ['--manifest-file'] + manifests + ['--manifest-rootpath', root]

    ret = subprocess.run(['idf-build-apps', *args])
    sys.exit(ret.returncode)
