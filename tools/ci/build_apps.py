# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from pathlib import Path
from idf_build_apps import find_apps, build_apps, setup_logging
from idf_build_apps.constants import SUPPORTED_TARGETS


if __name__ == '__main__':
    extra_default_build_targets = ['esp32p4']
    app_path = '.'

    setup_logging(verbose=True, colored=True)

    apps = find_apps(
        app_path,
        recursive=True,
        target='all',
        build_dir='build_@t_@w',
        build_log_path='build_log.txt',
        size_json_path='size.json',
        check_warnings=True,
        manifest_files=[str(p) for p in Path(app_path).glob('**/.build-test-rules.yml')],
        default_build_targets=SUPPORTED_TARGETS + extra_default_build_targets,
        manifest_rootpath=app_path,
    )

    build_apps(
        sorted(apps),
        dry_run=False,
        keep_going=True,
        collect_size_info='size_info.txt',
        copy_sdkconfig=True,
    )
