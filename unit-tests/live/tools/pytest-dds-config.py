# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import subprocess
import logging
import json
import os
import pytest
from rspy import repo, config_file

log = logging.getLogger(__name__)

pytestmark = [
    # device_each (not device) so hosts without a D555 skip silently
    pytest.mark.device_each("D555"),
    pytest.mark.context("nightly"),
]


def test_rs_dds_config_runs(module_device_setup):
    exe_path = repo.find_built_exe('tools/dds/dds-config', 'rs-dds-config')
    assert exe_path, "rs-dds-config not found"

    # No args: enables DDS by default and checks if it can connect to a supporting device
    p = subprocess.run(
        [exe_path],
        stdout=None,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        timeout=10,
        check=False,
    )
    assert p.returncode == 0


def _config_domain():
    """Return the SDK domain currently persisted in realsense-config.json, or None."""
    path = config_file.get_config_path()
    if not os.path.exists( path ):
        return None
    with open( path, "r", encoding="utf-8" ) as f:
        config = json.load( f )
    # .get() chain so a config that lacks any of these keys returns None instead of raising
    return config.get( "context", {} ).get( "dds", {} ).get( "domain" )


def test_transient_sdk_domain_id_does_not_persist(module_device_setup):
    """--transient-sdk-domain-id must apply only to the current run and never change the config file."""
    exe_path = repo.find_built_exe('tools/dds/dds-config', 'rs-dds-config')
    assert exe_path, "rs-dds-config not found"

    before = _config_domain()
    # Pick a domain different from the persisted one so a regression (accidental write) is visible.
    domain = 1 if before != 1 else 2
    # --no-reset => no device change; a domain with no device may exit non-zero, which is fine:
    # the point of this test is purely that the config file is left untouched.
    subprocess.run(
        [exe_path, '--transient-sdk-domain-id', str(domain), '--no-reset'],
        stdout=None,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        timeout=15,
        check=False,
    )
    after = _config_domain()
    assert after == before, f"--transient-sdk-domain-id must not persist (config domain {before} -> {after})"
