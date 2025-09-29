# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import logging
import os, subprocess, sys
import typing as t
import argparse
from pathlib import Path
from idf_build_apps.constants import SUPPORTED_TARGETS
from idf_build_apps import App, build_apps, find_apps, setup_logging
from idf_build_apps import __version__ as idf_build_apps_version

if idf_build_apps_version == '1.1.4':
    from idf_build_apps import LOGGER

DEFAULT_CONFIG_RULES_STR = ['sdkconfig.ci=default', 'sdkconfig.ci.*=', '=default']
DEFAULT_IGNORE_WARNING_FILEPATH = [os.path.join('tools', 'ci', 'ignore_build_warnings.txt')]
PREVIEW_TARGETS = []

logger = logging.getLogger('idf_build_apps')

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

def find_all_apps(root: str, manifest_files: list[str], modified_components: list[str], modified_files: list[str], targets: list[str]):
    if idf_build_apps_version == '1.1.4':
        apps = find_apps(
            paths=root,
            target='all',
            build_dir=f'build_@t_@w',
            build_log_path='build_log.txt',
            size_json_path='size.json',
            recursive=True,
            check_warnings=True,
            config_rules_str=DEFAULT_CONFIG_RULES_STR,
            default_build_targets=SUPPORTED_TARGETS + PREVIEW_TARGETS,
            modified_components=modified_components,
            modified_files=modified_files,
            manifest_files=manifest_files,
        )
    else:
        apps = find_apps(
            paths=root,
            target='all',
            build_dir=f'build_@t_@w',
            build_log_filename='build_log.txt',
            recursive=True,
            check_warnings=True,
            config_rules_str=DEFAULT_CONFIG_RULES_STR,
            default_build_targets=SUPPORTED_TARGETS + PREVIEW_TARGETS,
            modified_components=modified_components,
            modified_files=modified_files,
            manifest_files=manifest_files,
        )

    match_apps = []
    for app in apps:
        if app.target not in targets:
            continue

        if app.config_name == 'default':
            match_apps.append(app)
        else:
            target_match_str = '%s.'%app.target
            if target_match_str == app.config_name[0:len(target_match_str)]:
                match_apps.append(app)
            elif 'esp32' not in app.config_name:
                match_apps.append(app)
            else:
                logger.debug('Exclude: %s %s %s'%(app.name, app.target, app.config_name))

    return match_apps

if __name__ == '__main__':
    root = '.'

    setup_logging(verbose=1)

    parser = argparse.ArgumentParser()
    parser.add_argument('--targets', type=str, default='esp32p4', help='Targets to build')
    args = parser.parse_args()
    targets = args.targets.split(';')

    logger.info(f'Targets to build: {targets}')

    modified_files = get_mr_files(os.getenv('MODIFIED_FILES'))
    modified_components = get_mr_components(os.getenv('MODIFIED_FILES'))
    manifests = [str(p) for p in Path(root).glob('**/.build-test-rules.yml')]

    apps_to_build = find_all_apps(root=root, manifest_files=manifests, modified_components=modified_components, modified_files=modified_files, targets=targets)

    ignore_warning_strs = []
    for f in DEFAULT_IGNORE_WARNING_FILEPATH:
        ignore_warning_strs.extend(open(f).read().splitlines())

    ret_code = build_apps(
        apps_to_build,
        collect_size_info='size_info.txt',
        dry_run=False,
        keep_going=True,
        ignore_warning_strs=ignore_warning_strs,
        copy_sdkconfig=True
    )

    sys.exit(ret_code)

