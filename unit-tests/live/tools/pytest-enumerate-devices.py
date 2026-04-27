# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

import subprocess
import logging
import pytest
from rspy import repo
from rspy.snippets import is_dds_dev
from rspy.stopwatch import Stopwatch

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
]


def test_rs_enumerate_devices(test_device):
    dev, _ = test_device
    rs_enumerate_devices = repo.find_built_exe('tools/enumerate-devices', 'rs-enumerate-devices')
    assert rs_enumerate_devices, "rs-enumerate-devices not found"

    is_dds = is_dds_dev(dev)
    run_time_threshold = 5 if is_dds else 2  # currently, DDS devices take longer time to complete rs_enumerate_devices
    cmd = [rs_enumerate_devices]
    if not is_dds:
        cmd.append("--no-dds")
    run_time_stopwatch = Stopwatch()
    p = subprocess.run(
        cmd,
        stdout=None,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        timeout=10,
        check=False,
    )
    run_time_seconds = run_time_stopwatch.get_elapsed()
    assert p.returncode == 0
    log.debug("rs-enumerate-devices completed in: %s seconds", run_time_seconds)
    assert run_time_seconds < run_time_threshold, \
        f"Time elapsed too high! {run_time_seconds} > {run_time_threshold}"
