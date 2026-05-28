# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Regression tests for the pure functions in rspy.pytest.retry_aggregation.

Kept focused: each test guards a behaviour the e2e tests don't exercise directly
(canonical-id false-positives, mixed setup/call rescue paths, JUnit edge cases).
"""

import os
import sys
import tempfile
import types
import xml.etree.ElementTree as ET

import pytest

_unit_tests_dir = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))
_py_dir = os.path.join(_unit_tests_dir, 'py')
if _py_dir not in sys.path:
    sys.path.insert(0, _py_dir)

from rspy.pytest import retry_aggregation as ra


class TestCanonicalId:

    def test_valid_repeat_suffix_stripped(self):
        assert ra.canonical_id("test_y[D455-114222251278-1-2]") == "test_y[D455-114222251278]"

    def test_serial_tail_not_misinterpreted_as_repeat(self):
        # count=1 is not a valid pytest-repeat tail (need >= 2 attempts).
        assert ra.canonical_id("test_y[D455-7-1]") == "test_y[D455-7-1]"

    def test_step_gt_count_rejected(self):
        # step beyond count is impossible for pytest-repeat.
        assert ra.canonical_id("test_y[D455-5-2]") == "test_y[D455-5-2]"


def _mk_report(passed=False, failed=False):
    return types.SimpleNamespace(passed=passed, failed=failed)


class TestClassifyGroups:

    def setup_method(self):
        ra.reset()

    def test_rescued_drops_failed_attempt(self):
        f1, p2 = _mk_report(failed=True), _mk_report(passed=True)
        ra._call_reports['t1'] = [(1, f1), (2, p2)]
        rescued, hard_failed, _, fail_drop, err_drop = ra._classify_groups()
        assert rescued == ['t1'] and hard_failed == []
        assert fail_drop == [f1] and err_drop == []

    def test_setup_error_rescued_by_call_pass(self):
        e1, p2 = _mk_report(failed=True), _mk_report(passed=True)
        ra._call_reports['t1'] = [(2, p2)]
        ra._error_reports['t1'] = [(1, e1)]
        rescued, hard_failed, _, fail_drop, err_drop = ra._classify_groups()
        assert rescued == ['t1'] and hard_failed == []
        assert err_drop == [e1] and fail_drop == []


def _write_xml(content):
    fd, path = tempfile.mkstemp(suffix='.xml')
    os.close(fd)
    with open(path, 'w') as f:
        f.write(content)
    return path


class TestRewriteJunitXml:

    def test_rescued_failure_becomes_clean_passed_with_summed_time(self):
        xml = """<?xml version='1.0' encoding='utf-8'?>
<testsuites><testsuite name="pytest" tests="2" failures="1" errors="0" skipped="0" time="2.0">
  <testcase classname="m" name="test_x[D455-1-2]" time="0.1">
    <failure message="boom">boom</failure>
  </testcase>
  <testcase classname="m" name="test_x[D455-2-2]" time="0.2" />
</testsuite></testsuites>"""
        path = _write_xml(xml)
        try:
            ra.rewrite_junit_xml(path)
            ts = ET.parse(path).getroot().find('testsuite')
            tcs = ts.findall('testcase')
            assert len(tcs) == 1
            assert tcs[0].get('name') == 'test_x[D455]'
            assert tcs[0].findall('failure') == []
            assert ts.get('failures') == '0' and ts.get('tests') == '1'
            assert float(tcs[0].get('time')) == pytest.approx(0.3)
        finally:
            os.unlink(path)

    def test_skipped_in_middle_not_treated_as_pass(self):
        """FAIL -> SKIPPED -> PASS: the kept entry must be the PASS, not SKIPPED."""
        xml = """<?xml version='1.0' encoding='utf-8'?>
<testsuites><testsuite name="pytest" tests="3" failures="1" errors="0" skipped="1" time="3.0">
  <testcase classname="m" name="test_x[D455-1-3]" time="0.1">
    <failure message="boom">boom</failure>
  </testcase>
  <testcase classname="m" name="test_x[D455-2-3]" time="0.05">
    <skipped message="conditional skip" />
  </testcase>
  <testcase classname="m" name="test_x[D455-3-3]" time="0.2" />
</testsuite></testsuites>"""
        path = _write_xml(xml)
        try:
            ra.rewrite_junit_xml(path)
            tcs = ET.parse(path).getroot().find('testsuite').findall('testcase')
            assert len(tcs) == 1
            assert tcs[0].findall('failure') == []
            assert tcs[0].findall('skipped') == []
        finally:
            os.unlink(path)
