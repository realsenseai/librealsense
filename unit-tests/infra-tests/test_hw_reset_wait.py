# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Guards the hub-less recycle timing in rspy.devices: hw_reset() waits on the device removal and
re-add events (real for USB/DDS, simulated by the SDK for MIPI) instead of a fixed sleep, and the
waits notice a change within POLL_INTERVAL rather than on whole-second polls.
"""

import types
from unittest.mock import MagicMock

import rspy.devices as dev


def test_hw_reset_waits_on_events_not_fixed_sleep(monkeypatch):
    """A MIPI-like device (no hub port, not DDS) must not fall back to a blind sleep."""
    sleeps, calls = [], []
    handle = MagicMock()
    monkeypatch.setattr(dev, 'time', types.SimpleNamespace(sleep=sleeps.append))
    monkeypatch.setattr(dev, '_device_by_sn', {'111': types.SimpleNamespace(port=None, is_dds=False, handle=handle)})
    monkeypatch.setattr(dev, '_wait_until_removed', lambda sns, **kw: calls.append(('removed', list(sns))) or True)
    monkeypatch.setattr(dev, '_wait_for', lambda sns, **kw: calls.append(('added', list(sns))) or True)
    assert dev.hw_reset(['111'])
    handle.hardware_reset.assert_called_once()
    assert calls == [('added', ['111']), ('removed', ['111']), ('added', ['111'])]
    assert sleeps == []


def test_wait_for_polls_at_poll_interval_then_settles(monkeypatch):
    """A device that shows up after 3 polls is seen on the next poll, then gets the 1s settle."""
    sleeps = []
    monkeypatch.setattr(dev, 'time', types.SimpleNamespace(sleep=sleeps.append))
    monkeypatch.setattr(dev, 'enabled', lambda: {'111'} if len(sleeps) >= 3 else set())
    assert dev._wait_for(['111'], timeout=5)
    assert sleeps == [dev.POLL_INTERVAL] * 3 + [1]


def test_wait_until_removed_polls_at_poll_interval(monkeypatch):
    sleeps = []
    monkeypatch.setattr(dev, 'time', types.SimpleNamespace(sleep=sleeps.append))
    monkeypatch.setattr(dev, 'enabled', lambda: set() if len(sleeps) >= 2 else {'111'})
    assert dev._wait_until_removed(['111'], timeout=5)
    assert sleeps == [dev.POLL_INTERVAL] * 2


def test_waits_still_time_out(monkeypatch):
    """Timeouts stay in seconds: ~timeout/POLL_INTERVAL polls, then False."""
    sleeps = []
    monkeypatch.setattr(dev, 'time', types.SimpleNamespace(sleep=sleeps.append))
    monkeypatch.setattr(dev, 'enabled', lambda: set())
    assert not dev._wait_for(['111'], timeout=1)
    assert 1 <= sum(sleeps) <= 1 + dev.POLL_INTERVAL + 1e-9
