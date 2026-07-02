# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: a failing test records its exception in its per-test log file.

Regression for empty failing-test logs (Jetson #17522): a test whose call phase
raised left a .log ending on just the "Test:" header. Two effects combined --
pytest_runtest_makereport was the only hook that logged failures, and pytest-retry
reruns a test via pytest_runtest_call (never makereport) after reopening the module
log in 'w' mode, truncating the original attempt's logged failure. conftest now logs
the call-phase failure from pytest_runtest_call, which fires on every attempt.
"""

import os
from helpers import run_e2e, parse_outcomes

_LOGDIR = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'logs'))
_LOGFILE = os.path.join(_LOGDIR, 'pytest-logfail_D455-111.log')


def _cleanup():
    try:
        os.remove(_LOGFILE)
    except OSError:
        pass


def _read_log():
    assert os.path.exists(_LOGFILE), f"per-test log not created at {_LOGFILE}"
    with open(_LOGFILE) as f:
        return f.read()


class TestLogFailures:

    def test_call_failure_recorded(self):
        """A failing call-phase test writes its exception into the per-test log,
        exactly once (makereport no longer double-logs the call phase)."""
        _cleanup()
        rc, out, *_ = run_e2e("pytest-logfail.py")
        assert rc != 0, out
        assert parse_outcomes(out).get("failed") == 1, out
        log = _read_log()
        assert "call failed: RuntimeError: xioctl" in log, log
        assert "errno=22" in log, log
        assert log.count("call failed: RuntimeError") == 1, f"duplicate failure log line:\n{log}"
        _cleanup()

    def test_call_failure_recorded_under_retry(self):
        """Under --retries the module log is truncated + reopened per attempt and
        makereport is bypassed; the failure must still survive in the final file."""
        _cleanup()
        rc, out, *_ = run_e2e("pytest-logfail.py", "--retries", "2")
        assert rc != 0, out
        assert parse_outcomes(out).get("failed") == 1, out
        log = _read_log()
        assert "call failed: RuntimeError: xioctl(VIDIOC_G_EXT_CTRLS) failed, errno=22" in log, log
        _cleanup()
