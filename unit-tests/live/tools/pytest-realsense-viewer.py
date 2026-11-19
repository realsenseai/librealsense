# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 Intel Corporation. All Rights Reserved.

import pytest
from viewer_helper import run_viewer_tests

# the tests in the realsense-viewer-tests executable are defined along the tester file itself at
# tools/realsense-viewer/tests/. Using the --auto flag allows us to run all tests one by one.
# If we want to run a specific test / test group, we can use the -r flag

pytestmark = [
    pytest.mark.device_each("D400*"),
    # D500 is currently disabled until all tests pass there
    #pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
    pytest.mark.context("gui"),
    # Override the 200s default from conftest.py: mem_leak_depth_start_stop is a ~2.5 min
    # soak (10 * (10s + 5s) real) and the rest of the suite adds ~90s on real CI hardware
    # (LibCI 17981 D555 Win 245s, D585 Proto Linux 234s). 400s gives ~25% headroom over that.
    pytest.mark.timeout(400),
    # Opt out of retries: this launches the realsense-viewer GUI and is long-running /
    # not reliably retryable, so a rerun just doubles CI time without adding signal.
    pytest.mark.flaky(reruns=0),
]


def test_realsense_viewer_gui(module_device_setup):
    run_viewer_tests()
