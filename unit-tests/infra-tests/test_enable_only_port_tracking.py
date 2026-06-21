# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Regression test for the core port-tracking fix (RSDSO-21630).

enable_only(recycle=True) must decide which ports to disable from _enabled_ports -- the set of
ports WE powered on -- NOT from enabled() (the SDK's view of present devices). The two diverge
when a device is powered but momentarily absent from the SDK, e.g. a DDS device (D555) rebooting
after a FW update: its port is still on, but enabled() doesn't list it.

Before the fix, enable_only derived the disable set from enabled(), so such a port was left
powered (it lingered on the DDS domain and broke unrelated tests). These tests pin the new
behavior: the powered port is disabled even though no device for it is SDK-visible.
"""

import types
import pytest
from unittest.mock import MagicMock

import rspy.devices as dev


def test_recycle_disables_tracked_port_absent_from_sdk(monkeypatch):
    """A port in _enabled_ports but absent from enabled() must still be disabled on recycle."""
    # Target device we want online, on port 0.
    target = types.SimpleNamespace(port=0, serial_number='111')
    monkeypatch.setattr(dev, '_device_by_sn', {'111': target})
    # SDK currently sees nothing (e.g. the port-8 device is mid-reboot after a FW flash).
    monkeypatch.setattr(dev, 'enabled', lambda: set())
    monkeypatch.setattr(dev, '_wait_for', lambda *a, **kw: True)
    monkeypatch.setattr(dev, 'time', types.SimpleNamespace(sleep=lambda _: None))
    # We previously powered ports 0 and 8 (8 = the now-rebooting DDS device).
    monkeypatch.setattr(dev, '_enabled_ports', {0, 8})
    fake_hub = MagicMock()
    monkeypatch.setattr(dev, 'hub', fake_hub)

    dev.enable_only(['111'], recycle=True, timeout=1)

    # Recycle disables exactly the ports we know are powered -- including port 8, which
    # enabled() never reported. (The old enabled()-based logic would have left 8 on.)
    assert fake_hub.disable_ports.call_count == 1
    assert sorted(fake_hub.disable_ports.call_args.args[0]) == [0, 8]
    # ...then re-enables only the wanted port.
    assert fake_hub.enable_ports.call_args.args[0] == [0]


def test_recycle_with_nothing_powered_does_not_disable(monkeypatch):
    """Empty _enabled_ports (nothing powered, the normal post-map_unknown_ports state) means
    there is nothing to disable -- enable_only just enables the wanted port."""
    target = types.SimpleNamespace(port=0, serial_number='111')
    monkeypatch.setattr(dev, '_device_by_sn', {'111': target})
    monkeypatch.setattr(dev, 'enabled', lambda: set())
    monkeypatch.setattr(dev, '_wait_for', lambda *a, **kw: True)
    monkeypatch.setattr(dev, 'time', types.SimpleNamespace(sleep=lambda _: None))
    monkeypatch.setattr(dev, '_enabled_ports', set())
    fake_hub = MagicMock()
    monkeypatch.setattr(dev, 'hub', fake_hub)

    dev.enable_only(['111'], recycle=True, timeout=1)

    fake_hub.disable_ports.assert_not_called()
    assert fake_hub.enable_ports.call_args.args[0] == [0]


def test_disable_failure_keeps_port_tracked(monkeypatch):
    """If the hub's disable call fails, the port must stay in _enabled_ports so a later
    recycle retries it (a failed disable may have left the port powered)."""
    monkeypatch.setattr(dev, '_enabled_ports', {8})
    fake_hub = MagicMock()
    fake_hub.disable_ports.return_value = False  # hub reports failure
    monkeypatch.setattr(dev, 'hub', fake_hub)

    ok = dev._disable_hub_ports([8])

    assert ok is False
    assert dev._enabled_ports == {8}  # not dropped -- still considered powered
