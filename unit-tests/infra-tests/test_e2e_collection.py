# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: Context gating, --live filtering, and priority ordering in a real subprocess.

Tests the full collection pipeline (conftest hooks -> filter_and_sort_items)
by running pytest in a subprocess with mocked hardware.
"""

from helpers import run_e2e, assert_outcomes


class TestContextGatingE2E:
    """@context('nightly') tests should skip/run based on --context."""

    def test_nightly_skipped_by_default(self):
        rc, out, *_ = run_e2e("pytest-context.py", "-k", "test_nightly_only")
        assert_outcomes(out, skipped=1)

    def test_nightly_runs_with_context(self):
        rc, out, *_ = run_e2e("pytest-context.py", "-k", "test_nightly_only", "--context", "nightly")
        assert_outcomes(out, passed=1)

    def test_mixed_context_and_normal(self):
        rc, out, *_ = run_e2e("pytest-context.py")
        assert_outcomes(out, passed=1, skipped=1)


class TestLiveFilteringE2E:
    """--live should skip non-device tests."""

    def test_skips_non_device(self):
        rc, out, *_ = run_e2e("pytest-live.py", "-k", "test_no_device", "--live")
        assert_outcomes(out, skipped=1)

    def test_keeps_device_each(self):
        rc, out, *_ = run_e2e("pytest-live.py", "-k", "test_with_device", "--live")
        assert_outcomes(out, passed=1)

    def test_mixed(self):
        rc, out, *_ = run_e2e("pytest-live.py", "--live")
        assert_outcomes(out, passed=1, skipped=1)


class TestNotLiveFilteringE2E:
    """--not-live should skip device tests (run only the no-hardware ones)."""

    def test_keeps_non_device(self):
        rc, out, *_ = run_e2e("pytest-live.py", "-k", "test_no_device", "--not-live")
        assert_outcomes(out, passed=1)

    def test_skips_device_each(self):
        rc, out, *_ = run_e2e("pytest-live.py", "-k", "test_with_device", "--not-live")
        assert_outcomes(out, skipped=1)

    def test_mixed(self):
        rc, out, *_ = run_e2e("pytest-live.py", "--not-live")
        assert_outcomes(out, passed=1, skipped=1)

    def test_live_and_not_live_mutually_exclusive(self):
        """Passing both --live and --not-live should raise UsageError."""
        rc, out, *_ = run_e2e("pytest-live.py", "--live", "--not-live")
        assert rc != 0
        assert "mutually exclusive" in out.lower()


class TestPriorityOrderingE2E:
    """Tests should execute in priority order (lower first)."""

    def test_priority_order(self):
        rc, out, *_ = run_e2e("pytest-priority.py")
        assert_outcomes(out, passed=4)
