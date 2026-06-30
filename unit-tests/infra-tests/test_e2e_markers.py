# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: Verify all custom markers are registered without warnings.

Uses the real conftest.py via subprocess — if a marker registration is
removed or renamed, these tests fail.
"""

from helpers import run_e2e, assert_outcomes


class TestMarkerRegistration:

    def test_all_markers(self):
        rc, out, *_ = run_e2e("pytest-markers-all.py",
                              "--context", "nightly", "-W", "error::pytest.PytestUnknownMarkWarning")
        assert_outcomes(out, passed=1)

    def test_device_marker(self):
        rc, out, *_ = run_e2e("pytest-markers-device.py",
                              "-W", "error::pytest.PytestUnknownMarkWarning")
        assert_outcomes(out, passed=1)
