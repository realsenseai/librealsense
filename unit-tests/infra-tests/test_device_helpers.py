# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy/pytest/device_helpers.py (find_matching_devices).

Verifies how device markers resolve to serial numbers:
- Exact name match: device_each("D455") finds serial 111
- Wildcard by product line: device_each("D400*") finds all D400-family devices
- Exclusion: device_exclude("D401") removes a device from results
- CLI filters: --device and --exclude-device narrow results
- Union: multiple device_each markers combine their matches
- Deduplication: overlapping patterns don't produce duplicate serials
"""

import pytest
from unittest.mock import MagicMock
from rspy.pytest.device_helpers import (
    find_matching_devices, find_matching_devices_multi,
    resolve_device_each_serials,
    _MISSING_SENTINEL_PREFIX, _SKIP_SENTINEL_PREFIX,
)
from helpers import fake_by_spec, fake_get, make_device_marker, DEVICE_CONNECTION_TYPES


class TestFindMatchingDevices:
    """Device pattern resolution: wildcards, excludes, CLI filters."""

    @pytest.fixture(autouse=True)
    def _patch_devices(self):
        """Temporarily replace rspy.devices.by_spec/get with our fakes."""
        import rspy.devices as dev
        orig_by_spec, orig_get = dev.by_spec, dev.get
        dev.by_spec, dev.get = fake_by_spec, fake_get
        yield
        dev.by_spec, dev.get = orig_by_spec, orig_get

    def test_exact_name(self):
        sns, had = find_matching_devices([make_device_marker('device_each', 'D455')], each=True)
        assert sns == ['111'] and had is True

    def test_wildcard(self):
        """D400* should match all 6 devices in the D400 product line."""
        sns, had = find_matching_devices([make_device_marker('device_each', 'D400*')], each=True)
        assert len(sns) == 6 and '111' in sns and '222' in sns and had is True

    def test_exclude_marker(self):
        markers = [make_device_marker('device_each', 'D400*'),
                   make_device_marker('device_exclude', 'D401')]
        sns, _ = find_matching_devices(markers, each=True)
        assert '777' not in sns and '111' in sns

    def test_cli_include(self):
        sns, _ = find_matching_devices(
            [make_device_marker('device_each', 'D400*')], each=True, cli_includes=['D455'])
        assert sns == ['111']

    def test_cli_exclude(self):
        sns, _ = find_matching_devices(
            [make_device_marker('device_each', 'D400*')], each=True, cli_excludes=['D455'])
        assert '111' not in sns and '222' in sns

    def test_each_false_returns_first_only(self):
        sns, had = find_matching_devices([make_device_marker('device', 'D400*')], each=False)
        assert len(sns) == 1 and had is True

    def test_no_match(self):
        sns, had = find_matching_devices([make_device_marker('device_each', 'D999')], each=True)
        assert sns == [] and had is False

    def test_multiple_markers_union(self):
        markers = [make_device_marker('device_each', 'D455'),
                   make_device_marker('device_each', 'D515')]
        sns, _ = find_matching_devices(markers, each=True)
        assert set(sns) == {'111', '555'}

    def test_exclude_plus_cli_include(self):
        markers = [make_device_marker('device_each', 'D400*'),
                   make_device_marker('device_exclude', 'D401')]
        sns, _ = find_matching_devices(markers, each=True, cli_includes=['D455', 'D435'])
        assert set(sns) == {'111', '222'}

    def test_no_duplicates(self):
        """D455 + D400* (which includes D455) should not produce duplicate serials."""
        markers = [make_device_marker('device_each', 'D455'),
                   make_device_marker('device_each', 'D400*')]
        sns, _ = find_matching_devices(markers, each=True)
        assert sns.count('111') == 1


class TestConnectionTypeFiltering:
    """device_type / device_type_exclude marker filtering."""

    @pytest.fixture(autouse=True)
    def _patch_devices(self):
        import rspy.devices as dev
        orig_by_spec, orig_get = dev.by_spec, dev.get
        dev.by_spec, dev.get = fake_by_spec, fake_get
        yield
        dev.by_spec, dev.get = orig_by_spec, orig_get

    def test_device_type_exclude_gmsl(self):
        """device_type_exclude("GMSL") should remove D457 (serial 888) from D400* results."""
        markers = [make_device_marker('device_each', 'D400*'),
                   make_device_marker('device_type_exclude', 'GMSL')]
        sns, _ = find_matching_devices(markers, each=True)
        assert '888' not in sns   # D457 is GMSL
        assert '111' in sns       # D455 is USB, kept

    def test_device_type_include_gmsl(self):
        """device_type("GMSL") should return only D457 from D400* results."""
        markers = [make_device_marker('device_each', 'D400*'),
                   make_device_marker('device_type', 'GMSL')]
        sns, _ = find_matching_devices(markers, each=True)
        assert sns == ['888']     # only D457

    def test_device_type_include_usb(self):
        """device_type("USB") should exclude D457 (GMSL) from D400* results."""
        markers = [make_device_marker('device_each', 'D400*'),
                   make_device_marker('device_type', 'USB')]
        sns, _ = find_matching_devices(markers, each=True)
        assert '888' not in sns
        assert '111' in sns

    def test_device_type_unknown_excluded_by_required(self):
        """device_type("DDS") with no DDS devices should return empty."""
        markers = [make_device_marker('device_each', 'D400*'),
                   make_device_marker('device_type', 'DDS')]
        sns, _ = find_matching_devices(markers, each=True)
        assert sns == []


class TestFindMatchingDevicesMulti:
    """Multi-device marker resolution: device("D400*", "D400*") etc."""

    @pytest.fixture(autouse=True)
    def _patch_devices(self):
        import rspy.devices as dev
        orig_by_spec, orig_get = dev.by_spec, dev.get
        dev.by_spec, dev.get = fake_by_spec, fake_get
        yield
        dev.by_spec, dev.get = orig_by_spec, orig_get

    def test_two_d400_devices(self):
        """device("D400*", "D400*") should return 2 unique D400 serial numbers."""
        markers = [make_device_marker('device', 'D400*', 'D400*')]
        sns, had = find_matching_devices_multi(markers)
        assert len(sns) == 2
        assert sns[0] != sns[1]
        assert had is True

    def test_d400_plus_d500(self):
        """device("D400*", "D500*") should return one D400 and one D500."""
        from helpers import DEVICES
        markers = [make_device_marker('device', 'D400*', 'D500*')]
        sns, had = find_matching_devices_multi(markers)
        assert len(sns) == 2
        d400_sns = {sn for name, (sn, pl) in DEVICES.items() if pl == 'D400'}
        d500_sns = {sn for name, (sn, pl) in DEVICES.items() if pl == 'D500'}
        assert sns[0] in d400_sns
        assert sns[1] in d500_sns

    def test_specific_devices(self):
        """device("D455", "D435") should return exact matches."""
        markers = [make_device_marker('device', 'D455', 'D435')]
        sns, _ = find_matching_devices_multi(markers)
        assert sns == ['111', '222']

    def test_three_devices(self):
        """device("D400*", "D400*", "D400*") should return 3 unique D400 devices."""
        markers = [make_device_marker('device', 'D400*', 'D400*', 'D400*')]
        sns, _ = find_matching_devices_multi(markers)
        assert len(sns) == 3
        assert len(set(sns)) == 3  # all unique

    def test_insufficient_devices(self):
        """device("D500*", "D500*", "D500*") with only 2 D500 devices should return fewer than requested."""
        markers = [make_device_marker('device', 'D500*', 'D500*', 'D500*')]
        sns, had = find_matching_devices_multi(markers)
        assert len(sns) < 3  # only D515 and D555 exist, requested 3
        assert had is True

    def test_with_exclusion(self):
        """device("D400*", "D400*") + device_exclude("D455") should skip D455."""
        markers = [make_device_marker('device', 'D400*', 'D400*'),
                   make_device_marker('device_exclude', 'D455')]
        sns, _ = find_matching_devices_multi(markers)
        assert len(sns) == 2
        assert '111' not in sns  # D455 serial excluded

    def test_no_device_marker(self):
        """No device marker should return empty."""
        markers = [make_device_marker('device_exclude', 'D455')]
        sns, had = find_matching_devices_multi(markers)
        assert sns == [] and had is False


# =============================================================================
# resolve_device_each_serials — skip vs MISSING sentinel
# =============================================================================
#
# When device() coexists with device_each() the mandatory device() spec is
# resolved at collection time via resolve_device_each_serials.  Two outcomes:
#
#   had_raw_match=True  (device exists, all filtered) → _SKIP_SENTINEL_PREFIX
#   had_raw_match=False (no device of that type)      → _MISSING_SENTINEL_PREFIX
#
# These mirror the non-parametrized path in module_device_setup which calls
# pytest.skip() when had_candidates=True and pytest.fail() when False.
#
# Simulated Jetson lab: only D401 present, GMSL connection type.

def _make_metafunc(markers, cli_device=None, cli_exclude=None):
    """Minimal fake metafunc for resolve_device_each_serials."""
    metafunc = MagicMock()
    metafunc.definition.iter_markers.side_effect = lambda name=None: (
        [m for m in markers if name is None or m.name == name]
    )
    opts = {'--device': cli_device or [], '--exclude-device': cli_exclude or []}
    metafunc.config.getoption = lambda key, default=None: opts.get(key, default)
    captured = {}

    def fake_parametrize(name, values, ids=None, scope=None):
        captured['values'] = list(values)
        captured['ids'] = list(ids or [])

    metafunc.parametrize = fake_parametrize
    metafunc.fixturenames = []
    return metafunc, captured


class TestResolveDeviceEachSentinels:
    """Skip vs MISSING sentinel emitted by resolve_device_each_serials."""

    @pytest.fixture(autouse=True)
    def _patch_devices(self):
        import rspy.devices as dev
        orig_by_spec, orig_get = dev.by_spec, dev.get
        dev.by_spec, dev.get = fake_by_spec, fake_get
        yield
        dev.by_spec, dev.get = orig_by_spec, orig_get

    # Helpers shared by both single-device Jetson scenarios below.
    # Both simulate a lab where D401 (serial '777') is the only device present.

    @staticmethod
    def _d401_only_by_spec(pattern, ignored):
        """Return D401 for D400* and any direct D401 lookup; nothing else."""
        if pattern in ('D400*', 'D401'):
            yield '777'

    # --- type-exclude case (y16-calibration scenario) ---

    def test_type_excluded_emits_skip_not_missing(self):
        """device("D400*") + device_each("D555") + device_type_exclude("GMSL"):
        D401 (GMSL) is the only D400 device.  It is found by by_spec("D400*") but
        rejected by the GMSL type filter → had_raw_match=True, found_sn=None
        → SKIP sentinel, not MISSING."""
        from helpers import FakeDevice
        markers = [
            make_device_marker('device', 'D400*'),
            make_device_marker('device_each', 'D555'),       # finds nothing
            make_device_marker('device_type_exclude', 'GMSL'),
        ]
        import rspy.devices as dev
        orig_by_spec, orig_get = dev.by_spec, dev.get

        def get_d401_gmsl(sn):
            return FakeDevice('777', 'D401', connection_type='GMSL') if sn == '777' else fake_get(sn)

        dev.by_spec = self._d401_only_by_spec
        dev.get = get_d401_gmsl
        try:
            metafunc, captured = _make_metafunc(markers)
            resolve_device_each_serials(metafunc)
        finally:
            dev.by_spec, dev.get = orig_by_spec, orig_get

        values = captured.get('values', [])
        assert any(v.startswith(_SKIP_SENTINEL_PREFIX) for v in values), \
            f"Expected SKIP sentinel, got {values}"
        assert not any(v.startswith(_MISSING_SENTINEL_PREFIX) for v in values)

    # --- name-exclude case (backend-vs-frame-timestamp test_color scenario) ---

    def test_name_excluded_emits_skip_not_missing(self):
        """device("D400*") + device_each("D500*") + device_exclude("D401"):
        D401 is the only D400 device.  It is found by by_spec("D400*") but placed
        in excluded_sns (name match) → had_raw_match=True, found_sn=None
        → SKIP sentinel, not MISSING."""
        markers = [
            make_device_marker('device', 'D400*'),
            make_device_marker('device_each', 'D500*'),      # finds nothing
            make_device_marker('device_exclude', 'D401'),
        ]
        import rspy.devices as dev
        orig_by_spec = dev.by_spec
        dev.by_spec = self._d401_only_by_spec   # D500* → nothing; D400*/D401 → '777'
        try:
            metafunc, captured = _make_metafunc(markers)
            resolve_device_each_serials(metafunc)
        finally:
            dev.by_spec = orig_by_spec

        values = captured.get('values', [])
        assert any(v.startswith(_SKIP_SENTINEL_PREFIX) for v in values), \
            f"Expected SKIP sentinel, got {values}"
        assert not any(v.startswith(_MISSING_SENTINEL_PREFIX) for v in values)

    # --- no candidates at all → MISSING ---

    def test_no_candidates_emits_missing(self):
        """device("D999") + device_each("D888"): nothing in the lab matches either
        pattern → MISSING sentinel (device absent from lab → fail, not skip)."""
        markers = [
            make_device_marker('device', 'D999'),
            make_device_marker('device_each', 'D888'),
        ]
        metafunc, captured = _make_metafunc(markers)
        resolve_device_each_serials(metafunc)

        values = captured.get('values', [])
        assert any(v.startswith(_MISSING_SENTINEL_PREFIX) for v in values), \
            f"Expected MISSING sentinel, got {values}"
        assert not any(v.startswith(_SKIP_SENTINEL_PREFIX) for v in values)

    # --- device found and passes → real serial, no sentinel ---

    def test_matching_device_no_sentinel(self):
        """device("D455") + device_each("D999"): D455 found and passes → real serial."""
        markers = [
            make_device_marker('device', 'D455'),
            make_device_marker('device_each', 'D999'),
        ]
        metafunc, captured = _make_metafunc(markers)
        resolve_device_each_serials(metafunc)

        assert captured.get('values') == ['111']
