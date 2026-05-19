# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: Device hub port management and skip/fail behavior.

Tests that module_device_setup:
- Calls enable_only with the correct serials and recycle flag
- Reuses devices across tests in the same module without re-enabling
- Fails when @device has no match, skips when @device_each has no match
- Skips when all candidates are excluded (via marker or CLI)
"""

from helpers import run_e2e, assert_outcomes


class TestDevicePortManagement:

    def test_device_marker_enables_correct_port(self):
        """@device('D455') should call enable_only(['111'], recycle=True)."""
        rc, out, tracking = run_e2e("pytest-device-setup.py", "-k", "test_d455 and not excluded")
        assert_outcomes(out, passed=1)
        assert len(tracking["enable_only_calls"]) == 1
        assert tracking["enable_only_calls"][0]['serials'] == ['111']
        assert tracking["enable_only_calls"][0]['recycle'] is True

    def test_device_each_enables_one_port_per_test(self):
        """@device_each('D400*') should call enable_only once per device, each with recycle=True."""
        rc, out, tracking = run_e2e("pytest-each-setup.py", "-k", "test_d400 and not d999")
        assert_outcomes(out, passed=3)
        assert len(tracking["enable_only_calls"]) == 3
        serials_enabled = [c['serials'][0] for c in tracking["enable_only_calls"]]
        assert set(serials_enabled) == {'111', '222', '777'}
        assert all(c['recycle'] is True for c in tracking["enable_only_calls"])
        assert all(len(c['serials']) == 1 for c in tracking["enable_only_calls"])

    def test_second_test_same_device_no_recycle(self):
        """Two tests on the same device: first recycles, second reuses (no enable_only call)."""
        rc, out, tracking = run_e2e("pytest-port-reuse.py")
        assert_outcomes(out, passed=2)
        assert len(tracking["enable_only_calls"]) == 1

    def test_no_device_marker_no_enable(self):
        """Tests without device markers should not call enable_only."""
        rc, out, tracking = run_e2e("pytest-device-setup.py", "-k", "test_no_markers")
        assert_outcomes(out, passed=1)
        assert len(tracking["enable_only_calls"]) == 0

    def test_device_no_match_fails_without_enabling(self):
        """@device('D999') with no match should fail and never call enable_only."""
        rc, out, tracking = run_e2e("pytest-device-setup.py", "-k", "test_d999_no_match")
        assert_outcomes(out, error=1)
        assert "No devices" in out
        assert len(tracking["enable_only_calls"]) == 0

    def test_device_each_no_match_skips_without_enabling(self):
        """@device_each('D999') with no match should skip and never call enable_only."""
        rc, out, tracking = run_e2e("pytest-each-setup.py", "-k", "test_d999_no_match")
        assert_outcomes(out, skipped=1)
        assert len(tracking["enable_only_calls"]) == 0

    def test_device_skips_when_all_excluded_by_marker(self):
        """@device('D455') + @device_exclude('D455') → skip (excluded via marker)."""
        rc, out, tracking = run_e2e("pytest-device-setup.py", "-k", "test_d455_excluded")
        assert_outcomes(out, skipped=1)
        assert len(tracking["enable_only_calls"]) == 0

    def test_device_each_skips_when_all_excluded_by_cli(self):
        """@device_each('D455') + --exclude-device D455 → skip (excluded via CLI)."""
        rc, out, tracking = run_e2e("pytest-each-setup.py", "-k", "test_d455_excluded", "--exclude-device", "D455")
        assert_outcomes(out, skipped=1)
        assert len(tracking["enable_only_calls"]) == 0

    def test_device_each_no_match_runs_unparametrized(self):
        """@device_each('D999') with no match: test still collected but not parametrized."""
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_d999_no_match")
        assert_outcomes(out, passed=1)
