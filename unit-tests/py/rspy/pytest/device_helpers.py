# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Device resolution from markers and CLI filters."""

import logging
from typing import List

from rspy import devices

log = logging.getLogger('librealsense')


def find_matching_devices(device_markers, each=True, cli_includes=None, cli_excludes=None):
    """Resolve device markers + CLI filters into a list of matching serial numbers.

    Returns (matching_sns, had_candidates):
        matching_sns: list of serial numbers that passed all filters
        had_candidates: True if devices matched the pattern before exclusions were applied
    """
    matching_sns = []
    had_candidates = False

    if cli_includes is None:
        cli_includes = []
    if cli_excludes is None:
        cli_excludes = []

    # Resolve exclusion patterns (markers + CLI) to a set of excluded serial numbers
    exclude_patterns = []
    for marker in device_markers:
        if marker.name == 'device_exclude' and marker.args:
            exclude_patterns.append(marker.args[0])
            log.debug(f"Excluding devices matching pattern: {marker.args[0]}")
    exclude_patterns.extend(cli_excludes)

    excluded_sns = set()
    for pattern in exclude_patterns:
        excluded_sns.update(devices.by_spec(pattern, []))

    # Resolve CLI includes to a set of allowed serial numbers (None = no filter)
    included_sns = None
    if cli_includes:
        included_sns = set()
        for inc in cli_includes:
            included_sns.update(devices.by_spec(inc, []))

    # Find matching devices
    for marker in device_markers:
        if marker.name not in ['device', 'device_each'] or not marker.args:
            continue

        pattern = marker.args[0]
        log.debug(f"Looking for devices matching pattern: {pattern}")

        for sn in devices.by_spec(pattern, []):
            had_candidates = True
            if sn in excluded_sns:
                log.debug(f"  Device {devices.get(sn).name} ({sn}) excluded")
                continue
            if included_sns is not None and sn not in included_sns:
                continue

            if sn not in matching_sns:
                matching_sns.append(sn)
                log.debug(f"  Found matching device: {devices.get(sn).name} ({sn})")

            if not each:
                return matching_sns, had_candidates

    return matching_sns, had_candidates


def resolve_device_each_serials(metafunc):
    """Expand @device_each markers into parametrized test instances, one per matching device.

    Called from the pytest_generate_tests hook. Resolves exclude/include patterns from
    both markers and CLI options, then calls metafunc.parametrize() with matching serials.
    """
    device_each_markers = [m for m in metafunc.definition.iter_markers("device_each")]

    if not device_each_markers:
        return

    all_serials = []

    # Resolve exclusion patterns (markers + CLI) to a set of excluded serial numbers
    exclude_markers = [m for m in metafunc.definition.iter_markers("device_exclude")]
    exclude_patterns = [m.args[0] for m in exclude_markers if m.args]
    cli_excludes = metafunc.config.getoption("--exclude-device", default=[])
    exclude_patterns.extend(cli_excludes)
    excluded_sns = set()
    for pattern in exclude_patterns:
        excluded_sns.update(devices.by_spec(pattern, []))

    # Resolve CLI --device includes to a set of allowed serial numbers (None = no filter)
    cli_includes = metafunc.config.getoption("--device", default=[])
    included_sns = None
    if cli_includes:
        included_sns = set()
        for inc in cli_includes:
            included_sns.update(devices.by_spec(inc, []))

    for marker in device_each_markers:
        if not marker.args:
            continue
        pattern = marker.args[0]
        for sn in devices.by_spec(pattern, []):
            if sn in excluded_sns:
                continue
            if included_sns is not None and sn not in included_sns:
                continue
            if sn not in all_serials:
                all_serials.append(sn)

    if all_serials:
        ids = [f"{devices.get(sn).name}-{sn}" for sn in all_serials]
        metafunc.fixturenames.append('_test_device_serial')
        metafunc.parametrize("_test_device_serial", all_serials, ids=ids, scope="function")


def is_jetson_platform():
    """Detect NVIDIA Jetson — some tests behave differently on embedded platforms."""
    try:
        with open('/proc/device-tree/model', 'r') as f:
            model = f.read()
            return 'jetson' in model.lower()
    except:
        return False
