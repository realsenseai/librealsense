# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: a failing test records its exception in its per-test log file.

Regression for empty failing-test logs (Jetson #17522): a test whose call phase
raised left a .log ending on just the "Test:" header. conftest logs the call-phase
failure from pytest_runtest_call (one place that fires on every rerun attempt), and
the module log is opened in append mode so reruns/repeats accumulate instead of
overwriting (the between-attempts recycle finishes module_log; it reopens per attempt).
"""

from helpers import run_e2e, parse_outcomes

_LOG_NAME = "pytest-logfail_D455-111.log"


def _log(tracking):
    logs = tracking.get("logs", {})
    assert _LOG_NAME in logs, f"per-test log {_LOG_NAME} not created; got {list(logs)}"
    return logs[_LOG_NAME]


class TestLogFailures:

    def test_call_failure_and_teardown_recorded(self):
        """A failing call-phase test writes its exception into the per-test log,
        exactly once (makereport no longer double-logs the call phase), followed by
        the teardown marker."""
        rc, out, tracking = run_e2e("pytest-logfail.py")
        assert rc != 0, out
        assert parse_outcomes(out).get("failed") == 1, out
        log = _log(tracking)
        assert "call failed: RuntimeError: xioctl" in log, log
        assert "errno=22" in log, log
        assert log.count("call failed: RuntimeError") == 1, f"duplicate failure log line:\n{log}"
        assert "Teardown:" in log, log   # common prefix across hub / hub-less / --no-reset paths

    def test_every_retry_attempt_recorded_in_one_file(self):
        """Under --reruns the between-attempts recycle finishes module_log and it reopens
        in append mode per attempt; every attempt's failure AND teardown must accumulate
        in the one file (not overwritten, not empty). --reruns 2 == 3 attempts."""
        rc, out, tracking = run_e2e("pytest-logfail.py", "--reruns", "2")
        assert rc != 0, out
        assert parse_outcomes(out).get("failed") == 1, out
        log = _log(tracking)
        assert log.count("call failed: RuntimeError: xioctl(VIDIOC_G_EXT_CTRLS) failed, errno=22") == 3, log
        assert log.count("Teardown:") == 3, log

    def test_every_repeat_pass_recorded_in_one_file(self):
        """Under --repeat the module runs multiple passes; append mode must accumulate every
        pass's failure + teardown in the one file. --repeat 2 == 2 passes."""
        rc, out, tracking = run_e2e("pytest-logfail.py", "--repeat", "2")
        assert rc != 0, out
        log = _log(tracking)
        assert log.count("call failed: RuntimeError: xioctl(VIDIOC_G_EXT_CTRLS) failed, errno=22") == 2, log
        assert log.count("Teardown:") == 2, log
