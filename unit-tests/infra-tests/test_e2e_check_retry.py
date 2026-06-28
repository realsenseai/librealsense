# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: pytest-check soft checks under pytest-retry.

pytest-check defers failures to makereport; pytest-retry reruns a test via
pytest_runtest_call + TestReport.from_item_and_call, bypassing makereport. Without
the conftest pytest_runtest_call bridge a failed soft check looks "passed" to the
retry decision yet leaks onto the teardown report, so the run fails with a teardown
error on a test reported as passed. These tests lock in the fixed behavior.
"""

from helpers import run_e2e, assert_outcomes, parse_outcomes


class TestCheckRetry:

    def test_persistent_soft_check_stays_failed(self):
        """A soft check that fails every attempt ends as a plain FAILED test —
        attributed to the call phase, not a teardown ERROR, and not a false pass."""
        rc, out, *_ = run_e2e("pytest-check-retry.py",
                               "-k", "test_persistent_soft_check", "--retries", "2")
        assert rc != 0, out
        outcomes = parse_outcomes(out)
        assert outcomes.get("failed") == 1, out
        assert outcomes.get("error", 0) == 0, f"check failure leaked to teardown:\n{out}"
        assert "Failed Checks: 1" in out, out

    def test_flaky_soft_check_passes_on_retry(self):
        """A soft check that fails attempt 1 and passes attempt 2 genuinely PASSES —
        no teardown error, no lingering failure."""
        rc, out, *_ = run_e2e("pytest-check-retry.py",
                               "-k", "test_flaky_soft_check", "--retries", "2")
        assert rc == 0, out
        outcomes = parse_outcomes(out)
        assert outcomes.get("passed") == 1, out
        assert outcomes.get("error", 0) == 0, out
        assert outcomes.get("failed", 0) == 0, out
        assert "passed on attempt 2" in out, out
