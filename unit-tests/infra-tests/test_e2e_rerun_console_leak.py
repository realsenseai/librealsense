# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: the between-attempts recycle must not leak raw teardown stdout into the console.

The rerun-recycle hook fires in pytest_runtest_logreport, between protocol phases, where
pytest's output capture is suspended — fixture-teardown prints (e.g. rspy's "-D- Disabling
ports..." hub lines) would otherwise splatter into the CI console mid-progress-line. The
conftest hook swallows stdout for the duration; the python-logging bridge still records the
lines in the per-test .log file (covered by test_e2e_log_failures).
"""

from helpers import run_e2e, parse_outcomes


class TestRerunConsoleLeak:

    def test_recycle_teardown_stdout_not_leaked(self):
        """Attempt 1 fails → recycle runs the noisy module-fixture teardown between
        attempts → its raw print must NOT appear in the console output, while the
        recycle itself must still have happened (device disable tracked)."""
        rc, out, tracking = run_e2e("pytest-rerun-teardown-print.py", "--reruns", "1")
        assert rc == 0, out
        outcomes = parse_outcomes(out)
        assert outcomes.get("passed") == 1, out
        assert outcomes.get("rerun") == 1, f"expected exactly 1 rerun: {out}"
        assert "RAW-TEARDOWN-LEAK-MARKER" not in out, \
            f"raw teardown stdout leaked into console:\n{out}"
        assert len(tracking.get("disable_calls", [])) >= 1, \
            "recycle did not run the module fixture teardown between attempts"
