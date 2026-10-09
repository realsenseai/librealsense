# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy/device_map.py (device inventory + health classification).

No cameras or pyrealsense2 required -- rspy.devices module state is mocked.
Covers: map loading/normalization, node resolution, pre-exclusion spec matching,
the ok/in-recovery/missing classification, the no-fwid recovery heuristic,
unexpected-device reporting, run statuses, and the health.json/.txt output.
"""

import json
import types

import pytest

from rspy import device_map, devices


# =============================================================================
# Fakes / fixtures
# =============================================================================

class FakeHandle:
    def __init__(self, in_recovery, fwid=None):
        self._in_recovery = in_recovery
        self._fwid = fwid

    def is_in_recovery_mode(self):
        return self._in_recovery

    def supports(self, info):
        return self._fwid is not None

    def get_info(self, info):
        return self._fwid


class FakeDevice:
    """Minimal stand-in for rspy.devices.Device."""
    def __init__(self, sn, name, product_line, in_recovery=False, fwid=None):
        self.serial_number = sn
        self.name = name
        self.product_line = product_line
        self.handle = FakeHandle(in_recovery, fwid)
        self.enabled = True


def connect(monkeypatch, *fake_devices):
    """Install fake devices as the enumeration result (rspy.devices state)."""
    monkeypatch.setattr(devices, '_device_by_sn', {d.serial_number: d for d in fake_devices})
    # observed_fwid() reads through devices.rs; a stub is enough (FakeHandle
    # ignores which camera_info it is handed)
    monkeypatch.setattr(devices, 'rs', types.SimpleNamespace(
        camera_info=types.SimpleNamespace(firmware_update_id='firmware_update_id')), raising=False)


MAP_YAML = """
nodes:
  bench1:
    cameras:
      - product: D455
        sn: "111"
        fwid: "111f"
      - product: D585S
        sn: "222"
        fwid: "222f"
      - product: D555
        sn: 333          # unquoted on purpose: parses as int, must normalize to str
        fwid: "333f"
        connection: DDS
  bench2:
    cameras:
      - product: D435
        sn: "444"        # no fwid on purpose: exercises the recovery heuristic
  bench3:
    cameras:
      - product: L515    # unknown product line, no fwid: the heuristic must not apply
        sn: "555"
  bench4:                # nothing under it: parses as None, must not crash load_map
