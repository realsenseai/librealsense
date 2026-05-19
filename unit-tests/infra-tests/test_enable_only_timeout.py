# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests that devices.enable_only() raises when it cannot bring the requested
serials online, instead of silently returning. Covers all three branches:

- hub + recycle: TimeoutError when _wait_for times out
- no-hub + no-recycle: TimeoutError when _wait_for times out
- no-hub + recycle: RuntimeError when hw_reset fails (the False return
  there can mean hardware_reset() raised, devices didn't disappear, or
  devices didn't reappear — so "timeout" doesn't accurately describe it)

Before this behavior existed, enable_only would silently swallow these
failures and the test fixture would proceed with a non-existent device,
surfacing later as confusing IndexError / "list index out of range"
failures inside the test body.
"""

import types
import pytest
from unittest.mock import MagicMock

import rspy.devices as dev


@pytest.fixture
def fake_hub_with_device(monkeypatch):
    """Install a fake hub + one fake device on port 4 (D455-like)."""
    fake_device = types.SimpleNamespace(port=4, serial_number='111')
    monkeypatch.setattr(dev, '_device_by_sn', {'111': fake_device})
    monkeypatch.setattr(dev, 'hub', MagicMock())
    monkeypatch.setattr(dev, 'enabled', lambda: set())  # nothing enabled => skip the disable branch
    monkeypatch.setattr(dev, 'time', types.SimpleNamespace(sleep=lambda _: None))
    return fake_device


def test_enable_only_raises_when_wait_for_times_out(monkeypatch, fake_hub_with_device):
    monkeypatch.setattr(dev, '_wait_for', lambda *a, **kw: False)
    with pytest.raises(TimeoutError, match="did not enumerate"):
        dev.enable_only(['111'], recycle=True, timeout=1)


def test_enable_only_succeeds_when_wait_for_returns_true(monkeypatch, fake_hub_with_device):
    monkeypatch.setattr(dev, '_wait_for', lambda *a, **kw: True)
    dev.enable_only(['111'], recycle=True, timeout=1)  # no exception


def test_enable_only_no_hub_raises_when_wait_for_times_out(monkeypatch):
    """Without a hub and without recycle, enable_only still waits for enumeration."""
    monkeypatch.setattr(dev, 'hub', None)
    monkeypatch.setattr(dev, '_wait_for', lambda *a, **kw: False)
    with pytest.raises(TimeoutError, match="did not enumerate"):
        dev.enable_only(['111'], recycle=False, timeout=1)


def test_enable_only_no_hub_recycle_raises_when_hw_reset_fails(monkeypatch):
    """No hub + recycle=True: enable_only delegates to hw_reset; failure should raise.

    hw_reset can return False for several reasons (hardware_reset() raised, devices
    didn't disappear, devices didn't reappear) — so we raise RuntimeError, not
    TimeoutError, since "timeout" doesn't accurately describe all of them.
    """
    monkeypatch.setattr(dev, 'hub', None)
    monkeypatch.setattr(dev, 'time', types.SimpleNamespace(sleep=lambda _: None))
    monkeypatch.setattr(dev, '_device_by_sn', {
        '111': types.SimpleNamespace(port=None, is_dds=False, handle=MagicMock())
    })
    monkeypatch.setattr(dev, '_wait_for', lambda *a, **kw: False)
    monkeypatch.setattr(dev, '_wait_until_removed', lambda *a, **kw: True)
    with pytest.raises(RuntimeError, match="hw_reset failed"):
        dev.enable_only(['111'], recycle=True, timeout=1)
