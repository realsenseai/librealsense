# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Device resolution from markers and CLI filters."""

import logging
from typing import List

from rspy import devices

log = logging.getLogger('librealsense')


def split_cli_patterns(patterns):
    """Flatten a list of patterns by splitting each entry on whitespace.

    Supports both repeated flags (``--exclude-device D555 --exclude-device D585S``)
    and a single flag with a space-separated value (``--exclude-device 'D555 D585S'``),
    matching the legacy run-unit-tests.py behavior.
    """
    out = []
    for p in patterns or []:
        out.extend(p.split())
    return out


def _build_sn_filter(markers, cli_includes=None, cli_excludes=None):
    """Build a ``passes(sn) -> bool`` callable that applies all three filter layers.

    The returned callable returns True only when *sn* clears:

    1. **Exclusion**: serial not in the set derived from ``device_exclude`` markers
       and *cli_excludes*.
    2. **Inclusion allowlist**: serial is in the set derived from *cli_includes*,
       or *cli_includes* is empty (no restriction).
    3. **Connection-type**: serial satisfies any ``device_type`` /
       ``device_type_exclude`` markers.

    *cli_includes* / *cli_excludes* may be raw option strings (whitespace-separated
    names in a single entry) or already-split lists — ``split_cli_patterns`` is
    applied internally either way.
    """
    cli_includes = split_cli_patterns(cli_includes)
    cli_excludes = split_cli_patterns(cli_excludes)

    # --- exclusion set ---
    exclude_patterns = [m.args[0] for m in markers if m.name == 'device_exclude' and m.args]
    for pat in exclude_patterns:
        log.debug(f"Excluding devices matching pattern: {pat}")
    exclude_patterns.extend(cli_excludes)
    excluded_sns: set = set()
    for pattern in exclude_patterns:
        excluded_sns.update(devices.by_spec(pattern, []))

    # --- inclusion allowlist (None = no restriction) ---
    included_sns = None
    if cli_includes:
        included_sns = set()
        for inc in cli_includes:
            included_sns.update(devices.by_spec(inc, []))

    # --- connection-type sets ---
    required_types, excluded_types = _build_type_sets(markers)

    def passes(sn):
        if sn in excluded_sns:
            return False
        if included_sns is not None and sn not in included_sns:
            return False
        return _passes_connection_type(sn, required_types, excluded_types)

    return passes


def find_matching_devices(device_markers, each=True, cli_includes=None, cli_excludes=None):
    """Resolve device markers + CLI filters into a list of matching serial numbers.

    Returns (matching_sns, had_candidates):
        matching_sns: list of serial numbers that passed all filters
        had_candidates: True if devices matched the pattern before exclusions were applied
    """
    matching_sns = []
    had_candidates = False

    passes = _build_sn_filter(device_markers, cli_includes, cli_excludes)

    for marker in device_markers:
        if marker.name not in ['device', 'device_each'] or not marker.args:
            continue

        pattern = marker.args[0]
        log.debug(f"Looking for devices matching pattern: {pattern}")

        for sn in devices.by_spec(pattern, []):
            had_candidates = True
            if not passes(sn):
                continue
            if sn not in matching_sns:
                matching_sns.append(sn)
                log.debug(f"  Found matching device: {devices.get(sn).name} ({sn})")
            if not each:
                return matching_sns, had_candidates

    return matching_sns, had_candidates


_MISSING_SENTINEL_PREFIX = "__MISSING__:"
_SKIP_SENTINEL_PREFIX = "__SKIP__:"


def _build_type_sets(markers):
    """Return (required_types, excluded_types) from device_type / device_type_exclude markers."""
    required = set()
    excluded = set()
    for m in markers:
        if m.name == 'device_type' and m.args:
            required.add(m.args[0].upper())
            log.debug(f"Requiring devices with connection type: {m.args[0]}")
        elif m.name == 'device_type_exclude' and m.args:
            excluded.add(m.args[0].upper())
            log.debug(f"Excluding devices with connection type: {m.args[0]}")
    return required, excluded


