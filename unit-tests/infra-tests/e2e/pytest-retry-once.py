# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""E2E fixture: a module with two tests. test_a fails on the first attempt
and passes on the second; test_b always passes. Used to verify parent-side
retry re-spawns the WHOLE module (not just the failed test) on failure.

The marker file lives next to this file in helpers.run_e2e()'s tmpdir, so
it survives across subprocess attempts within one run_e2e() invocation but
not across runs.

@device marker makes the subprocess_isolation plugin's
_enable_target_devices_for_group resolve a target serial and trigger
devices.enable_only -- which the e2e_conftest mock counts.
"""

import os

import pytest

_MARKER = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".retry_marker")

pytestmark = [pytest.mark.device("D400*")]


def test_a():
    if not os.path.exists(_MARKER):
        with open(_MARKER, "w") as f:
            f.write("first attempt was here")
        raise AssertionError("first attempt fails by design (marker created)")
    # second attempt: marker exists -> pass


def test_b():
    # Always passes. Used to verify the WHOLE module re-runs on retry, not
    # just the failed test_a.
    pass
