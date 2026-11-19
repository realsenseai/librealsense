# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
from viewer_helper import run_viewer_tests

# The full viewer GUI suite is not enabled on D500 yet (see pytest-realsense-viewer.py),
# so the D500 restore-factory-calibration GUI test runs on its own here.

pytestmark = [
    pytest.mark.device_each("D585"),
    pytest.mark.device_exclude("D585S"),  # other D500 cameras not verified yet
    pytest.mark.context("nightly"),
    pytest.mark.context("gui"),
    pytest.mark.timeout(400),
    pytest.mark.flaky(reruns=0),
]


def test_viewer_restore_factory_calibration(module_device_setup):
    run_viewer_tests("calibration")
