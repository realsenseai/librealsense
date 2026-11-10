# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

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


def test_install_bridges_and_uninstall_resets(caplog):
    rs = FakeRs()
    with caplog.at_level(logging.DEBUG, logger='librealsense.rs'):
        assert logging_setup.install_rs_log_bridge(rs)
        rs.callback(rs.log_severity.debug, 'udev remove')
    assert 'udev remove' in caplog.text
    logging_setup.uninstall_rs_log_bridge(rs)
    assert rs.callback is None and rs.resets == 1 and logging_setup._rs_log_callback is None


def test_line_delivered_after_uninstall_is_dropped(caplog):
    rs = FakeRs()
    logging_setup.install_rs_log_bridge(rs)
    stale = rs.callback
    logging_setup.uninstall_rs_log_bridge(rs)
    with caplog.at_level(logging.DEBUG, logger='librealsense.rs'):
        stale(rs.log_severity.debug, 'queued before reset')
    assert 'queued before reset' not in caplog.text


def test_failed_install_leaves_no_bridge_behind():
    rs = FakeRs()
    def fail(*args, **kwargs):
        raise RuntimeError('log_to_callback failed')
    rs.log_to_callback = fail
    with pytest.raises(RuntimeError):
        logging_setup.install_rs_log_bridge(rs)
    assert logging_setup._rs_log_callback is None


def test_no_install_when_session_bridge_exists(monkeypatch):
    """Under --rslog the session bridge is already installed: report it, so the caller won't reset it."""
    monkeypatch.setattr(logging_setup, '_rs_log_callback', object())
    assert not logging_setup.install_rs_log_bridge(FakeRs())
