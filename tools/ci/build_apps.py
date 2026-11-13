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
PREVIEW_TARGETS = ['esp32s31']

CI_BUILD_ALL_FILES = ['esp_cam_sensor/CMakeLists.txt', 'esp_ipa/CMakeLists.txt', 'esp_video/CMakeLists.txt', 'esp_sccb_intf/CMakeLists.txt']

logger = logging.getLogger('idf_build_apps')

def get_mr_files(modified_files: str) -> str:
    if modified_files is None:
        return ''
    modified_files = modified_files.split(' ')

    # If change to CI related files, we need to build all components
    for f in modified_files:
        if '.gitlab/ci/' in f:
            modified_files = CI_BUILD_ALL_FILES
            break

    return modified_files

def get_mr_components(modified_files: str) -> str:
    if modified_files is None:
        return ''
    components: t.Set[str] = set()
    modified_files = modified_files.split(' ')

    # If change to CI related files, we need to build all components
    for f in modified_files:
        if '.gitlab/ci/' in f:
            modified_files = CI_BUILD_ALL_FILES
            break

    for f in modified_files:
        file = Path(f)
        if (
            file.parts[0] == 'esp_cam_sensor' or file.parts[0] == 'esp_sccb_intf' or file.parts[0] == 'esp_video' or file.parts[0] == 'esp_ipa'
            and 'test_apps' not in file.parts
            and file.parts[-1] != '.build-test-rules.yml'
        ):
            components.add(file.parts[0])

    return list(components)

BMGR_EXAMPLES = ('ae_meter', 'touch_3a', 'manual_exp_gain')
BMGR_BOARD = 'esp32_p4_function_ev_board'
BMGR_OVERLAY_MARK = '# --- example sdkconfig.defaults.esp32p4 ---'


def prepare_board_manager(apps):
    """Generate board-manager sources before idf.py build.

    A clean checkout has no components/gen_bmgr_codes. ae_meter, touch_3a,
    and manual_exp_gain include dev_display_lcd.h from that generated
    component.     board_manager.defaults is applied after the project defaults and
    selects the board sensor. sdkconfig.defaults.esp32p4 is appended so
    partition, LDO, and console settings win. The example does not
    select a camera sensor.
    """
    prepared = set()
    assist_ready = False
    for app in apps:
        app_dir = Path(getattr(app, 'app_dir', ''))
        if app_dir.name not in BMGR_EXAMPLES:
            continue
        key = str(app_dir.resolve())
        if key in prepared:
            continue
        prepared.add(key)

        gen_dir = app_dir / 'components' / 'gen_bmgr_codes'
        if not (gen_dir / 'CMakeLists.txt').is_file():
            if not assist_ready:
                logger.info('Installing esp-bmgr-assist==0.8.3 for board generation')
                subprocess.check_call([sys.executable, '-m', 'pip', 'install', 'esp-bmgr-assist==0.8.3'])
                assist_ready = True
            logger.info('Generating %s board codes in %s', BMGR_BOARD, app_dir)
            subprocess.check_call(['idf.py', '-C', str(app_dir), 'bmgr', '-b', BMGR_BOARD])

        defaults = gen_dir / 'board_manager.defaults'
        overlay = app_dir / 'sdkconfig.defaults.esp32p4'
        if not defaults.is_file() or not overlay.is_file():
            raise FileNotFoundError(f'board manager output missing under {app_dir}')
        text = defaults.read_text(encoding='utf-8')
        if BMGR_OVERLAY_MARK not in text:
            defaults.write_text(
                text.rstrip() + '\n\n' + BMGR_OVERLAY_MARK + '\n' + overlay.read_text(encoding='utf-8'),
                encoding='utf-8',
            )
        # idf.py bmgr writes a sdkconfig before the device Kconfig exists.
        # A later build then keeps that file and ignores sdkconfig.defaults.
        for stale in ('sdkconfig', 'sdkconfig.old'):
            stale_path = app_dir / stale
            if stale_path.is_file():
                stale_path.unlink()


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
            verbose=1,
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

def slice_apps_for_ci(apps: list) -> list:
    """Keep each CI job's artifact archive small enough to upload.

    Matrix jobs set APP_BUILD_INDEX (1-based) and APP_BUILD_TOTAL.
    Jobs that leave them unset build the full list.
    """
    parallel_count = int(os.getenv('APP_BUILD_TOTAL', '1'))
    parallel_index = int(os.getenv('APP_BUILD_INDEX', '1'))
    total = len(apps)
    if parallel_count <= 1:
        logger.info('Processing %d total apps: building apps %d-%d', total, 1 if total else 0, total)
        return apps
    if parallel_index < 1 or parallel_index > parallel_count:
        raise SystemExit(f'APP_BUILD_INDEX {parallel_index} is outside 1..{parallel_count}')

    chunk = (total + parallel_count - 1) // parallel_count
    start = (parallel_index - 1) * chunk
    end = min(start + chunk, total)
    logger.info(
        'Processing %d total apps: building apps %d-%d (chunk %d/%d)',
        total,
        start + 1 if start < end else 0,
        end,
        parallel_index,
        parallel_count,
    )
    return apps[start:end]

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
    apps_to_build = slice_apps_for_ci(apps_to_build)
    if not apps_to_build:
        logger.info('No apps in this CI chunk')
        sys.exit(0)
    prepare_board_manager(apps_to_build)

    ignore_warning_strs = []
    for f in DEFAULT_IGNORE_WARNING_FILEPATH:
        ignore_warning_strs.extend(open(f).read().splitlines())

    if idf_build_apps_version == '1.1.4':
        ret_code = build_apps(
            apps_to_build,
            collect_size_info='size_info.txt',
            dry_run=False,
            keep_going=True,
            ignore_warning_strs=ignore_warning_strs,
            copy_sdkconfig=True,
        )
    else:
        ret_code = build_apps(
            apps_to_build,
            collect_size_info='size_info.txt',
            dry_run=False,
            keep_going=True,
            ignore_warning_strs=ignore_warning_strs,
            copy_sdkconfig=True,
            verbose=1,
        )

    sys.exit(ret_code)

