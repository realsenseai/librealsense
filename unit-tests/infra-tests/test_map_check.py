# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for pytest-map-check.py and the harness pieces it relies on. No hardware.

- verdict() / unusable_products() / write_reports() on fabricated health reports
- the ``infra`` marker: collected under --live, skipped under --not-live
- a device() pattern excluded on the CLI skips instead of failing when absent
- --device-map is an accepted option
"""

import importlib.util
import json
import os

from helpers import run_e2e, assert_outcomes
from rspy import device_map

_UNIT_TESTS = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))
_spec = importlib.util.spec_from_file_location('pytest_map_check', os.path.join(_UNIT_TESTS, 'pytest-map-check.py'))
map_check = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(map_check)


def report(status, *cameras, node='bench1'):
    return {'schema': 1, 'node': node, 'runner': 'pytest', 'timestamp': 't',
            'status': status, 'cameras': list(cameras)}


OK = {'product': 'D455', 'sn': '111', 'state': device_map.OK}
MISSING = {'product': 'D435', 'sn': '222', 'state': device_map.MISSING}
DEGRADED = {'product': 'D555', 'sn': '333', 'state': device_map.DEGRADED, 'detail': '100Mbps'}
RECOVERY = {'product': 'D585S', 'sn': '444', 'state': device_map.IN_RECOVERY}
PRE_EXCLUDED = {'product': 'D401', 'sn': '555', 'state': device_map.PRE_EXCLUDED, 'by': 'D401'}
UNEXPECTED = {'product': 'D415', 'sn': '666', 'state': device_map.UNEXPECTED}


# =============================================================================
# verdict
# =============================================================================

class TestVerdict:

    def test_no_map_passes(self):
        assert map_check.verdict(report(device_map.STATUS_NO_MAP)) is None

    def test_all_ok_passes(self):
        assert map_check.verdict(report(device_map.STATUS_OK, OK, RECOVERY, PRE_EXCLUDED, UNEXPECTED)) is None

    def test_missing_fails_naming_camera_and_node(self):
        msg = map_check.verdict(report(device_map.STATUS_DEGRADED, OK, MISSING))
        assert msg.startswith('INFRA:')
        assert 'bench1' in msg
        assert 'D435 (SN 222) not detected' in msg
        assert 'D455' not in msg  # healthy cameras are not in the message

    def test_degraded_fails_with_detail(self):
        msg = map_check.verdict(report(device_map.STATUS_DEGRADED, DEGRADED))
        assert 'D555 (SN 333) degraded: 100Mbps' in msg

    def test_total_enumeration_failure(self):
        msg = map_check.verdict(report(device_map.STATUS_TOTAL_ENUMERATION_FAILURE, OK, MISSING))
        assert msg.startswith('INFRA: no devices enumerated on bench1')
        assert 'expected 2' in msg


# =============================================================================
# exclusion list + report files
# =============================================================================

class TestReports:

    def test_unusable_products_is_missing_plus_degraded(self):
        r = report(device_map.STATUS_DEGRADED, OK, MISSING, DEGRADED, RECOVERY, PRE_EXCLUDED, UNEXPECTED)
        assert map_check.unusable_products(r) == ['D435', 'D555']

    def test_write_reports(self, tmp_path):
        r = report(device_map.STATUS_DEGRADED, OK, MISSING, DEGRADED)
        map_check.write_reports(r, str(tmp_path))
        with open(tmp_path / 'health.json') as f:
            assert json.load(f)['status'] == device_map.STATUS_DEGRADED
        assert 'D435 MISSING' in (tmp_path / 'health.txt').read_text()
        assert (tmp_path / 'health-exclude.txt').read_text() == 'D435,D555'

    def test_html_line_colours_per_camera(self):
        html = map_check.render_html(report(device_map.STATUS_DEGRADED, OK, MISSING, DEGRADED, RECOVERY, PRE_EXCLUDED))
        assert html.startswith('bench1: ')
        assert '<span style="color:Green">D455 ok</span>' in html
        assert '<span style="color:Red"><b>D435 MISSING</b></span>' in html
        assert '<span style="color:Red"><b>D555 DEGRADED (100Mbps)</b></span>' in html
        assert '<span style="color:DarkOrange">D585S in-recovery</span>' in html
        assert '<span style="color:Gray">D401 pre-excluded</span>' in html
        assert html.count(' | ') == 4

    def test_html_total_failure_and_no_map(self):
        assert 'NO DEVICES ENUMERATED' in map_check.render_html(report(device_map.STATUS_TOTAL_ENUMERATION_FAILURE, OK))
        assert map_check.render_html(report(device_map.STATUS_NO_MAP)) == 'bench1: no device map'

    def test_write_reports_includes_html(self, tmp_path):
        map_check.write_reports(report(device_map.STATUS_DEGRADED, OK, MISSING), str(tmp_path))
        html = (tmp_path / 'health.html').read_text()
        assert 'color:Green">D455 ok' in html and 'color:Red"><b>D435 MISSING' in html

    def test_exclude_file_empty_when_healthy(self, tmp_path):
        map_check.write_reports(report(device_map.STATUS_OK, OK), str(tmp_path))
        assert (tmp_path / 'health-exclude.txt').read_text() == ''


# =============================================================================
# infra marker (e2e, real conftest)
# =============================================================================

class TestInfraMarker:

    def test_registered(self):
        rc, out, *_ = run_e2e("pytest-infra-marker.py", "-W", "error::pytest.PytestUnknownMarkWarning")
        assert_outcomes(out, passed=2)

    def test_live_keeps_infra_skips_plain(self):
        rc, out, *_ = run_e2e("pytest-infra-marker.py", "--live")
        assert_outcomes(out, passed=1, skipped=1)
        assert 'test_infra_runs_without_device PASSED' in out

    def test_not_live_skips_infra_keeps_plain(self):
        rc, out, *_ = run_e2e("pytest-infra-marker.py", "--not-live")
        assert_outcomes(out, passed=1, skipped=1)
        assert 'test_plain_no_device PASSED' in out


# =============================================================================
# absent device() pattern: MISSING unless excluded on the CLI
# =============================================================================

class TestExcludedMissing:

    def test_absent_is_a_failure(self):
        # a MISSING sentinel fails in module_device_setup, so pytest reports it as an error
        rc, out, *_ = run_e2e("pytest-excluded-missing.py")
        assert_outcomes(out, error=1)
        assert 'test_single_absent[MISSING-D555] ERROR' in out

    def test_excluded_by_name_skips(self):
        rc, out, *_ = run_e2e("pytest-excluded-missing.py", "--exclude-device", "D555")
        assert_outcomes(out, skipped=1)

    def test_excluded_by_product_line_skips(self):
        rc, out, *_ = run_e2e("pytest-excluded-missing.py", "--exclude-device", "D500*")
        assert_outcomes(out, skipped=1)

    def test_excluded_in_comma_list_skips(self):
        rc, out, *_ = run_e2e("pytest-excluded-missing.py", "--exclude-device", "D435,D555")
        assert_outcomes(out, skipped=1)

    def test_unrelated_exclusion_still_fails(self):
        rc, out, *_ = run_e2e("pytest-excluded-missing.py", "--exclude-device", "D455")
        assert_outcomes(out, error=1)


# =============================================================================
# --device-map option
# =============================================================================

class TestDeviceMapOption:

    def test_accepted(self, tmp_path):
        rc, out, *_ = run_e2e("pytest-passthrough.py", "--device-map", str(tmp_path / 'nope.yaml'))
        assert rc == 0
        assert_outcomes(out, passed=1)
