# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Shared helpers for infra regression tests — fake devices, mock builders, E2E runner."""

import json
import os
import re
import subprocess
import sys
import types
from unittest.mock import MagicMock
import pytest


# =============================================================================
# Fake device inventory
# =============================================================================
#
#   name   serial  product_line
#   D455   111     D400
#   D435   222     D400
#   D435i  333     D400
#   D405   444     D400
#   D401   777     D400
#   D457   888     D400
#   D515   555     D500
#   D555   666     D500
#
# The E2E conftest uses a subset (D455, D435, D515, D401) — enough to
# test wildcards, excludes, and multi-device parametrization without noise.

DEVICES = {
    'D455':  ('111', 'D400'),
    'D435':  ('222', 'D400'),
    'D435i': ('333', 'D400'),
    'D405':  ('444', 'D400'),
    'D401':  ('777', 'D400'),
    'D457':  ('888', 'D400'),
    'D515':  ('555', 'D500'),
    'D555':  ('666', 'D500'),
}

SN_TO_NAME = {sn: name for name, (sn, _) in DEVICES.items()}


class FakeDevice:
    """Minimal stand-in for rspy.devices.Device."""
    def __init__(self, sn, name):
        self.sn = sn
        self.name = name


def fake_by_spec(pattern, ignored):
    """Mock devices.by_spec against the DEVICES inventory."""
    if pattern.endswith('*'):
        product_line = pattern[:-1]
        for name, (sn, pl) in DEVICES.items():
            if pl == product_line:
                yield sn
    elif pattern in DEVICES:
        yield DEVICES[pattern][0]
    elif pattern in SN_TO_NAME:
        yield pattern


def fake_get(sn):
    """Mock devices.get against the DEVICES inventory."""
    name = SN_TO_NAME.get(sn)
    return FakeDevice(sn, name) if name else None


# =============================================================================
# Mock builders for unit tests
# =============================================================================

def make_mock_item(name="test_example", markers=None, module_name="fake_module",
                   device_serial=None):
    """Build a fake pytest Item for unit-testing collection/filter logic.

    Uses SimpleNamespace so that `hasattr(item, 'callspec')` is genuinely False
    when no device_serial is set (MagicMock auto-creates attributes on access).
    """
    marks = list(markers or [])

    item = types.SimpleNamespace()
    item.name = name
    item.module = types.ModuleType(module_name)
    item.add_marker = MagicMock()

    if device_serial:
        item.callspec = types.SimpleNamespace(
            params={'_test_device_serial': device_serial}
        )

    def iter_markers(match_name=None):
        for m in marks:
            if match_name is None or m.name == match_name:
                yield m

    item.iter_markers = iter_markers
    item.get_closest_marker = lambda n: next(
        (m for m in marks if m.name == n), None
    )
    return item


def make_mock_config(context="", live=False, not_live=False, markexpr=""):
    """Build a mock pytest Config for unit-testing collection/filter logic."""
    config = MagicMock()
    opts = {"--context": context, "--live": live, "--not-live": not_live, "-m": markexpr}
    config.getoption = lambda key, default=None: opts.get(key, default)
    return config


def make_device_marker(name, *patterns):
    """Build a mock marker (device/device_each/device_exclude) for find_matching_devices tests."""
    m = MagicMock()
    m.name = name
    m.args = patterns
    return m


# =============================================================================
# E2E subprocess runner
# =============================================================================

_E2E_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'e2e')
_E2E_CONFTEST = os.path.join(_E2E_DIR, 'e2e_conftest.py')


def run_e2e(test_filename, *extra_pytest_args):
    """Run a pytest subprocess on a static test file from e2e/.

    Copies e2e_conftest.py and the test file to a temp dir for isolation
    from the parent unit-tests/conftest.py. No content is generated — both
    files are static and checked into the repo.

    Returns (returncode, stdout, tracking) where tracking is a dict with:
        - enable_only_calls: list of {serials, recycle} dicts
        - rslog_calls: list of {level} dicts
        - query_kwargs: list of kwargs dicts passed to devices.query()
    """
    import shutil, tempfile

    with tempfile.TemporaryDirectory() as tmpdir:
        shutil.copy(_E2E_CONFTEST, os.path.join(tmpdir, 'conftest.py'))
        shutil.copy(os.path.join(_E2E_DIR, test_filename), os.path.join(tmpdir, test_filename))

        env = os.environ.copy()
        env['INFRA_UNIT_TESTS_DIR'] = os.path.normpath(os.path.join(_E2E_DIR, '..', '..'))  # unit-tests/

        p = subprocess.run(
            [sys.executable, "-m", "pytest", test_filename, "-v", *extra_pytest_args],
            cwd=tmpdir,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            timeout=30,
        )

        if p.returncode != 0:
            out_lower = p.stdout.lower()
            if not any(kw in out_lower for kw in ('no tests ran', 'passed', 'skipped', 'error', 'failed')):
                pytest.fail(f"Subprocess crashed (rc={p.returncode}):\n{p.stdout}")

        tracking_file = os.path.join(tmpdir, '_tracking.json')
        tracking = json.loads(open(tracking_file).read()) if os.path.exists(tracking_file) else {
            "enable_only_calls": [], "rslog_calls": [], "query_kwargs": []
        }

        return p.returncode, p.stdout, tracking


def parse_outcomes(stdout):
    """Parse pytest summary line into a dict like {'passed': 3, 'skipped': 1}."""
    matches = re.findall(r'=+ (.+?) =+\s*$', stdout, re.MULTILINE)
    if not matches:
        return {}
    summary = matches[-1]
    outcomes = {}
    for match in re.finditer(r'(\d+) (\w+)', summary):
        outcomes[match.group(2)] = int(match.group(1))
    return outcomes


def assert_outcomes(stdout, **expected):
    """Assert pytest outcomes from subprocess stdout."""
    actual = parse_outcomes(stdout)
    for key, val in expected.items():
        assert actual.get(key, 0) == val, \
            f"Expected {key}={val}, got {actual.get(key, 0)}. Full output:\n{stdout}"
