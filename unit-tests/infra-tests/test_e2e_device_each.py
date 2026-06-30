# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: @device_each parametrization — one test instance per matching device.

Verifies that resolve_device_each_serials creates the right parametrized
instances, respects excludes and CLI filters, and generates correct test IDs.
"""

from helpers import run_e2e, assert_outcomes


class TestDeviceEachParametrization:

    def test_creates_per_device_instances(self):
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_d400 and not exclude")
        assert_outcomes(out, passed=3)  # D455, D435, D401

    def test_with_exclude_marker(self):
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_d400_exclude")
        assert_outcomes(out, passed=2)  # D455, D435

    def test_multiple_markers_union(self):
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_union")
        assert_outcomes(out, passed=2)

    def test_ids_contain_device_name(self):
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_ids")
        assert "D455-111" in out

    def test_device_and_each_both_run(self):
        """device("D455") + device_each("D515") should run two instances, both passing."""
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_device_and_each")
        assert_outcomes(out, passed=2)

    def test_device_missing_sentinel_fails(self):
        """device("D999") not found → one instance errors (fixture fail); device_each("D455") → one passes."""
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_device_missing_and_each")
        assert_outcomes(out, passed=1, error=1)

    def test_missing_sentinel_id(self):
        """The missing-device instance should have a human-readable MISSING-... test ID."""
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_device_missing_and_each")
        assert "MISSING-D999" in out
