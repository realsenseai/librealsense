# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import subprocess
import logging
import pytest
from rspy import repo

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
