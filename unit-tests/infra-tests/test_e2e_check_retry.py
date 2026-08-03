# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: pytest-check soft checks under reruns.

pytest-check defers failures to makereport. The conftest pytest_runtest_call bridge raises
them in the call phase instead, so they are attributed to the call (not teardown), visible to
the rerun decision, and logged into the per-test .log file on every attempt. These tests lock
in that behavior.
"""

from helpers import run_e2e, parse_outcomes


class TestCheckRetry:

    def test_persistent_soft_check_stays_failed(self):
        """A soft check that fails every attempt ends as a plain FAILED test —
        attributed to the call phase, not a teardown ERROR, and not a false pass."""
        rc, out, *_ = run_e2e("pytest-check-retry.py",
                               "-k", "test_persistent_soft_check", "--reruns", "2")
        assert rc != 0, out
        outcomes = parse_outcomes(out)
        assert outcomes.get("failed") == 1, out
        assert outcomes.get("error", 0) == 0, f"check failure leaked to teardown:\n{out}"
        assert "Failed Checks: 1" in out, out

    def test_flaky_soft_check_passes_on_retry(self):
        """A soft check that fails attempt 1 and passes attempt 2 genuinely PASSES —
        no teardown error, no lingering failure."""
        rc, out, *_ = run_e2e("pytest-check-retry.py",
                               "-k", "test_flaky_soft_check", "--reruns", "2")
        assert rc == 0, out
        outcomes = parse_outcomes(out)
        assert outcomes.get("passed") == 1, out
        assert outcomes.get("error", 0) == 0, out
        assert outcomes.get("failed", 0) == 0, out
        assert outcomes.get("rerun") == 1, f"expected exactly 1 rerun: {out}"