"""


@pytest.fixture
def map_file(tmp_path):
    path = tmp_path / 'device-map.yaml'
    path.write_text(MAP_YAML)
    return str(path)


def run_check(monkeypatch, map_file, node='bench1', exclude_specs=None, *fake_devices):
    connect(monkeypatch, *fake_devices)
    return device_map.check(exclude_specs=exclude_specs, node=node, runner='pytest', map_file=map_file)


def states_by_product(report):
    return {c['product']: c['state'] for c in report['cameras']}


D455 = lambda **kw: FakeDevice('111', 'D455', 'D400', **kw)
D585S = lambda **kw: FakeDevice('222', 'D585S', 'D500', **kw)
D555 = lambda **kw: FakeDevice('333', 'D555', 'D500', **kw)


# =============================================================================
# Map loading / node resolution
# =============================================================================

class TestLoadAndResolve:

    def test_missing_file_returns_none(self, tmp_path):
        assert device_map.load_map(str(tmp_path / 'nope.yaml')) is None

    def test_no_nodes_section_returns_none(self, tmp_path):
        path = tmp_path / 'bad.yaml'
        path.write_text('something: else\n')
        assert device_map.load_map(str(path)) is None

    def test_serial_numbers_normalized_to_str(self, map_file):
        nodes = device_map.load_map(map_file)
        d555 = nodes['bench1']['cameras'][2]
        assert d555['sn'] == '333'  # was an unquoted int in the YAML

    def test_empty_node_entry_loads(self, map_file):
        nodes = device_map.load_map(map_file)
        assert 'bench4' in nodes and nodes['bench4'] is None

    def test_resolve_exact(self, map_file):
        nodes = device_map.load_map(map_file)
        assert len(device_map.resolve_node(nodes, 'bench1')['cameras']) == 3

    def test_resolve_case_insensitive(self, map_file):
        nodes = device_map.load_map(map_file)
        assert device_map.resolve_node(nodes, 'BENCH2') is nodes['bench2']

    def test_resolve_fqdn_matches_short_key(self, map_file):
        # Jenkins reports some machines as FQDN; map keys are short hostnames
        nodes = device_map.load_map(map_file)
        assert device_map.resolve_node(nodes, 'bench1.realsenseai.com') is nodes['bench1']

    def test_resolve_key_is_the_map_spelling(self, map_file):
        nodes = device_map.load_map(map_file)
        assert device_map.resolve_node_key(nodes, 'BENCH1.realsenseai.com') == 'bench1'

    def test_resolve_unknown_returns_none(self, map_file):
        nodes = device_map.load_map(map_file)
        assert device_map.resolve_node(nodes, 'stranger') is None


# =============================================================================
# Pre-exclusion spec matching (against map data, not enumeration)
# =============================================================================

class TestSpecMatchesCamera:
    CAMERA = {'product': 'D455', 'sn': '111', 'fwid': '111f'}

    def test_exact_product(self):
        assert device_map.spec_matches_camera('D455', self.CAMERA)

    def test_product_substring(self):
        assert device_map.spec_matches_camera('455', self.CAMERA)

    def test_serial(self):
        assert device_map.spec_matches_camera('111', self.CAMERA)

    def test_fwid(self):
        assert device_map.spec_matches_camera('111f', self.CAMERA)

    def test_product_line_wildcard(self):
        assert device_map.spec_matches_camera('D400*', self.CAMERA)

    def test_prefix_wildcard(self):
        assert device_map.spec_matches_camera('D45*', self.CAMERA)

    def test_no_match(self):
        assert not device_map.spec_matches_camera('D555', self.CAMERA)
        assert not device_map.spec_matches_camera('D500*', self.CAMERA)


# =============================================================================
# Classification
# =============================================================================

class TestCheck:

    def test_no_map_file(self, monkeypatch, tmp_path):
        report = run_check(monkeypatch, str(tmp_path / 'nope.yaml'), 'bench1', None, D455())
        assert report['status'] == device_map.STATUS_NO_MAP
        assert report['cameras'] == []

    def test_report_node_uses_map_spelling(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1.realsenseai.com', None, D455(), D585S(), D555())
        assert report['node'] == 'bench1'
        assert device_map.render_line(report).startswith('bench1: ')

    def test_empty_node_entry_is_no_map(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench4', None, D455())
        assert report['status'] == device_map.STATUS_NO_MAP

    def test_unknown_node(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'stranger', None, D455())
        assert report['status'] == device_map.STATUS_NO_MAP

    def test_all_present(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D585S(), D555())
        assert report['status'] == device_map.STATUS_OK
        assert set(states_by_product(report).values()) == {device_map.OK}
        assert report['node'] == 'bench1'
        assert report['runner'] == 'pytest'

    def test_one_missing(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D555())
        assert report['status'] == device_map.STATUS_DEGRADED
        assert states_by_product(report)['D585S'] == device_map.MISSING
        assert device_map.missing_products(report) == ['D585S']
        assert device_map.missing_serials(report) == ['222']

    def test_missing_but_pre_excluded(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', ['D585S'], D455(), D555())
        assert report['status'] == device_map.STATUS_OK
        d585 = next(c for c in report['cameras'] if c['product'] == 'D585S')
        assert d585['state'] == device_map.PRE_EXCLUDED
        assert d585['by'] == 'D585S'
        assert device_map.missing_products(report) == []

    def test_pre_excluded_by_wildcard(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', ['D500*'], D455())
        states = states_by_product(report)
        assert states['D585S'] == device_map.PRE_EXCLUDED
        assert states['D555'] == device_map.PRE_EXCLUDED
        assert states['D455'] == device_map.OK
        assert report['status'] == device_map.STATUS_OK

    def test_in_recovery_by_recovery_mode(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None,
                           D455(), D585S(), D555(in_recovery=True))
        assert states_by_product(report)['D555'] == device_map.IN_RECOVERY
        assert report['status'] == device_map.STATUS_OK  # in-recovery is not a failure here

    def test_in_recovery_by_fwid_key(self, monkeypatch, map_file):
        # A DFU device has no serial_number: it enumerates keyed by firmware_update_id
        dfu = FakeDevice('333f', 'D555 Recovery', 'D500', in_recovery=True)
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D585S(), dfu)
        assert states_by_product(report)['D555'] == device_map.IN_RECOVERY
        assert report['status'] == device_map.STATUS_OK

    def test_recovery_heuristic_without_fwid(self, monkeypatch, map_file):
        # bench2's D435 has no fwid in the map; an unmatched D400-line recovery
        # device is assumed to be it (unconfirmed), not missing
        dfu = FakeDevice('unknown-fwid', 'D435 Recovery', 'D400', in_recovery=True)
        report = run_check(monkeypatch, map_file, 'bench2', None, dfu)
        d435 = next(c for c in report['cameras'] if c['product'] == 'D435')
        assert d435['state'] == device_map.IN_RECOVERY
        assert 'unconfirmed' in d435['note']
        assert report['status'] == device_map.STATUS_OK

    def test_no_heuristic_for_unknown_product_line(self, monkeypatch, map_file):
        # bench3's L515 has no product line we know; an unknown-line recovery device
        # must not be matched to it (None == None), it stays missing
        dfu = FakeDevice('weird-fwid', 'Unknown Recovery', None, in_recovery=True)
        report = run_check(monkeypatch, map_file, 'bench3', None, dfu)
        assert states_by_product(report)['L515'] == device_map.MISSING
        assert report['status'] == device_map.STATUS_DEGRADED

    def test_no_heuristic_when_fwid_known(self, monkeypatch, map_file):
        # bench1 cameras all have fwid: an unrelated recovery device must NOT
        # rescue a missing camera whose exact DFU identity did not show up
        dfu = FakeDevice('unrelated-fwid', 'D585S Recovery', 'D500', in_recovery=True)
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D555(), dfu)
        assert states_by_product(report)['D585S'] == device_map.MISSING
        assert report['status'] == device_map.STATUS_DEGRADED

    def test_total_enumeration_failure(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None)  # nothing enumerated
        assert report['status'] == device_map.STATUS_TOTAL_ENUMERATION_FAILURE

    def test_all_pre_excluded_no_enumeration_is_ok(self, monkeypatch, map_file):
        # Everything intentionally excluded: an empty bench is not a failure
        report = run_check(monkeypatch, map_file, 'bench1', ['D400*', 'D500*'])
        assert report['status'] == device_map.STATUS_OK

    def test_unexpected_device_reported_not_failed(self, monkeypatch, map_file):
        rogue = FakeDevice('999', 'D415', 'D400')
        report = run_check(monkeypatch, map_file, 'bench1', None,
                           D455(), D585S(), D555(), rogue)
        assert report['status'] == device_map.STATUS_OK
        unexpected = [c for c in report['cameras'] if c['state'] == device_map.UNEXPECTED]
        assert len(unexpected) == 1
        assert unexpected[0]['sn'] == '999'

    def test_report_cached(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D585S(), D555())
        assert device_map.get_report() is report


# =============================================================================
# Output: render_line + health.json / health.txt
# =============================================================================

class TestOutput:

    def test_render_line_ok(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D585S(), D555())
        line = device_map.render_line(report)
        assert line.startswith('bench1: ')
        assert 'D455 ok' in line

    def test_render_line_missing_shouts(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D555())
        assert 'D585S MISSING' in device_map.render_line(report)

    def test_render_line_total_failure(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None)
        assert 'NO DEVICES ENUMERATED' in device_map.render_line(report)

    def test_render_line_no_map(self, monkeypatch, tmp_path):
        report = run_check(monkeypatch, str(tmp_path / 'nope.yaml'), 'bench1', None)
        assert 'no device map' in device_map.render_line(report)

    def test_write_health_json_and_txt(self, monkeypatch, map_file, tmp_path):
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D555())
        path = tmp_path / 'out' / 'health.json'
        device_map.write_health_json(report, str(path))

        with open(path) as f:
            loaded = json.load(f)
        assert loaded['status'] == device_map.STATUS_DEGRADED
        assert loaded['schema'] == device_map.SCHEMA_VERSION

        txt = (tmp_path / 'out' / 'health.txt').read_text()
        assert 'D585S MISSING' in txt


# =============================================================================
# firmware_update_id observation (fills in map entries that lack fwid)
# =============================================================================

class TestObservedFwid:

    def test_observed_when_map_has_none(self, monkeypatch, map_file):
        # bench2's D435 has no fwid in the map -- the run reports what the device says
        d435 = FakeDevice('444', 'D435', 'D400', fwid='444-asic')
        report = run_check(monkeypatch, map_file, 'bench2', None, d435)
        camera = report['cameras'][0]
        assert camera['fwid'] == '444-asic'
        assert camera['fwid_source'] == 'observed'

    def test_confirmed_when_map_matches(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None,
                           D455(fwid='111f'), D585S(fwid='222f'), D555(fwid='333f'))
        d455 = next(c for c in report['cameras'] if c['product'] == 'D455')
        assert d455['fwid'] == '111f'
        assert 'fwid_source' not in d455  # nothing to fix in the map

    def test_mismatch_is_flagged(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None,
                           D455(fwid='not-111f'), D585S(), D555())
        d455 = next(c for c in report['cameras'] if c['product'] == 'D455')
        assert d455['fwid'] == 'not-111f'
        assert 'map says 111f' in d455['fwid_source']
        assert report['status'] == device_map.STATUS_OK  # informational, not a failure

    def test_absent_when_device_does_not_expose_it(self, monkeypatch, map_file):
        report = run_check(monkeypatch, map_file, 'bench1', None, D455(), D585S(), D555())
        d455 = next(c for c in report['cameras'] if c['product'] == 'D455')
        assert 'fwid' not in d455
