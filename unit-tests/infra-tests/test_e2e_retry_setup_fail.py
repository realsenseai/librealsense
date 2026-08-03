# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: a module-scoped fixture that fails setup must be retried, recover, and leave the REAL
error diagnosable — never a masking KeyError.

pytest-rerunfailures reruns the whole protocol, so a failed setup is retried natively and
pytest_runtest_makereport fires on every attempt; the conftest makereport hook logs each
setup-phase failure into the per-test .log file. (Under the old pytest-retry engine a failed
re-setup surfaced as `KeyError: '<fixture>'`, hiding the real error — this guard keeps that
regression from coming back with any engine.)
"""

from helpers import run_e2e, parse_outcomes


class TestRetrySetupFail:

    def test_module_fixture_setup_fail_surfaces_real_error(self):
        """Fixture fails setup on attempts 1 & 2 (real RuntimeError) and recovers on attempt 3.
        The run must recover cleanly and record the real error, never a masking KeyError."""
        rc, out, tracking = run_e2e("pytest-retry-module-setup-fail.py", "--reruns", "2")
        assert rc == 0, out
        outcomes = parse_outcomes(out)
        assert outcomes.get("passed") == 1, out
        assert outcomes.get("rerun") == 2, f"expected 2 reruns: {out}"
        assert "KeyError" not in out, f"masking KeyError leaked:\n{out}"
        logs = tracking.get("logs", {})
        module_log = logs.get("pytest-retry-module-setup-fail.log", "")
        assert "intentional module-fixture setup failure" in module_log, \
            f"real setup error missing from per-test log; logs: {list(logs)}"
