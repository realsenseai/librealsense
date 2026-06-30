# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy/pytest/log_live_format.py.

Verifies that `log.debug("%s", obj)` materializes the formatted string **live**
(synchronously, at the log call) rather than deferring it to emit time. This
means pytest's LogCaptureHandler, which retains every LogRecord for the test's
captured-logs report, stores only the formatted string -- not a reference to
`obj`.

Why we care: rs.frame objects come from a bounded syncer publish pool (default
16 slots). Any retained Python reference blocks pool reclamation; once full,
the matcher silently drops framesets. Before the fix in log_live_format.py,
the default pytest + stdlib logging combination pinned each rs.frame for the
duration of the test and broke syncer-based tests under --debug. See PR #14962.

Three tests, each exercises one property the fix must hold:
  - args cleared + msg materialized (direct assertion)
  - retained record doesn't pin arg (weakref proof)
  - format mismatch doesn't crash our filter
"""

import logging
import weakref

from rspy.pytest.log_live_format import install as install_live


class _RetainingHandler(logging.Handler):
    """Mimics pytest's LogCaptureHandler: appends every record to a list."""
    def __init__(self):
        super().__init__(logging.DEBUG)
        self.records = []
    def emit(self, record):
        self.records.append(record)


class _Token:
    """Arbitrary Python object used to stand in for rs.frame in tests."""
    pass


class TestLogLiveFormat:

    def setup_method(self):
        # Fresh logger per test, DEBUG-enabled, with a retaining handler.
        # install() is idempotent (real-world it's called from pytest_configure).
        install_live()
        self.logger = logging.getLogger(f"test_log_live_format.{id(self)}")
        self.logger.setLevel(logging.DEBUG)
        self.handler = _RetainingHandler()
        self.logger.addHandler(self.handler)
        self.logger.propagate = False

    def teardown_method(self):
        self.logger.removeHandler(self.handler)

    def test_args_cleared_after_log(self):
        """Direct proof: `record.msg` is already the formatted string and
        `record.args is None` by the time `log.debug(...)` returns.
        """
        token = _Token()
        self.logger.debug("got %s", token)

        assert len(self.handler.records) == 1
        rec = self.handler.records[0]
        assert rec.args is None, "args should be cleared by live-format patch"
        assert rec.msg == f"got {token}"

    def test_retained_record_doesnt_pin_arg(self):
        """Weakref proof: even though the handler retains the LogRecord, the
        original arg object can be garbage-collected once the log call returns.
        This is the load-bearing test -- it verifies the actual bug is fixed.
        """
        token = _Token()
        wref = weakref.ref(token)

        self.logger.debug("got %s", token)
        del token  # our only strong ref -- if LogRecord pinned it, weakref stays alive

        import gc; gc.collect()
        assert wref() is None, (
            "arg object should be collectable once the log call returns; "
            "if it's still alive, LogRecord is pinning it via args"
        )
        assert len(self.handler.records) == 1  # sanity: record itself retained

    def test_format_mismatch_doesnt_crash_in_filter(self):
        """Edge case: `log.debug("Got", f)` with no %s placeholder raises
        TypeError during getMessage(). Our filter must swallow that so the
        caller gets no exception from inside `_live_handle`. Downstream
        handlers still hit the same TypeError in their own emit() and report
        it via Handler.handleError (which under pytest re-raises to fail the
        test -- desired).
        """
        token = _Token()
        self.logger.debug("Got", token)  # no %s -- "Got" % (token,) raises TypeError
        # Our _RetainingHandler uses the default handleError (print to stderr,
        # no re-raise), so the caller sees no exception here.
        assert len(self.handler.records) == 1