def _passes_connection_type(sn, required_types, excluded_types):
    """Return True if the device at sn satisfies the connection-type filters."""
    if not required_types and not excluded_types:
        return True
    dev = devices.get(sn)
    conn_type = (getattr(dev, 'connection_type', None) or "").upper() if dev else ""
    if required_types and conn_type not in required_types:
        log.debug(f"  Device {dev.name if dev else sn} ({sn}) skipped: connection type {conn_type!r} not in {required_types}")
        return False
    if conn_type in excluded_types:
        log.debug(f"  Device {dev.name if dev else sn} ({sn}) excluded by connection type {conn_type!r}")
        return False
    return True


def resolve_device_each_serials(metafunc):
    """Expand @device_each and @device markers into parametrized test instances.

    Called from the pytest_generate_tests hook. Resolves exclude/include patterns from
    both markers and CLI options, then calls metafunc.parametrize() with matching serials.

    - ``device_each(pattern)``: one instance per matching device (optional — no instance
      if no device matches).
    - ``device(pattern)`` (single-spec form): exactly one instance using the first matching
      device.  If no device matches, a ``__MISSING__:pattern`` sentinel is added so that
      the test still runs and ``module_device_setup`` can call pytest.fail() for it.

    When only ``device`` markers are present (no ``device_each``), this hook returns early
    and lets the existing non-parametrized path in ``module_device_setup`` handle them.
    """
    device_each_markers = [m for m in metafunc.definition.iter_markers("device_each")]
    # Single-spec device() markers only — multi-spec (e.g. device("A", "B")) are handled
    # entirely inside module_device_setup via find_matching_devices_multi.
    single_device_markers = [
        m for m in metafunc.definition.iter_markers("device")
        if m.args and len(m.args) == 1
    ]

    # Nothing to do if there are no device_each markers.
    # (Pure device() markers are handled by the non-parametrized path in module_device_setup.)
    if not device_each_markers:
        return

    all_serials = []

    all_markers = list(metafunc.definition.iter_markers())
    cli_includes = metafunc.config.getoption("--device", default=[])
    cli_excludes = metafunc.config.getoption("--exclude-device", default=[])
    passes = _build_sn_filter(all_markers, cli_includes, cli_excludes)

    for marker in device_each_markers:
        if not marker.args:
            continue
        pattern = marker.args[0]
        for sn in devices.by_spec(pattern, []):
            if passes(sn) and sn not in all_serials:
                all_serials.append(sn)

    # When device() markers coexist with device_each(), resolve them here so they also
    # receive a parametrized instance.  Each device() marker contributes exactly one
    # serial (the first matching device), or a sentinel if none is found.
    #
    # Two sentinel cases mirror the non-parametrized path in module_device_setup:
    #   had_raw_match=True  → device exists but all filtered (exclude/type) → skip
    #   had_raw_match=False → no device of this type in the lab at all      → fail
    for marker in single_device_markers:
        pattern = marker.args[0]
        found_sn = None
        had_raw_match = False
        for sn in devices.by_spec(pattern, []):
            had_raw_match = True
            if passes(sn):
                found_sn = sn
                break
        if found_sn is not None:
            if found_sn not in all_serials:
                all_serials.append(found_sn)
        elif had_raw_match:
            # Device(s) matched the pattern but were all excluded — skip gracefully.
            sentinel = f"{_SKIP_SENTINEL_PREFIX}{pattern}"
            if sentinel not in all_serials:
                all_serials.append(sentinel)
        else:
            # No candidates at all — device genuinely absent from the lab.
            sentinel = f"{_MISSING_SENTINEL_PREFIX}{pattern}"
            if sentinel not in all_serials:
                all_serials.append(sentinel)

    if all_serials:
        def _serial_id(sn):
            if sn.startswith(_MISSING_SENTINEL_PREFIX):
                return f"MISSING-{sn[len(_MISSING_SENTINEL_PREFIX):]}"
            if sn.startswith(_SKIP_SENTINEL_PREFIX):
                return f"SKIP-{sn[len(_SKIP_SENTINEL_PREFIX):]}"
            dev = devices.get(sn)
            return f"{dev.name}-{sn}" if dev else sn

        ids = [_serial_id(sn) for sn in all_serials]
        metafunc.fixturenames.append('_test_device_serial')
        metafunc.parametrize("_test_device_serial", all_serials, ids=ids, scope="function")


