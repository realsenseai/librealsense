# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Collection-phase filtering and sorting for RealSense tests."""

import pytest


def filter_and_sort_items(config, items):
    """Auto-skip nightly/dds tests unless opted in, filter --live, and sort by priority.

    Called from the pytest_collection_modifyitems hook.
    """
    markexpr = config.getoption("-m", default="")

    if not (markexpr and "nightly" in markexpr):
        skip_nightly = pytest.mark.skip(reason="Nightly test (use -m nightly to run)")
        for item in items:
            if "nightly" in item.keywords:
                item.add_marker(skip_nightly)

    if not (markexpr and "dds" in markexpr):
        skip_dds = pytest.mark.skip(reason="DDS test (use -m dds to run)")
        for item in items:
            if "dds" in item.keywords:
                item.add_marker(skip_dds)

    # Skip non-device tests when --live is specified
    if config.getoption("--live", default=False):
        skip_no_device = pytest.mark.skip(reason="--live: test has no device requirement")
        for item in items:
            has_device = any(item.iter_markers("device")) or any(item.iter_markers("device_each"))
            if not has_device:
                item.add_marker(skip_no_device)

    def get_priority(item):
        marker = item.get_closest_marker("priority")
        if marker and marker.args:
            return marker.args[0]
        return 500

    items.sort(key=get_priority)

    # Group parametrized tests by device within each module, so all tests run on one
    # device before switching to the next (matching run-unit-tests.py behavior).
    def get_device_group_key(item):
        module = item.module.__name__
        if hasattr(item, 'callspec') and '_test_device_serial' in item.callspec.params:
            device_serial = item.callspec.params['_test_device_serial']
        else:
            device_serial = ''
        return (module, device_serial)

    items.sort(key=get_device_group_key)
