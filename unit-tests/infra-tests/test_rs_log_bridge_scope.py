# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
rs_log_bridge_scope (rspy/pytest/logging_setup.py) bridges LibRS logs into Python logging for
one scope and resets the process-global LibRS logger on exit, so later tests don't inherit it.
"""

import logging
import types

import pytest

from rspy.pytest import logging_setup


class FakeRs:
    log_severity = types.SimpleNamespace(debug=0, info=1, warn=2, error=3, fatal=4)

    def __init__(self):
        self.callback = None
        self.resets = 0

    def log_to_callback(self, severity, callback, asynchronous=False):
        self.callback = callback

    def reset_logger(self):
        self.callback = None
        self.resets += 1


@pytest.fixture(autouse=True)
def no_session_bridge(monkeypatch):
    monkeypatch.setattr(logging_setup, '_rs_log_callback', None)


def test_bridges_inside_and_resets_on_exit(caplog):
    rs = FakeRs()
    with caplog.at_level(logging.DEBUG, logger='librealsense.rs'):
        with logging_setup.rs_log_bridge_scope(rs):
            rs.callback(rs.log_severity.debug, 'udev remove')
    assert 'udev remove' in caplog.text
    assert rs.callback is None and rs.resets == 1
    assert logging_setup._rs_log_callback is None


def test_resets_when_block_raises():
    rs = FakeRs()
    with pytest.raises(RuntimeError):
        with logging_setup.rs_log_bridge_scope(rs):
            raise RuntimeError('test failed')
    assert rs.resets == 1


def test_keeps_session_bridge(monkeypatch):
    """Under --rslog the session bridge is already installed: leave it, and never reset it."""
    monkeypatch.setattr(logging_setup, '_rs_log_callback', object())
    rs = FakeRs()
    with logging_setup.rs_log_bridge_scope(rs):
        pass
    assert rs.resets == 0
