# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Collection-phase filtering and sorting for RealSense tests."""

import pytest


def filter_and_sort_items(config, items):
    """Auto-skip nightly/dds tests unless opted in, filter --live/--not-live, and sort by priority.

    Called from the pytest_collection_modifyitems hook.
    """
    markexpr = config.getoption("-m", default="")
    context = config.getoption("--context", default="").split()

    # Generic context gating: tests marked with @pytest.mark.context("X") are skipped
    # unless "X" appears in --context or -m. No infra changes needed for new contexts.
    for item in items:
        for marker in item.iter_markers("context"):
            if not marker.args:
                continue
            required_context = marker.args[0]
            if required_context in context:
                continue
            if markexpr and required_context in markexpr:
                continue
            item.add_marker(pytest.mark.skip(
                reason=f"Requires --context {required_context} (or -m {required_context})"))

    # Skip non-device tests when --live is specified
    if config.getoption("--live", default=False):
        skip_no_device = pytest.mark.skip(reason="--live: test has no device requirement")
        for item in items:
            has_device = any(item.iter_markers("device")) or any(item.iter_markers("device_each"))
            if not has_device:
                item.add_marker(skip_no_device)

    # Skip device tests when --not-live is specified (no hardware, e.g. GHA runners)
    if config.getoption("--not-live", default=False):
        skip_device = pytest.mark.skip(reason="--not-live: test requires a live device")
        for item in items:
            has_device = any(item.iter_markers("device")) or any(item.iter_markers("device_each"))
            if has_device:
                item.add_marker(skip_device)

    def get_priority(item):
        marker = item.get_closest_marker("priority")
        if marker and marker.args:
            return marker.args[0]
        return 500

    items.sort(key=get_priority)

    # Group parametrized tests by device within each module, so all tests run on one
    # device before switching to the next (matching run-unit-tests.py behavior).
    # Within a (module, device) bucket, also sort by pytest-repeat step so pass 0
    # completes before pass 1 — preserves --repeat N module-scoped ordering so
    # module-scoped fixtures see one pass at a time.
    def get_device_group_key(item):
        module = item.module.__name__
        params = item.callspec.params if hasattr(item, 'callspec') else {}
        device_serial = params.get('_test_device_serial', '')
        step = params.get('__pytest_repeat_step_number', 0)
        return (module, device_serial, step)

    items.sort(key=get_device_group_key)
