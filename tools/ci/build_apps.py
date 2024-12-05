# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import os, subprocess, sys
import typing as t
from pathlib import Path
from idf_build_apps.constants import SUPPORTED_TARGETS

def get_mr_files(modified_files: str) -> str:
    if modified_files is None:
        return ''
    return modified_files.split(' ')

def get_mr_components(modified_files: str) -> str:
    if modified_files is None:
        return ''
    components: t.Set[str] = set()
    modified_files = modified_files.split(' ')
    for f in modified_files:
        file = Path(f)
        if (
            file.parts[0] == 'esp_cam_sensor' or file.parts[0] == 'esp_sccb_intf' or file.parts[0] == 'esp_video' or file.parts[0] == 'esp_ipa'
            and 'test_apps' not in file.parts
            and file.parts[-1] != '.build-test-rules.yml'
        ):
            components.add(file.parts[0])

    return list(components)

if __name__ == '__main__':
    modified_files = get_mr_files(os.getenv('MODIFIED_FILES'))
    modified_components = get_mr_components(os.getenv('MODIFIED_FILES'))

    preview_targets = []
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
        #'--check-warnings',
        # Build args
        '--collect-size-info',
        'size_info.txt',
        '--keep-going',
        '--copy-sdkconfig',
        '--config',
        'sdkconfig.ci.*=',
        '=default',
        '-v'
    ]

    args += ['--modified-components'] + modified_components
    args += ['--modified-files'] + modified_files

    args += ['--default-build-targets'] + SUPPORTED_TARGETS + preview_targets

    manifests = [str(p) for p in Path(root).glob('**/.build-test-rules.yml')]
    if manifests:
        args += ['--manifest-file'] + manifests + ['--manifest-rootpath', root]

    ret = subprocess.run(['idf-build-apps', *args])
    sys.exit(ret.returncode)