def find_matching_devices_multi(device_markers, cli_includes=None, cli_excludes=None):
    """Resolve a multi-device marker into a list of unique serial numbers.

    Supports device("D400*", "D400*") meaning "need 2 unique D400 devices",
    or device("D400*", "D500*") meaning "need one D400 and one D500".
    Each spec grabs a unique device not already taken by a previous spec
    (same logic as legacy devices.by_configuration).

    Returns (matching_sns, had_candidates):
        matching_sns: list of serial numbers, one per spec
        had_candidates: True if any devices matched before exclusions
    """
    # Find the multi-device marker (only one expected)
    specs = []
    for marker in device_markers:
        if marker.name == 'device' and marker.args:
            specs = list(marker.args)
            break

    if not specs:
        return [], False

    passes = _build_sn_filter(device_markers, cli_includes, cli_excludes)

    # Resolve each spec to a unique device (like legacy by_configuration)
    matching_sns = []
    taken: set = set()
    had_candidates = False

    for spec in specs:
        found = False
        for sn in devices.by_spec(spec, []):
            had_candidates = True
            if sn in taken or not passes(sn):
                continue
            matching_sns.append(sn)
            taken.add(sn)
            found = True
            log.debug(f"  Spec '{spec}' matched: {devices.get(sn).name} ({sn})")
            break
        if not found:
            log.debug(f"  Spec '{spec}' found no available device")

    return matching_sns, had_candidates


def is_jetson_platform():
    """Detect NVIDIA Jetson — some tests behave differently on embedded platforms."""
    try:
        with open('/proc/device-tree/model', 'r') as f:
            model = f.read()
            return 'jetson' in model.lower()
    except:
        return False


_fw_version_cache: dict = {}  # (serial, str(min_version), inclusive) -> True (set only on pass)


def require_min_fw_version(dev, min_version, feature_name="", inclusive=True):
    """Skip the test if the device FW does not meet the minimum version requirement.

    Caches the result per (device serial, min_version, inclusive) so the check
    runs at most once per module per device — subsequent calls for the same key
    are no-ops when the check already passed.  If the FW is too old, pytest.skip()
    is called (raising Skipped), so the cache entry is never written and every
    test that calls this function will also be skipped.

    Args:
        dev: RealSense device (rs.device).
        min_version: rsutils.version — the minimum acceptable FW version.
        feature_name: Optional name of the feature requiring this FW, included in
                      the skip message for clarity.
        inclusive: True (default) → require fw >= min_version (skip if fw < min).
                   False → require fw > min_version (skip if fw <= min).
    """
    import pytest
    import pyrealsense2 as rs
    import pyrsutils as rsutils

    serial = dev.get_info(rs.camera_info.serial_number)
    key = (serial, str(min_version), inclusive)
    if key in _fw_version_cache:
        return
    if not dev.supports(rs.camera_info.firmware_version):
        pytest.skip("Device does not support firmware version info")
    fw_version = rsutils.version(dev.get_info(rs.camera_info.firmware_version))
    should_skip = (fw_version < min_version) if inclusive else (fw_version <= min_version)
    if should_skip:
        op = ">=" if inclusive else ">"
        feature_str = f" ({feature_name})" if feature_name else ""
        pytest.skip(f"FW version {fw_version} does not meet minimum {op} {min_version}{feature_str}, skipping test...")
    _fw_version_cache[key] = True
