#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SENSORS_DIR = REPO_ROOT / 'esp_cam_sensor' / 'sensors'
DETECT_C = REPO_ROOT / 'esp_cam_sensor' / 'src' / 'esp_cam_sensor_detect.c'

MENCONFIG_RE = re.compile(r'^menuconfig\s+(CAMERA_\w+)\s', re.M)
AUTO_DETECT_RE = re.compile(
    r'^\s*config\s+(CAMERA_\w+_AUTO_DETECT_\w+_INTERFACE_SENSOR)\s', re.M
)
DETECT_FN_RE = re.compile(
    r'ESP_CAM_SENSOR_DETECT_FN\((\w+),\s*(ESP_CAM_SENSOR_\w+),\s*(\w+)\)'
)
INCLUDE_RE = re.compile(
    r'#if\s+CONFIG_(CAMERA_\w+)\s*\n#include\s+"([^"]+)"', re.M
)
DECLARE_RE = re.compile(
    r'#if\s+CONFIG_(CAMERA_\w+_AUTO_DETECT_\w+_INTERFACE_SENSOR)\s*\n'
    r'ESP_CAM_SENSOR_DETECT_DECLARE\((\w+),\s*(ESP_CAM_SENSOR_\w+)\);',
    re.M,
)
ENTRY_RE = re.compile(
    r'#if\s+CONFIG_(CAMERA_\w+_AUTO_DETECT_\w+_INTERFACE_SENSOR)\s*\n'
    r'\s*ESP_CAM_SENSOR_DETECT_ENTRY\((\w+),\s*(ESP_CAM_SENSOR_\w+),\s*(\w+)\)',
    re.M,
)

@dataclass(frozen=True)
class DetectFn:
    config: str
    fn: str
    port: str
    sccb: str = ''

def find_kconfig_files() -> list[Path]:
    return sorted(SENSORS_DIR.rglob('Kconfig.*'))

def parse_sensor_menuconfig(kconfig: Path) -> str | None:
    text = kconfig.read_text(encoding='utf-8')
    match = MENCONFIG_RE.search(text)
    return match.group(1) if match else None

def parse_auto_detect_configs(kconfig: Path) -> set[str]:
    text = kconfig.read_text(encoding='utf-8')
    return set(AUTO_DETECT_RE.findall(text))

def find_driver_detect_fns(sensor_dir: Path) -> list[tuple[str, str, str]]:
    result = []
    for c_file in sensor_dir.rglob('*.c'):
        text = c_file.read_text(encoding='utf-8', errors='ignore')
        result.extend(DETECT_FN_RE.findall(text))
    return result

def interface_token(port: str) -> str:
    if 'MIPI' in port:
        return 'MIPI'
    if 'DVP' in port:
        return 'DVP'
    if 'SPI' in port:
        return 'SPI'
    raise ValueError(f'unknown port: {port}')

def auto_detect_config(main_cfg: str, port: str) -> str:
    return f'{main_cfg}_AUTO_DETECT_{interface_token(port)}_INTERFACE_SENSOR'

def parse_detect_c(text: str) -> tuple[set[str], set[DetectFn], set[DetectFn]]:
    includes = {match.group(1) for match in INCLUDE_RE.finditer(text)}
    declares = {
        DetectFn(match.group(1), match.group(2), match.group(3))
        for match in DECLARE_RE.finditer(text)
    }
    entries = {
        DetectFn(match.group(1), match.group(2), match.group(3), match.group(4))
        for match in ENTRY_RE.finditer(text)
    }
    return includes, declares, entries

def main() -> int:
    detect_text = DETECT_C.read_text(encoding='utf-8')
    includes, declares, entries = parse_detect_c(detect_text)

    errors: list[str] = []
    known_main_cfgs: set[str] = set()

    for kconfig in find_kconfig_files():
        main_cfg = parse_sensor_menuconfig(kconfig)
        if not main_cfg:
            continue

        known_main_cfgs.add(main_cfg)
        sensor_dir = kconfig.parent
        rel_dir = sensor_dir.relative_to(REPO_ROOT)
        auto_cfgs = parse_auto_detect_configs(kconfig)
        driver_fns = find_driver_detect_fns(sensor_dir)

        if main_cfg not in includes:
            errors.append(
                f'[{rel_dir}] missing `#if CONFIG_{main_cfg}` include '
                f'in esp_cam_sensor_detect.c'
            )

        for fn, port, sccb in driver_fns:
            cfg = auto_detect_config(main_cfg, port)

            if cfg not in auto_cfgs:
                errors.append(
                    f'[{rel_dir}] driver defines {fn}({port}) but Kconfig '
                    f'missing config {cfg}'
                )
                continue

            declare = DetectFn(cfg, fn, port)
            entry = DetectFn(cfg, fn, port, sccb)

            if declare not in declares:
                errors.append(
                    f'[{rel_dir}] missing ESP_CAM_SENSOR_DETECT_DECLARE({fn}, {port}) '
                    f'under `#if CONFIG_{cfg}`'
                )

            if entry not in entries:
                errors.append(
                    f'[{rel_dir}] missing ESP_CAM_SENSOR_DETECT_ENTRY({fn}, {port}, {sccb}) '
                    f'under `#if CONFIG_{cfg}`'
                )

    stale_includes = includes - known_main_cfgs
    for cfg in sorted(stale_includes):
        errors.append(
            f'esp_cam_sensor_detect.c references CONFIG_{cfg}, but no matching '
            f'Kconfig under esp_cam_sensor/sensors/'
        )

    if errors:
        print('Camera sensor detect registration check FAILED:\n')
        for error in errors:
            print(f'  - {error}')
        print(
            '\nFix: update esp_cam_sensor/src/esp_cam_sensor_detect.c '
            'per esp_cam_sensor/README.md (Static store section).'
        )
        return 1

    print('Camera sensor detect registration check passed.')
    return 0

if __name__ == '__main__':
    sys.exit(main())
