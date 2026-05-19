# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Live-format LogRecord args so pytest's log-capture doesn't pin arg objects.

Scope
-----
TEST INFRASTRUCTURE ONLY. This module is imported by `unit-tests/conftest.py`
during `pytest_configure`. It is **not** packaged or shipped in the
`realsense2` runtime library; no consumer application runs this code. It is
part of the pytest-only harness living under `unit-tests/py/rspy/pytest/`.

Problem
-------
Python's stdlib logging defers message formatting until a handler emits, so
`LogRecord` stores `msg` + `args` separately. Pytest's `LogCaptureHandler`
retains every emitted `LogRecord` in a list for the duration of the test (so
captured logs can be displayed on failure). That list keeps references to
anything passed in `args` -- including `rs.frame` objects.

For librealsense that's a real problem: `rs.syncer`'s processing-block
publish pool defaults to 16 slots. Each retained `rs.frame` keeps
`published_frames_count` elevated. After 16 retained frames,
`publish_frame()` returns null, `allocate_composite_frame()` returns null,
and the matcher silently drops framesets -- making frame-sync tests fail
deterministically under `--debug`.

Intent
------
Restore the legacy `rspy.log` behavior that was lost in the stdlib-logging
migration. `rspy.log.d(*args)` always called `str()` on every arg
synchronously and wrote the result -- it never held references. This patch
makes stdlib `logging` symmetric with that behavior for the pytest session.

Fix
---
Monkey-patch `logging.Logger.handle` so that, for any record carrying args,
we materialize `msg` via `record.getMessage()` (which interpolates args into
the format string) and then drop `args`. Downstream handlers see a record
with a pre-formatted `msg` and `args = None`.

Safety audit
------------
Grepped the repo for any code that reads `LogRecord.args` after emission:
  - No `caplog.records[i].args` usage anywhere
  - No custom `logging.Filter` / `logging.Handler` subclasses inspecting args
  - `logging_setup.py` only uses stdlib `FileHandler` with the default
    formatter (which uses `record.message`, already materialized by us)
If future code needs arg details after emission, it must read the formatted
`record.message` (or `record.msg` after our patch runs), not `record.args`.

Trade-offs
----------
- Forces formatting even when no handler would emit. Mitigated by the level
  check in `Logger.debug/info/...` still running first -- `handle()` is only
  called when the record passed its level gate.
- Under `--debug`, at least one handler (LogCaptureHandler) was already
  going to call `getMessage()`. We're just doing it once, immediately, and
  caching the result.
- Alternative narrower scope: monkey-patching `_pytest.logging.LogCaptureHandler.emit`
  specifically. Rejected because `_pytest.logging` is pytest private API --
  fragile across pytest versions. The stdlib hook is stable.
"""

import logging


_orig_handle = logging.Logger.handle
_installed = False


def _live_handle( self, record ):
    if record.args:
        try:
            record.msg = record.getMessage()
            record.args = None
        except Exception:
            # Format mismatch (e.g. log.debug("Got", f) with no %s placeholder).
            # Leave msg/args alone so the downstream handler's normal error
            # path (Handler.handleError) reports it -- don't crash the caller.
            pass
    return _orig_handle( self, record )


def install():
    """Install the live-format patch on logging.Logger.handle. Idempotent."""
    global _installed
    if _installed:
        return
    logging.Logger.handle = _live_handle
    _installed = True
